project('esnet-smartnic', 'c', default_options : ['warning_level=2', 'werror=true'])

meson.override_find_program('regio-generate', find_program('regio/regio-generate'))
meson.override_find_program('regio-elaborate', find_program('regio/regio-elaborate'))
meson.override_find_program('regio-info', find_program('regio/regio-info'))

subproject('esnet-smartnic-hwapi', default_options: ['default_library=static'])
subdir('libsnutil')
subdir('libsdnetapi')
subdir('libopennic')
subdir('sn-cli')
