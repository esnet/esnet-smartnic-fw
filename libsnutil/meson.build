ext_incdir = include_directories('include')

install_headers(
  [
    'include/array_size.h',
    'include/memory-barriers.h',
    'include/stringize.h',
    'include/unused.h',
  ],
  subdir : 'esnet-smartnic'
)

pkg = import('pkgconfig')
pkg.generate(
  name : 'snutil',
  description : 'ESnet SmartNIC utility library',
  subdirs : 'esnet-smartnic',
)

libsnutil_dep = declare_dependency(
  include_directories : [ext_incdir],
)
