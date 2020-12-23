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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <unistd.h>

#include "common.h"


static struct {
	const char *sock_addr;
} cmd_args;


static int send_cmd(int sock_fd, const char *tag, const char *data)
{
	struct sockaddr_un sock_addr;
	struct msghdr mh;
	struct iovec iov[3];
	char end[2] = "\n";
	int ret = EXIT_FAILURE;

	memset(iov, 0, sizeof(iov));
	memset(&mh, 0, sizeof(mh));
	
	sock_addr.sun_family = AF_UNIX;
	strcpy(sock_addr.sun_path, cmd_args.sock_addr);

	mh.msg_name = &sock_addr;
	mh.msg_namelen = sizeof(sock_addr);
	mh.msg_iov = iov;
	mh.msg_iovlen = 3;

	iov[0].iov_base = strdup(tag);
	iov[0].iov_len = strlen(tag);
	if (data) {
		iov[1].iov_base = strdup(data);
		iov[1].iov_len = strlen(data);
	}
	iov[2].iov_base = end;
	iov[2].iov_len = 1;

	if (!iov[0].iov_base || (data && !iov[1].iov_base))
		fprintf(stderr, "Memory allocation failed\n");
	else if (sendmsg(sock_fd, &mh, 0) < 0)
		fprintf(stderr, "Sending message failed (%s)\n",
		        strerror(errno));
	else
		ret = EXIT_SUCCESS;

	if (data)
		free(iov[1].iov_base);
	free(iov[0].iov_base);

	return ret;
}

static void print_help(FILE *file)
{
	fprintf(file,
	"Usage: kioskcmd [OPTS]\n"
	"   -h --help          Show this help text and exit.\n"
	"   -a --address ADDR  Set socket path to ADDR.\n"
	"   -q --quit          Send a QUIT request to kiosker.\n"
	"   -u --uri URI       Send an URI=<URI> request to kiosker.\n"
	);
}

int main(int argc, char *argv[])
{
	const char s_opts[] = "a:hqu:";
	const struct option l_opts[] = {
		{ "address", required_argument, NULL, 'a' },
		{ "help",    no_argument,       NULL, 'h' },
		{ "quit",    no_argument,       NULL, 'q' },
		{ "uri",     required_argument, NULL, 'u' },
		{ NULL, 0, NULL, 0 }
	};
	const char *data = NULL;
	int cmd = 0;
	int c;
	int sock_fd;
	int ret;

	cmd_args.sock_addr = DEFAULT_SOCK_PATH;

	while ((c = getopt_long(argc, argv, s_opts, l_opts, NULL)) != -1) {
		switch (c) {
		case 'a':
			cmd_args.sock_addr = optarg;
			break;
		case 'h':
			fprintf(stdout,
			        "kioskcmd - "
			        "Command client for kiosker web browser\n");
			print_help(stdout);
			return EXIT_SUCCESS;
		case 'q':
			cmd = c;
			break;
		case 'u':
			data = optarg;
			cmd = c;
			break;
		default:
			fprintf(stderr, "Illegal argument!\n");
			print_help(stderr);
			return EXIT_FAILURE;
		}
	}

	sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock_fd < 0) {
		fprintf(stderr, "Failed to create socket (%s)\n",
		        strerror(errno));
		return EXIT_FAILURE;
	}

	switch (cmd) {
	case 'q':
		ret = send_cmd(sock_fd, "QUIT", NULL);
		break;
	case 'u':
		ret = send_cmd(sock_fd, "URI=", data);
		break;
	default:
		ret = EXIT_SUCCESS;
		break;
	}

	close(sock_fd);
	return ret;
}
