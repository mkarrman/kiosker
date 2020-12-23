set(KIOSKCMD_SOURCES
	kioskcmd.c
)

set(KIOSKCMD_LIBRARIES
	${CMAKE_DL_LIBS}
)

add_executable(kioskcmd ${KIOSKCMD_SOURCES})

target_link_libraries(kioskcmd PRIVATE
	${KIOSKCMD_LIBRARIES}
)

