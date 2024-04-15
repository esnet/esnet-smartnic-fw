cpp = meson.get_compiler('cpp')
libgrpcpp_reflection_dep = cpp.find_library('grpc++_reflection')

sn_p4_agent = executable(
  'sn-p4-agent',
  files(
    [
      'src/sn-p4-agent.cpp',
    ]
  ),
  include_directories : include_directories(
    [
    ]
  ),
  dependencies : [
    libgmp_dep,
    libgrpcpp_reflection_dep,
    libopennic_dep,
    libsnp4_dep,
    sn_p4_proto_cpp,
    dependency('regmap'),
  ],
  # Force linking all libs so that the unreferenced grpc++_reflection lib gets linked
  link_args : '-Wl,--no-as-needed',
  install : true,
)

