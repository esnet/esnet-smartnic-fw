sources = files(
  [
    'src/sdnetapi.c',
    'src/sdnetconfig.c',
    'src/sdnetinfo.c',
    'src/sdnetio.c',
  ]
)

ext_incdir = include_directories('include')
int_incdir = include_directories('src/include')

cc = meson.get_compiler('c')
libgmp_dep = cc.find_library('gmp')

libsdnetapi = shared_library(
  'sdnetapi',
  sources,
  dependencies : [
    libgmp_dep,
    dependency('libsdnetdrv'),
    libsnutil_dep,
  ],
  include_directories : [
    ext_incdir,
    int_incdir,
  ],
  install : true,
  soversion : 1,
)

install_headers(
  [
    'include/sdnetapi.h',
  ],
  subdir : 'esnet-smartnic'
)

pkg = import('pkgconfig')
pkg.generate(
  libsdnetapi,
  subdirs : 'esnet-smartnic',
)

libsdnetapi_dep = declare_dependency(
  include_directories : [ext_incdir],
  link_with : [
    libsdnetapi,
    libsnutil,
  ])
