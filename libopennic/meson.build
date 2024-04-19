sources = files(
  [
    'src/cmac.c',
    'src/qdma.c',
    'src/smartnic_probe.c',
    'src/stats.c',
    'src/switch.c',
    'src/sysmon.c',
    'src/pcie.c',
  ]
)

ext_incdir = include_directories('include')
libprom_dep = dependency('prom')
threads_dep = dependency('threads')

libopennic = shared_library(
  'opennic',
  sources,
  dependencies : [
    dependency('regmap'),
    libprom_dep,
    libsnutil_dep,
    threads_dep,
  ],
  include_directories : [
    ext_incdir,
  ],
  install : true,
  soversion : 1,
)

install_headers(
  [
    'include/cmac.h',
    'include/qdma.h',
    'include/stats.h',
    'include/switch.h',
    'include/sysmon.h',
    'include/smartnic.h',
    'include/smartnic-block-compat.h',
  ],
  subdir : 'esnet-smartnic'
)

pkg = import('pkgconfig')
pkg.generate(
  libopennic,
  subdirs : 'esnet-smartnic',
)

libopennic_dep = declare_dependency(
  include_directories : [ext_incdir],
  link_with : [
    libopennic,
  ],
  dependencies: [
      libprom_dep,
      threads_dep,
  ],
)
