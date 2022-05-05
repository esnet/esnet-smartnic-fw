sources = files(
  [
    'src/snp4_api.c',
    'src/snp4_config.c',
    'src/snp4_info.c',
    'src/snp4_io.c',
    'src/snp4_p4bm_file.c',
    'src/snp4_table.c',
  ]
)

ext_incdir = include_directories('include')
int_incdir = include_directories('src/include')

cc = meson.get_compiler('c')
libgmp_dep = cc.find_library('gmp')

libsnp4 = shared_library(
  'snp4',
  sources,
  dependencies : [
    libgmp_dep,
    dependency('libvitisnetp4drv'),
    libsnutil_dep,
  ],
  include_directories : [
    ext_incdir,
    int_incdir,
  ],
  c_args : [
    #'-DSDNETIO_DEBUG',
    '-DSDNETCONFIG_DEBUG',
  ],
  install : true,
  soversion : 1,
)

install_headers(
  [
    'include/snp4.h',
  ],
  subdir : 'esnet-smartnic'
)

pkg = import('pkgconfig')
pkg.generate(
  libsnp4,
  subdirs : 'esnet-smartnic',
)

libsnp4_dep = declare_dependency(
  include_directories : [ext_incdir],
  link_with : [
    libsnp4,
    libsnutil,
  ])
