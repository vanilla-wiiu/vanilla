project(
    'Vanilla',
    'c',
    'cpp',
    default_options: {'warning_level': '3'},
    license: 'GPL-2.0-only',
    license_files: 'LICENSE',
    meson_version: '>=1.2.0',
)

subdir('lib')
if host_machine.system() != 'android'
    subdir('pipe')
endif
subdir('app')
