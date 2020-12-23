pkg_check_modules(GLIB_PKG REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(GTK_PKG REQUIRED IMPORTED_TARGET gtk+-3.0)
pkg_check_modules(LIBSYSTEMD_PKG REQUIRED IMPORTED_TARGET libsystemd)
pkg_check_modules(WEBKITGTK_PKG REQUIRED IMPORTED_TARGET webkit2gtk-4.0)

set(KIOSKER_SOURCES
	kiosker.c
)

set(KIOSKER_LIBRARIES
	PkgConfig::GLIB_PKG
	PkgConfig::GTK_PKG
	PkgConfig::LIBSYSTEMD_PKG
	PkgConfig::WEBKITGTK_PKG
	${CMAKE_DL_LIBS}
)

add_executable(kiosker ${KIOSKER_SOURCES})

target_link_libraries(kiosker PRIVATE
	${KIOSKER_LIBRARIES}
)

