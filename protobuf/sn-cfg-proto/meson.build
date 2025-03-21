#---------------------------------------------------------------------------------------------------
ver = 'v2'
base_name = 'sn_cfg'
base_name_ver = base_name + '_' + ver
proto_file = base_name_ver + '.proto'
py_pkg_name = base_name + '_proto'
py_pkg_ver = '0.1.0'
py_pkg_stem = py_pkg_name + '-' + py_pkg_ver
py_pkg_desc = 'SmartNIC Configuration'

#---------------------------------------------------------------------------------------------------
# C++ library.
sn_cfg_proto = custom_target(
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
sn_cfg_proto_dep = declare_dependency(
    sources: [
        sn_cfg_proto[2],
        sn_cfg_proto[3],
    ],
)

libsn_cfg_proto_cpp_args = [
    '-Wno-deprecated-declarations', # Needed due to how protobuf generates C++ code when a
                                    # nested message has been deprecated.
]
libsn_cfg_proto = shared_library(
    'sn_cfg_proto',
    [
        sn_cfg_proto[0],
        sn_cfg_proto[1],
    ],
    dependencies: [
        libgrpcpp,
        libprotobuf,
        sn_cfg_proto_dep,
    ],
    cpp_args: libsn_cfg_proto_cpp_args,
    install: true,
    soversion: 1,
)

pkg = import('pkgconfig')
pkg.generate(
    libsn_cfg_proto,
    extra_cflags: libsn_cfg_proto_cpp_args,
    subdirs: 'esnet-smartnic',
)

libsn_cfg_proto_dep = declare_dependency(
    link_with: [
        libsn_cfg_proto,
    ],
    compile_args: libsn_cfg_proto_cpp_args,
    dependencies: [
        libgrpcpp,
        libprotobuf,
        sn_cfg_proto_dep,
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
