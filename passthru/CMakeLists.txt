add_executable(vanilla-passthru
	main.c
)

target_include_directories(vanilla-passthru PRIVATE
	${CMAKE_SOURCE_DIR}/lib
)

install(TARGETS vanilla-passthru)
