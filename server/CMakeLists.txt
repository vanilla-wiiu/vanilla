add_library(vanillaserver SHARED
    server.c
)

target_include_directories(vanillaserver PRIVATE
    /home/matt/src/drc-hostap/src/common
    /home/matt/src/drc-hostap/src/utils
)

target_link_directories(vanillaserver PRIVATE
    /home/matt/src/drc-hostap/wpa_supplicant
)

target_link_libraries(vanillaserver PRIVATE
    wpa_client
)