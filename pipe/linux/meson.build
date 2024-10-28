copy_hostap = custom_target(
    command: [
        'cp',
        '-frT',
        meson.current_source_dir() / 'hostap',
        '@OUTPUT@',
    ],
    output: 'hostap'
)

wpa_supplicant_dir = copy_hostap.full_path() / 'wpa_supplicant'

configure_hostap = custom_target(
    command: [
        'cp',
        '-fT',
        '@INPUT@',
        wpa_supplicant_dir / '.config',
        '&&',
        find_program('touch'),
        '@OUTPUT@',
    ],
    depends: copy_hostap,
    input: 'hostap/conf/wpa_supplicant.config',
    output: 'hostap_configured',
)

libwpa_client = custom_target(
    build_always_stale: true,
    command: [
        'make',
        '-C',
        wpa_supplicant_dir,
        '-j',
        'libwpa_client.a',
        '&&',
        find_program('cp'),
        '-fT',
        wpa_supplicant_dir / 'libwpa_client.a',
        '@OUTPUT@',
    ],
    console: true,
    depends: [configure_hostap, copy_hostap],
    output: 'libwpa_client.a',
)

custom_target(
    build_always_stale: true,
    command: [
        'make',
        '-C',
        wpa_supplicant_dir,
        '-j',
        'wpa_supplicant',
        '&&',
        find_program('cp'),
        '-fT',
        wpa_supplicant_dir / 'wpa_supplicant',
        '@OUTPUT@',
    ],
    console: true,
    # depend on libwpa_client so wpa_supplicant is not built at the same time
    depends: [configure_hostap, copy_hostap, libwpa_client],
    install: true,
    install_dir: get_option('bindir'),
    output: 'wpa_supplicant_drc',
)

executable(
    'vanilla-pipe',
    'main.c',
    'wpa.c',
    libwpa_client,
    dependencies: [libvanilla, dependency('threads')],
    include_directories: ['hostap/src/common', 'hostap/src/utils'],
    install: true,
)
