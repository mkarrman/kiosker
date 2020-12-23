/*
 * Kiosker - Kiosk mode web browser with UDS command interface
 * Copyright (C) 2021  Mats Karrman  <mats.dev.list.at.gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 
#include <errno.h>
#include <getopt.h>
#include <glib.h>
#include "glib-unix.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <systemd/sd-daemon.h>
#include <unistd.h>
#include <webkit2/webkit2.h>

#include "common.h"


#define DEFAULT_URI        "http://localhost/"
#define VIEW_BG_COLOR      COL_OPAQUE_DARKCYAN

#define COL_OPAQUE_DARKCYAN   { 0.0, 0.1, 0.1, 1.0 }
#define COL_OPAQUE_BLACK      { 0.0, 0.0, 0.0, 1.0 }


static struct {
	const char *sock_addr;
	const char *start_uri;
} cmd_args;

static int conn_sock_fd = -1;
static const char *conn_sock_path;

static GtkWidget *main_window;
static WebKitWebView *web_view;
static WebKitWebContext *web_context;
static GdkWindow *gdk_window;


static void print_help(FILE *file)
{
	fprintf(file,
	"Usage: kiosker [OPTS]\n"
	"   -h --help           Show this help text and exit.\n"
	"   -a --address ADDR   Set server socket path to ADDR.\n"
	"   -u --start-uri URI  Use <URI> as start page.\n"
	"NOTE: kiosker will check with systemd if a socket has been passed.\n"
	"      If not, the specified address will be used or the default\n"
	"      address \"" DEFAULT_SOCK_PATH "\".\n"
	"      The default start URI is \"" DEFAULT_URI "\".\n"
	);
}

static int parse_command_line(int argc, char *argv[])
{
	const char s_opts[] = "a:hu:";
	const struct option l_opts[] = {
		{ "address",   required_argument, NULL, 'a' },
		{ "help",      no_argument,       NULL, 'h' },
		{ "start-uri", required_argument, NULL, 'u' },
		{ NULL, 0, NULL, 0 }
	};
	int c;
	int ret;

	cmd_args.sock_addr = DEFAULT_SOCK_PATH;
	cmd_args.start_uri = DEFAULT_URI;

	while ((c = getopt_long(argc, argv, s_opts, l_opts, NULL)) != -1) {
		switch (c) {
		case 'a':
			cmd_args.sock_addr = optarg;
			break;
		case 'h':
			fprintf(stdout,
			        "kiosker - Minimal kiosk mode web browser\n");
			print_help(stdout);
			return EXIT_FAILURE;
		case 'u':
			cmd_args.start_uri = optarg;
			break;
		default:
			fprintf(stderr, "Illegal argument!\n");
			print_help(stderr);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

static int setup_command_socket(void)
{
	/* check for listening socket passed in by systemd */
	if (sd_listen_fds(1) > 0) {

		conn_sock_fd = SD_LISTEN_FDS_START;
		if (!sd_is_socket(conn_sock_fd, AF_UNIX, SOCK_DGRAM, -1)) {
			fprintf(stderr, "Invalid listen socket passed\n");
			return EXIT_FAILURE;
		}

		/* systemd responsible for removing socket path */

	} else {

		/* open socket manually */
		struct sockaddr_un name;

		if (unlink(cmd_args.sock_addr) < 0 && errno != ENOENT) {
			fprintf(stderr, "Failed to unlink socket '%s' (%d)\n",
			        cmd_args.sock_addr, errno);
			return EXIT_FAILURE;
		}

		conn_sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
		if (conn_sock_fd < 0) {
			fprintf(stderr, "Failed to create socket (%d)\n", errno);
			return EXIT_FAILURE;
		}
		
		name.sun_family = AF_UNIX;
		strcpy(name.sun_path, cmd_args.sock_addr);
		if (bind(conn_sock_fd, (struct sockaddr *)&name, sizeof(name))) {
			fprintf(stderr, "Failed to bind socket to '%s' (%d)\n",
			        cmd_args.sock_addr, errno);
			return EXIT_FAILURE;
		}
		
		conn_sock_path = cmd_args.sock_addr;
	}

	return EXIT_SUCCESS;
}

static void shutdown_command_socket(void)
{
	if (conn_sock_fd > 0)
		close(conn_sock_fd);

	if (conn_sock_path)
		unlink(conn_sock_path);
}

static void destroy_window_cb(GtkWidget* widget, GtkWidget* window)
{
	gtk_main_quit();
}

static gboolean close_webview_cb(WebKitWebView* webView, GtkWidget* window)
{
	gtk_widget_destroy(window);
	return TRUE;
}

static int setup_gtk_webkit(void)
{
	static const GdkRGBA bg_color = VIEW_BG_COLOR;

	main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_fullscreen(GTK_WINDOW(main_window));

	web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
	//webkit_web_view_set_background_color(web_view, &bg_color);   still painted white first...
	gtk_container_add(GTK_CONTAINER(main_window), GTK_WIDGET(web_view));

	/* handlers for window/widget close */
	g_signal_connect(main_window, "destroy",
	                 G_CALLBACK(destroy_window_cb), NULL);
	g_signal_connect(web_view, "close",
	                 G_CALLBACK(close_webview_cb), main_window);

	web_context = webkit_web_context_get_default();
	webkit_web_context_clear_cache(web_context);
	webkit_web_context_set_cache_model(web_context,
	                                   WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);

	webkit_web_view_load_uri(web_view, cmd_args.start_uri);
	gtk_widget_grab_focus(GTK_WIDGET(web_view));
	gtk_widget_show_all(main_window);

	//gdk_window = gtk_widget_get_window(main_window);
	//fprintf(stderr, "gdk_window = %p, %p\n", gdk_window, GDK_BLANK_CURSOR);
	//gdk_window_set_cursor(gdk_window, GDK_BLANK_CURSOR);   //SEGV!!!

	return EXIT_SUCCESS;
}

static void shutdown_gtk_webkit(void)
{
	/* rely on automatic garbage collection */
}

static gboolean command_fd_source_cb(gpointer user_data)
{
	static char rx_buffer[1024];
	char *ptr;
	ssize_t count;

	count = recv(conn_sock_fd, rx_buffer, sizeof(rx_buffer) - 1, 0);
	if (count < 0) {
		if (errno == EINTR)
			goto out;
		g_warning("Command recv failed (%d)\n", errno);
		goto out;
	}

	if (count == 0) {
		g_warning("Command recv empty\n");
		goto out;
	}

	rx_buffer[count] = '\0';
	ptr = strchr(rx_buffer, '\n');
	if (!ptr) {
		g_warning("Command recv without '\\n': '%s'\n", rx_buffer);
		goto out;
	}

	*ptr = '\0';
	if (!memcmp(rx_buffer, "URI=", 4)) {

		ptr = &rx_buffer[4];
		g_message("Command URI '%s'", ptr);
		webkit_web_view_load_uri(web_view, ptr);

	} else if (!memcmp(rx_buffer, "QUIT", 5)) {

		g_message("Command QUIT");
		gtk_main_quit();

	} else {

		g_warning("Command dropped: '%s'\n", rx_buffer);
	}

  out:
	return G_SOURCE_CONTINUE;
}

static int setup_command_source(void)
{
	GSource *gsrc;
	int gsrc_id;

	gsrc = g_unix_fd_source_new(conn_sock_fd, G_IO_IN | G_IO_PRI);
	if (!gsrc)
		return EXIT_FAILURE;

	g_source_set_callback(gsrc, command_fd_source_cb, NULL, NULL);
	gsrc_id = g_source_attach(gsrc, NULL);

	return EXIT_SUCCESS;
}

static void shutdown_command_source(void)
{
	/* rely on automatic garbage collection */
}

int main(int argc, char* argv[])
{
	int exit_code = EXIT_FAILURE;

	/* check for gtk cmdline options and remove them from argc/argv */
	if (!gtk_init_check(&argc, &argv)) {
		fprintf(stderr, "GTK failed to initialize windowing system.\n");
		goto out;
	}

	if (parse_command_line(argc, argv))
		goto out;
	if (setup_command_socket())
		goto out;
	if (setup_gtk_webkit())
		goto out;
	if (setup_command_source())
		goto out;

	gtk_main();
	exit_code = EXIT_SUCCESS;

  out:
	shutdown_command_source();
	shutdown_gtk_webkit();
	shutdown_command_socket();
	
	return EXIT_SUCCESS;
}
