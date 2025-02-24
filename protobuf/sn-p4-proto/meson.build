#---------------------------------------------------------------------------------------------------
ver = 'v2'
base_name = 'sn_p4'
base_name_ver = base_name + '_' + ver
proto_file = base_name_ver + '.proto'
py_pkg_name = base_name + '_proto'
py_pkg_ver = '0.1.0'
py_pkg_stem = py_pkg_name + '-' + py_pkg_ver
py_pkg_desc = 'SmartNIC P4'

#---------------------------------------------------------------------------------------------------
# C++ library.
sn_p4_proto = custom_target(
    base_name_ver,
    input: [
        proto_file,
    ],
    output: [
        base_name_ver + '.pb.cc',
        base_name_ver + '.grpc.pb.cc',

        base_name_ver + '.pb.h',
        base_name_ver + '.grpc.pb.h',
    ],
    command: [
        protoc.full_path(),
        '--proto_path=@CURRENT_SOURCE_DIR@',
        '--cpp_out=@OUTDIR@',
        '--grpc_out=@OUTDIR@',
        '--plugin=protoc-gen-grpc=@0@'.format(grpc_cpp_plugin.path()),
        '@INPUT@',
    ],
    install: true,
    install_dir: 'include/esnet-smartnic',
)
sn_p4_proto_dep = declare_dependency(
    sources: [
        sn_p4_proto[2],
        sn_p4_proto[3],
    ],
)

libsn_p4_proto = shared_library(
    'sn_p4_proto',
    [
        sn_p4_proto[0],
        sn_p4_proto[1],
    ],
    dependencies: [
        libgrpcpp,
        libprotobuf,
        sn_p4_proto_dep,
    ],
    install: true,
    soversion: 1,
)

pkg = import('pkgconfig')
pkg.generate(
    libsn_p4_proto,
    subdirs: 'esnet-smartnic',
)

libsn_p4_proto_dep = declare_dependency(
    link_with: [
        libsn_p4_proto,
    ],
    dependencies: [
        libgrpcpp,
        libprotobuf,
        sn_p4_proto_dep,
    ],
)

#---------------------------------------------------------------------------------------------------
# Python generator.
src_dir = 'python/'
custom_target(
    py_pkg_name,
    input: [
        src_dir + '__init__.py',
        src_dir + ver + '/__init__.py',
        protoc_py_gen.process(proto_file),
    ],
    output: [
        py_pkg_stem + '-py3-none-any.whl',
        py_pkg_stem + '.tar.gz',
    ],
    command: [
        protobuf_py_pkg,
        py_pkg_name,
        py_pkg_ver,
        py_pkg_desc,
        '@PRIVATE_DIR@',
        '@OUTDIR@',
        '@CURRENT_SOURCE_DIR@/' + src_dir,
        '@INPUT@',
    ],
    install: true,
    install_dir: protobuf_py_install_dir,
)
