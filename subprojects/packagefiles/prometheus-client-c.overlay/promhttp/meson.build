cc = meson.get_compiler('c')
libmicrohttpd_dep = cc.find_library('microhttpd')

inc_dirs = include_directories('include')
libpromhttp = shared_library(
    'promhttp',
    [
        'src/promhttp.c',
    ],
    c_args: [
        '-std=gnu11',
        '-Werror',
        '-Wall',
        '-Wuninitialized',
        '-Wno-error=unused-parameter',
        '-Wno-error=incompatible-pointer-types',
    ],
    dependencies: [
        libmicrohttpd_dep,
        libprom_dep,
    ],
    include_directories: [
        inc_dirs,
    ],
    install: true,
    soversion: 1,
)

inc_subdir = 'promhttp'
install_headers(
    [
        'include/promhttp.h',
    ],
    subdir: inc_subdir,
)

pkgcfg = import('pkgconfig')
pkgcfg.generate(
    libpromhttp,
    subdirs: [
        inc_subdir,
    ],
)

libpromhttp_dep = declare_dependency(
    include_directories: [
        inc_dirs,
    ],
    link_with: [
        libpromhttp,
    ],
    dependencies: [
        libmicrohttpd_dep,
    ],
)
