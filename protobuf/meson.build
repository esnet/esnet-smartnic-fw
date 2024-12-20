#---------------------------------------------------------------------------------------------------
# C++ Protobuf and gRPC
libgrpcpp = dependency('grpc++')
libprotobuf = dependency('protobuf')

protoc = find_program('protoc')
grpc_cpp_plugin = find_program('grpc_cpp_plugin')

#---------------------------------------------------------------------------------------------------
# Python Protobuf and gRPC
# As per https://grpc.io/docs/languages/python/basics/
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
        '--proto_path=@CURRENT_SOURCE_DIR@',
        '--python_out=@BUILD_DIR@',
        '--pyi_out=@BUILD_DIR@',
        '--grpc_python_out=@BUILD_DIR@',
        '@INPUT@',
    ],
)

protobuf_py_pkg = meson.current_source_dir() + '/python-pkg.sh'
protobuf_py_install_dir = 'share/esnet-smartnic/python'

#---------------------------------------------------------------------------------------------------
# Sub-directories containing specific protobuf sources.
subdir('sn-cfg-proto')
subdir('sn-p4-proto')
