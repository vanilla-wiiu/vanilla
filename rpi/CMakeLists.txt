find_package(SDL2 REQUIRED)
#find_package(SDL2_ttf REQUIRED)
find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil avfilter)

add_executable(vanilla-pi
    main.c
)

target_include_directories(vanilla-pi PRIVATE
    "${CMAKE_SOURCE_DIR}/lib"
)

target_link_libraries(vanilla-pi PRIVATE
    SDL2::SDL2
#    SDL2_ttf::SDL2_ttf
    vanilla
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avutil
)
