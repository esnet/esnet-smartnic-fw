sources = files(
  [
    'src/iov.c',
  ]
)

ext_incdir = include_directories('include')

libsnutil = static_library(
  'snutil',
  sources,
  include_directories : [
    ext_incdir,
  ])

libsnutil_dep = declare_dependency(
  include_directories : [ext_incdir],
  link_with : [
    libsnutil
  ])
