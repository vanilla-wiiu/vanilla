add_executable(caramel
	main.c
)

target_link_libraries(caramel PRIVATE
	vanilla
	m
)
