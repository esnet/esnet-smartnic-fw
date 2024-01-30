#---------------------------------------------------------------------------------------------------
# C++ Protobuf and gRPC
libgrpcpp = dependency('grpc++')
libprotobuf = dependency('protobuf')

protoc = find_program('protoc')
grpc_cpp_plugin = find_program('grpc_cpp_plugin')

protoc_cpp_gen = generator(
    protoc,
    output: [
        '@BASENAME@.grpc.pb.cc',
        '@BASENAME@.grpc.pb.h',
        '@BASENAME@.pb.cc',
        '@BASENAME@.pb.h',
    ],
    arguments: [
        '--proto_path=@CURRENT_SOURCE_DIR@',
        '--cpp_out=@BUILD_DIR@',
        '--grpc_out=@BUILD_DIR@',
        '--plugin=protoc-gen-grpc=@0@'.format(grpc_cpp_plugin.path()),
        '@INPUT@',
    ],
)

proto_cpp = declare_dependency(
    sources: protoc_cpp_gen.process('sn_cfg_v1.proto'),
    dependencies: [
        libgrpcpp,
        libprotobuf,
    ],
)

#---------------------------------------------------------------------------------------------------
# C++ Agent
cpp = meson.get_compiler('cpp')
libgrpcpp_reflection_dep = cpp.find_library('grpc++_reflection')
cli11_dep = dependency('cli11')
jsoncpp_dep = dependency('jsoncpp')

executable(
    'sn-cfg-agent',
    files([
        'src/agent/agent.cpp',
        'src/agent/batch.cpp',
        'src/agent/device.cpp',
        'src/agent/host.cpp',
        'src/agent/port.cpp',
        'src/agent/switch.cpp',
    ]),
    dependencies: [
        cli11_dep,
        jsoncpp_dep,
        libgrpcpp_reflection_dep,
        proto_cpp,
    ],
    # Force linking all libs so that the unreferenced grpc++_reflection lib gets linked
    link_args: '-Wl,--no-as-needed',
    install: true,
)

#---------------------------------------------------------------------------------------------------
# Python Protobuf and gRPC
# As per https://grpc.io/docs/languages/python/basics/
pkg_name = 'sn_cfg'
python = find_program('python3')
protoc_py_gen = generator(
    python,
    output: [
        '@BASENAME@_pb2.py',
        '@BASENAME@_pb2.pyi',
        '@BASENAME@_pb2_grpc.py',
    ],
    arguments: [
        '-m', 'grpc_tools.protoc',
        '--proto_path=' + pkg_name + '=@CURRENT_SOURCE_DIR@', # pkg_name needed for proper imports.
        '--python_out=@BUILD_DIR@',
        '--pyi_out=@BUILD_DIR@',
        '--grpc_python_out=@BUILD_DIR@',
        '@INPUT@',
    ],
)

#---------------------------------------------------------------------------------------------------
# Python Client
pkg_ver = '0.1.0'
pkg_stem = pkg_name + '-' + pkg_ver
custom_target(
    pkg_name,
    input: [
        'src/client/__init__.py',
        'src/client/client.py',
        'src/client/completions.py',
        'src/client/device.py',
        'src/client/error.py',
        'src/client/host.py',
        'src/client/port.py',
        'src/client/switch.py',
        'src/client/utils.py',
        protoc_py_gen.process('sn_cfg_v1.proto'),
    ],
    output: [
        pkg_stem + '-py3-none-any.whl',
        pkg_stem + '.tar.gz',
    ],
    command: [
        'python-pkg.sh',
        pkg_name,
        pkg_ver,
        '@PRIVATE_DIR@',
        '@OUTDIR@',
        '@INPUT@',
    ],
    install: true,
    install_dir: 'share/esnet-smartnic/python',
)
