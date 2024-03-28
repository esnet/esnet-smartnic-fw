sources = files(
  [
    'src/sn-cli.c',
    'src/sub-cmac.c',
    'src/sub-cms.c',
    'src/sub-dev.c',
    'src/sub-probe.c',
    'src/sub-qdma.c',
    'src/sub-sw.c',
  ]
)

int_incdir = []

cc = meson.get_compiler('c')
libgmp_dep = cc.find_library('gmp')
libm_dep = cc.find_library('m')

sn_cli_bin = executable(
  'sn-cli',
  sources,
  include_directories : [
    int_incdir,
  ],
  dependencies : [
    libgmp_dep,
    libopennic_dep,
    libm_dep,
    libsnp4_dep,
    libsnutil_dep,
    dependency('regmap'),
  ],
  install : true,
)
