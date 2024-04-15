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

subproject('esnet-smartnic-hwapi', default_options: ['default_library=static'])
subdir('libsnutil')
subdir('libsnp4')
subdir('libopennic')

subdir('protobuf') # Must occur before dependencies needing protobuf/grpc generated sources.
subdir('sn-p4-proto')
subdir('sn-p4-agent')
subdir('sn-p4-cli')
subdir('sn-cli')
subdir('sn-cfg')
