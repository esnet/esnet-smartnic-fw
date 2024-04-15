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
