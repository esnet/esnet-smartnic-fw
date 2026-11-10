project('esnet-opennic-hwapi', 'c')

fs = import('fs')

this_dir = meson.current_source_dir()
board = get_option('SN_HW_BOARD')
if board != 'None' and fs.is_dir(board)
  board_subdir = board
  hwapi_root_dir = this_dir / board_subdir
else
  board_subdir = ''
  hwapi_root_dir = this_dir
endif

# The regmap IR input file needs to be anchored at the root of the hwapi tree since custom_target()
# input doesn't support reaching a level above (i.e. is locked to the "regmap" sub-directory).
top_ir_file = files(board_subdir / 'regmap/esnet-smartnic-top-ir.yaml')

subdir('regmap')
subdir('libvitisnetp4drv')

install_subdir(
  board_subdir / 'firmware',
  install_dir : 'lib/firmware/esnet-smartnic',
  strip_directory : true,
)

install_subdir(
  board_subdir / 'regmap/spec',
  install_dir : 'share/esnet-smartnic-dev/spec',
  strip_directory : true,
)

install_data(
  top_ir_file,
  install_dir : 'share/esnet-smartnic',
)

install_subdir(
  board_subdir / 'wireshark',
  install_dir : get_option('libdir'),
)
