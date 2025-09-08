#---------------------------------------------------------------------------------------------------
# C++ Agent
cpp = meson.get_compiler('cpp')
libgmp_dep = cc.find_library('gmp')
libgrpcpp_reflection_dep = cpp.find_library('grpc++_reflection')

cli11_dep = dependency('cli11')
jsoncpp_dep = dependency('jsoncpp',
    # Recent versions of meson (1.8.4+) have started to respect the subproject's "buildtype" option.  Since
    # jsoncpp sets buildtype=release, the c++ compiler runs with -O3 and is now reporting a warning in jsoncpp
    # due to stringop-overflow.  Temporarily override the subproject build to run with -O2 to avoid the warning
    # and (due to werror, a compile failure).
    #
    # This override should be removed (or reduced) once either jsoncpp is fixed or meson grows the ability to
    # pass cpp_args to a subproject to disable only one warning.
    default_options : [
        'buildtype=custom',
	'optimization=2',
    ],
)
libprom_dep = dependency('prom')
libpromhttp_dep = dependency('promhttp')
regmap_dep = dependency('regmap')

executable(
    'sn-p4-agent',
    files([
        'src/agent/agent.cpp',
        'src/agent/batch.cpp',
        'src/agent/counters.cpp',
        'src/agent/device.cpp',
        'src/agent/pipeline.cpp',
        'src/agent/server.cpp',
        'src/agent/stats.cpp',
        'src/agent/table.cpp',
        'src/agent/table-ecc.cpp',
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
        libsn_p4_proto_dep,
        regmap_dep,
    ],
    # Force linking all libs so that the unreferenced grpc++_reflection lib gets linked
    link_args: '-Wl,--no-as-needed',
    install: true,
)

ext_incdir = include_directories('include')
libsn_p4_client = shared_library(
    'sn_p4_client',
    'src/client/client.cpp',
    dependencies: [
        libsn_p4_proto_dep,
    ],
    include_directories: [
        ext_incdir,
    ],
    install: true,
    soversion: 1,
)

pkg = import('pkgconfig')
pkg.generate(
    libsn_p4_client,
    subdirs: 'esnet-smartnic',
)

libsn_p4_client_dep = declare_dependency(
    include_directories: [ext_incdir],
    link_with: [
        libsn_p4_client,
    ],
    dependencies: [
        libsn_p4_proto_dep,
    ],
)

install_headers(
    [
        'include/sn_p4_client.hpp',
    ],
    subdir: 'esnet-smartnic'
)

executable(
    'sn-p4-client-test',
    files([
        'src/client/client_test.cpp',
    ]),
    dependencies: [
        libsn_p4_client_dep,
    ],
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
        'src/client/completions.py',
        'src/client/device.py',
        'src/client/error.py',
        'src/client/p4bm.py',
        'src/client/pipeline.py',
        'src/client/server.py',
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
