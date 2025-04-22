# See: https://mesonbuild.com/Shipping-prebuilt-binaries-as-wraps.html

this_dir = meson.current_source_dir()

lib_base = 'vitisnetp4drv'
intf_base = lib_base + '-intf'

#-------------------------------------------------------------------------------
# Find system dependencies.
cc = meson.get_compiler('c')
libm_dep = cc.find_library('m', required: false)
regmap_dep = dependency('regmap')

#-------------------------------------------------------------------------------
# Find Vitis drivers. There are two layouts:
# - legacy: Include and lib directories for the ingress pipeline are peers with
#           this file. This will generate a single vitisnetp4 driver wrapper.
# - new:    <prefix>* directories for different pipelines are peers with this
#           file. The include and lib directories are beneath each <prefix>*. This
#           will generate a vitisnetp4 driver wrapper per pipeline. Currently,
#           only a combination of 2 pipelines is supported: ingress and egress.
#           The ordering follows the definition of the PipelineId enum from the
#           protobuf sn-p4-proto/sn_p4_v1.proto file. When an unknown pipeline
#           name is encountered, a build-time error will be forced to ensure that
#           the driver software is updated appropriately to support it.
EXPECTED_IP_PREFIXES = [
    'vitisnetp4_',
    'sdnet_',
]
EXPECTED_IP_NAMES = [
    'igr', # enum PipelineId INGRESS
    'egr', # enum PipelineId EGRESS
]
MAX_IP_NAMES = EXPECTED_IP_NAMES.length()

n_ip_names = 0
foreach ip_prefix: EXPECTED_IP_PREFIXES
    found_ip_names = run_command(
        'bash', '-c',
        'find ' + this_dir +
        ' -mindepth 1 -maxdepth 1 ' +
        ' -name \'@0@*\' -printf \'%P\\n\''.format(ip_prefix) +
        ' | sort -V',
        check: true).stdout().splitlines()

    n_ip_names = found_ip_names.length()
    if n_ip_names > 0
        break
    endif
endforeach

ip_dir_prefixes = {}
ip_names = []
regmap_names = {}
if n_ip_names == 0
    # Legacy naming for backwards compatibility.
    ip_name = 'sdnet_0'
    ip_names += [ip_name]
    ip_dir_prefixes += {ip_name: ''}
    regmap_names += {ip_name: 'sdnet'}
elif n_ip_names > MAX_IP_NAMES
    # Fail the build if an unsupported number of drivers is found.
    error('Too many vitisnetp4 drivers. Expected no more than @0@, found @1@.'.
          format(MAX_IP_NAMES, n_ip_names))
else
    expected_ip_names = []
    foreach ip_name: EXPECTED_IP_NAMES
        expected_ip_names += [ip_prefix + ip_name]
    endforeach

    # Fail the build if an unsupported IP name is found.
    foreach ip_name: found_ip_names
        if ip_name not in expected_ip_names
            error('Unknown vitisnetp4 IP name "@0@". Expected names are: @1@.'.
                  format(ip_name, expected_ip_names))
        endif
    endforeach

    # Force the ordering of the names rather than relying on the lexicographical
    # sorting. This is to ensure that each pipeline is given a known position in
    # the vitisnetp4 driver wrapper table. Alternatively, pipelines can be found
    # by name (i.e. use "sdnet_egr" to find the egress pipeline driver wrapper).
    # The ordering of strings in the ip_names list follows the definition of the
    # PipelineId enum from the protobuf sn-p4-proto/sn_p4_v1.proto file (refer
    # to the comment for EXPECTED_IP_NAMES). Empty strings indicate that an sdnet
    # is not implemented, resulting in a NULL entry in the driver wrapper table.
    foreach ip_name: expected_ip_names
        if ip_name in found_ip_names
            ip_names += [ip_name]
            ip_dir_prefixes += {ip_name: ip_name + '/'}
            regmap_names += {ip_name: ip_name}
        else
            # Leave a placeholder for the unimplemented sdnet. A NULL entry will
            # be made in the driver wrapper table.
            ip_names += ['']
        endif
    endforeach
endif

#-------------------------------------------------------------------------------
ip_lib_deps = []
foreach ip_name: ip_names
    if ip_name == ''
        continue
    endif

    ip_dir_prefix = ip_dir_prefixes[ip_name]
    ip_dir = this_dir / ip_dir_prefix
    inc_dir = include_directories('.', ip_dir_prefix + 'include')

    # Create a dependency for the Vitis driver library.
    drv_dep = cc.find_library(
        lib_base,
        dirs: ip_dir + 'lib',
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
            regmap_names[ip_name],
            ip_dir + 'metadata.yaml',
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
            regmap_dep,
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
