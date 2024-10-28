libvanilla = declare_dependency(
    include_directories: '.',
    link_with: static_library(
        'vanilla',
        'gamepad/audio.c',
        'gamepad/command.c',
        'gamepad/gamepad.c',
        'gamepad/input.c',
        'gamepad/video.c',
        'status.c',
        'util.c',
        'vanilla.c',
        dependencies: dependency('threads'),
        install: true,
    ),
)
