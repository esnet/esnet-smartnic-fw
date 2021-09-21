sources = files(
  [
    'src/sn-cli.c',
  ]
)

int_incdir = []

cc = meson.get_compiler('c')
libgmp_dep = cc.find_library('gmp')

sn_cli_bin = executable(
  'sn-cli',
  sources,
  include_directories : [
    int_incdir,
  ],
  dependencies : [
    libgmp_dep,
    libopennic_dep,
    libsdnetapi_dep,
    dependency('regmap'),
  ],
  install : true,
)
