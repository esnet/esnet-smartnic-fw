# See: https://mesonbuild.com/Shipping-prebuilt-binaries-as-wraps.html

this_dir = meson.current_source_dir()

lib_base = 'vitisnetp4drv'
intf_base = lib_base + '-intf'

#-------------------------------------------------------------------------------
# Find system dependencies.
cc = meson.get_compiler('c')
libm_dep = cc.find_library('m', required: false)

#-------------------------------------------------------------------------------
# Find Vitis drivers. There are two layouts:
# - legacy: include and lib directories are peers with this file.
# - new:    sdnet_* directories are peers with this file. The include and lib
#           directories are beneath each sdnet_*.
ip_names = run_command(
    'bash', '-c',
    'find ' + this_dir +
    ' -mindepth 1 -maxdepth 1 ' +
    ' -name \'sdnet_*\' -printf \'%P\\n\'' +
    ' | sort -V',
    check: true).stdout().splitlines()

ip_dir_prefixes = {}
if ip_names.length() > 0
    foreach ip_name: ip_names
        ip_dir_prefixes += {ip_name: ip_name + '/'}
    endforeach
else
    ip_name = 'sdnet_0'
    ip_names += ip_name
    ip_dir_prefixes += {ip_name: ''}
endif

#-------------------------------------------------------------------------------
ip_lib_deps = []
foreach ip_name: ip_names
    ip_dir_prefix = ip_dir_prefixes[ip_name]
    inc_dir = include_directories('.', ip_dir_prefix + 'include')

    # Create a dependency for the Vitis driver library.
    drv_dep = cc.find_library(
        lib_base,
        dirs: this_dir / ip_dir_prefix + 'lib',
        static: true,
    )

    # Generate files to wrap the Vitis driver in a public interface.
    intf_name = intf_base + '-' + ip_name
    intf = custom_target(
        intf_name,
        output: [
            intf_name + '.c',
            intf_name + '.h',
            intf_name + '.map',
        ],
        command: [
            this_dir / intf_base + '.py',
            ip_name,
            '@OUTPUT@',
        ],
        depend_files: [
            intf_base + '.c.j2',
            intf_base + '.h.j2',
            intf_base + '.map.j2',
        ],
    )

    # Create a shared library for the public interface.
    lib_name = lib_base + '-' + ip_name
    lib = shared_library(
        lib_name,
        intf[0], # Get generated .c file.
        link_args: [
            # Restrict exported symbols according to generated .map file.
            '-Wl,--version-script=' + intf[2].full_path(),
        ],
        dependencies: [
            drv_dep,
            libm_dep,
        ],
        include_directories: inc_dir,
        install : true,
    )

    # Tie the public interface library into the dependency search.
    lib_dep = declare_dependency(
        link_with: lib,
        dependencies: declare_dependency(sources: intf[1]),
        include_directories: inc_dir,
    )
    ip_lib_deps += [lib_dep]
    meson.override_dependency(lib_name, lib_dep)
endforeach

#-------------------------------------------------------------------------------
# Generate a table for all wrapped Vitis driver interfaces.
table_base = intf_base + '-table'
table = custom_target(
    table_base,
    output: table_base + '.c',
    command: [
        this_dir / table_base + '.py',
        '@OUTPUT@',
        ip_names,
    ],
    depend_files: [
        table_base + '.c.j2',
    ],
)

# Tie the public interface library into the dependency search.
table_lib_dep = declare_dependency(
    sources: table[0],
    dependencies: ip_lib_deps,
)
meson.override_dependency(intf_base, table_lib_dep)
