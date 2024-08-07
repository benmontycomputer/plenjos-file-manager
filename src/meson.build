files_sources = [
  'main.c',
  'files-window.c',
]

files_deps = [
  dependency('gio-2.0', version: '>= 2.50'),
  dependency('gtk+-3.0', version: '>= 3.22'),
  dependency('libhandy-1', version: '>= 1.0.0'),
]

gnome = import('gnome')

files_sources += gnome.compile_resources('files-resources',
  'files.gresource.xml',
  c_name: 'files'
)

executable('files', files_sources,
  dependencies: files_deps,
  install: true,
)
