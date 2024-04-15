#---------------------------------------------------------------------------------------------------
base_name = 'sn_p4'
proto_file = base_name + '_v1.proto'
py_pkg_name = base_name + '_proto'
py_pkg_ver = '0.1.0'
py_pkg_stem = py_pkg_name + '-' + py_pkg_ver
py_pkg_desc = 'SmartNIC P4'

#---------------------------------------------------------------------------------------------------
# C++ generetor.
sn_p4_proto_cpp = declare_dependency(
    sources: protoc_cpp_gen.process(proto_file),
    dependencies: [
        libgrpcpp,
        libprotobuf,
    ],
)

#---------------------------------------------------------------------------------------------------
# Python generator.
custom_target(
    py_pkg_name,
    input: [
        'python/__init__.py',
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
        '@INPUT@',
    ],
    install: true,
    install_dir: protobuf_py_install_dir,
)
