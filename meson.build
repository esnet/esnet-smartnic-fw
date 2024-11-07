project(
  'esnet-smartnic',
  ['c', 'cpp'],
  default_options : [
    'warning_level=2',
    'werror=true',
  ],
)

subproject('gtest')
subproject('cli11')
subproject('prometheus-client-c')

subproject('esnet-smartnic-hwapi', default_options: ['default_library=static'])
subdir('libsnutil')
subdir('libsnp4')
subdir('libopennic')

subdir('protobuf') # Must occur before dependencies needing protobuf/grpc generated sources.
subdir('sn-cfg')
subdir('sn-p4')
