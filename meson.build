project(
	'slurp',
	'c',
	version: '1.4.0',
	license: 'MIT',
	meson_version: '>=0.59.0',
	default_options: ['c_std=c11', 'warning_level=2', 'werror=true'],
)

add_project_arguments('-Wno-unused-parameter', language: 'c')

cc = meson.get_compiler('c')

cairo = dependency('cairo')
realtime = cc.find_library('rt')
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols', version: '>=1.14')
xkbcommon = dependency('xkbcommon')
pkgcfg = import('pkgconfig')

subdir('protocol')

libslurp = static_library(
	'slurp',
	[
		'slurp.c',
		'pool-buffer.c',
		'render.c',
		protos_src,
	],
	dependencies: [
		cairo,
		realtime,
		wayland_client,
		wayland_cursor,
		xkbcommon,
	],
	include_directories: 'include',
	install: true,

)

executable(
	'slurp',
	'main.c',
	link_with: libslurp,
	include_directories: 'include',
	install: true,
)

install_headers('include/slurp.h', subdir : 'slurp')
pkgcfg.generate(libslurp)

scdoc = find_program('scdoc', required: get_option('man-pages'))

if scdoc.found()
	man_pages = ['slurp.1.scd']

	foreach src : man_pages
		topic = src.split('.')[0]
		section = src.split('.')[1]
		output = '@0@.@1@'.format(topic, section)

		custom_target(
			output,
			input: src,
			output: output,
			command: scdoc,
			feed: true,
			capture: true,
			install: true,
			install_dir: get_option('mandir') / 'man' + section,
		)
	endforeach
endif
