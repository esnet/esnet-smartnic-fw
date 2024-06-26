#---------------------------------------------------------------------------------------------------
# C++ Agent
cpp = meson.get_compiler('cpp')
libgrpcpp_reflection_dep = cpp.find_library('grpc++_reflection')

cli11_dep = dependency('cli11')
jsoncpp_dep = dependency('jsoncpp')
libprom_dep = dependency('prom')
libpromhttp_dep = dependency('promhttp')
regmap_dep = dependency('regmap')

executable(
    'sn-cfg-agent',
    files([
        'src/agent/agent.cpp',
        'src/agent/batch.cpp',
        'src/agent/defaults.cpp',
        'src/agent/device.cpp',
        'src/agent/host.cpp',
        'src/agent/module.cpp',
        'src/agent/port.cpp',
        'src/agent/stats.cpp',
        'src/agent/switch.cpp',
    ]),
    dependencies: [
        cli11_dep,
        jsoncpp_dep,
        libgrpcpp_reflection_dep,
        libopennic_dep,
        libprom_dep,
        libpromhttp_dep,
        sn_cfg_proto_cpp,
        regmap_dep,
    ],
    # Force linking all libs so that the unreferenced grpc++_reflection lib gets linked
    link_args: '-Wl,--no-as-needed',
    install: true,
)

install_data(
    'sn-cfg-server-entrypoint.sh',
    'sn-cfg-check-for-fpga-ready',
    install_mode: ['rwxr-xr-x', 0, 0],
    install_dir: '/usr/local/sbin',
)

#---------------------------------------------------------------------------------------------------
# Python Client
pkg_name = 'sn_cfg'
pkg_ver = '0.1.0'
pkg_stem = pkg_name + '-' + pkg_ver
custom_target(
    pkg_name,
    input: [
        'src/client/__init__.py',
        'src/client/client.py',
        'src/client/completions.py',
        'src/client/defaults.py',
        'src/client/device.py',
        'src/client/error.py',
        'src/client/host.py',
        'src/client/module.py',
        'src/client/port.py',
        'src/client/stats.py',
        'src/client/switch.py',
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
