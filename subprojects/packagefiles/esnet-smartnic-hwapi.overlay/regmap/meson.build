regio_generate_prog = find_program('regio-generate')
regio_info_prog = find_program('regio-info')

# Auto-discover the names of all of the blocks described within an IR definition
# This is used to precompute the names of all of the header files that will be
# generated.
#
# NOTE: This command is executed during the *configuration* phase and the results will be cached.
#       If you want to regenerate this list, you'll need to force one of the meson.build files to be out-of-date
#       by e.g. touch meson.build

dev = 'esnet-smartnic'
dev_ = dev.underscorify()
top_ir = dev + '-top-ir.yaml'
template_dir = '@SOURCE_ROOT@/regio/src/regio/tools/templates'

regmap_info = run_command(
  regio_info_prog,
  top_ir,
  check: true,
)

regmap_blocks = regmap_info.stdout().strip().split('\n')
regmap_block_headers = []
foreach b : regmap_blocks
  message('Block: ' + b)
  regmap_block_headers += b + '_block.h'
endforeach

regmap_c_headers = custom_target(
  'regmap-c-headers',
  input : top_ir,
  output : regmap_block_headers + [dev_ + '_toplevel.h'],
  command : [
    regio_generate_prog,
    '--file-type', 'top',
    '--recursive',
    '--template-dir', template_dir,
    '--generator', 'c',
    '--output-dir', '@OUTDIR@',
    '@INPUT@'
  ],
  install : true,
  install_dir : 'include/esnet-smartnic',
)

regmap_c_headers_dep = declare_dependency(
  sources : [regmap_c_headers],
  include_directories : include_directories('.')
)

meson.override_dependency(
  'regmap',
  regmap_c_headers_dep
)

regmap_ver = '0.1.0' # TODO: How to discover this?
regmap_stem = 'regmap_' + dev_ + '-' + regmap_ver
regmap_python = custom_target(
  'regmap-python',
  input : top_ir,
  output : [
    regmap_stem + '-py3-none-any.whl',
    regmap_stem + '.tar.gz',
  ],
  command : [
    'regmap-python.sh',
    regio_generate_prog,
    template_dir,
    '@PRIVATE_DIR@',
    '@OUTDIR@',
    '@INPUT@',
  ],
  install : true,
  install_dir : 'share/esnet-smartnic/python',
)
