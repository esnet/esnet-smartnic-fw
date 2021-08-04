# See: https://mesonbuild.com/Shipping-prebuilt-binaries-as-wraps.html

cc = meson.get_compiler('c')

m_dep = cc.find_library('m', required : false)

libsdnetdrv_dep = declare_dependency(
  dependencies :
    [
      cc.find_library(
	'libsdnetdrv',
	dirs : join_paths(meson.current_source_dir(),
			  'lib'),
	static : true,
      ),
      m_dep
    ],
  include_directories : include_directories('include'),
)

meson.override_dependency(
  'libsdnetdrv',
  libsdnetdrv_dep
)
