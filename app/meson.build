executable(
    'vanilla-gui',
    'audiohandler.cpp',
    'backend.cpp',
    'backendinitdialog.cpp',
    'gamepadhandler.cpp',
    'inputconfigdialog.cpp',
    'keymap.cpp',
    'main.cpp',
    'mainwindow.cpp',
    'syncdialog.cpp',
    'syncprogressdialog.cpp',
    'udpaddressdialog.cpp',
    'videodecoder.cpp',
    'viewer.cpp',
    import('qt6').compile_moc(
        headers: [
            'audiohandler.h',
            'backend.h',
            'backendinitdialog.h',
            'gamepadhandler.h',
            'inputconfigdialog.h',
            'mainwindow.h',
            'syncdialog.h',
            'syncprogressdialog.h',
            'udpaddressdialog.h',
            'videodecoder.h',
            'viewer.h',
        ],
    ),
    import('qt6').compile_resources(sources: 'resource.qrc'),
    dependencies: [
        dependency('libavcodec'),
        dependency('libavfilter'),
        dependency('libavformat'),
        dependency('libavutil'),
        libvanilla,
        dependency(
            'qt6',
            modules: [
                'Concurrent',
                'Core',
                'Multimedia',
                'Network',
                'OpenGLWidgets',
                'Widgets',
            ],
        ),
        dependency('sdl2'),
    ],
    install: true,
)

install_data(
    'com.mattkc.vanilla.svg',
    install_dir: 'share/icons/hicolor/scalable/apps',
)
install_data('com.mattkc.vanilla.desktop', install_dir: 'share/applications')
