add_library(udhcpc STATIC
    common.c
    options.c
    packet.c
    pidfile.c
    signalpipe.c
    socket.c
    dhcpc.c
    clientpacket.c
    clientsocket.c
    script.c
)

target_compile_definitions(udhcpc PRIVATE _GNU_SOURCE)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(udhcpc PRIVATE UDHCP_DEBUG)
endif()
