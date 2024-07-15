#---------------------------------------------------------------------------------------------------
# C++ Agent
cpp = meson.get_compiler('cpp')
libgmp_dep = cc.find_library('gmp')
libgrpcpp_reflection_dep = cpp.find_library('grpc++_reflection')

cli11_dep = dependency('cli11')
jsoncpp_dep = dependency('jsoncpp')
libprom_dep = dependency('prom')
libpromhttp_dep = dependency('promhttp')
regmap_dep = dependency('regmap')

executable(
    'sn-p4-v2-agent',
    files([
        'src/agent/agent.cpp',
        'src/agent/batch.cpp',
        'src/agent/device.cpp',
        'src/agent/pipeline.cpp',
        'src/agent/stats.cpp',
        'src/agent/table.cpp',
    ]),
    dependencies: [
        cli11_dep,
        jsoncpp_dep,
        libgmp_dep,
        libgrpcpp_reflection_dep,
        libopennic_dep,
        libprom_dep,
        libpromhttp_dep,
        libsnp4_dep,
        sn_p4_v2_proto_cpp,
        regmap_dep,
    ],
    # Force linking all libs so that the unreferenced grpc++_reflection lib gets linked
    link_args: '-Wl,--no-as-needed',
    install: true,
)

install_data(
    'sn-p4-server-entrypoint.sh',
    'sn-p4-check-for-fpga-ready',
    install_mode: ['rwxr-xr-x', 0, 0],
    install_dir: '/usr/local/sbin',
)

#---------------------------------------------------------------------------------------------------
# Python Client
pkg_name = 'sn_p4'
pkg_ver = '0.1.0'
pkg_stem = pkg_name + '-' + pkg_ver
custom_target(
    pkg_name,
    input: [
        'src/client/__init__.py',
        'src/client/client.py',
        'src/client/client_v2.py',
        'src/client/completions.py',
        'src/client/device.py',
        'src/client/error.py',
        'src/client/pipeline.py',
        'src/client/stats.py',
        'src/client/table.py',
        'src/client/utils.py',
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
    install_dir: protobuf_py_install_dir,
)
