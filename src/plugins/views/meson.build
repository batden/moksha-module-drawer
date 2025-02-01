views_install_dir = join_paths(lib_install_dir, 'plugins/views')

#### grid view ####

grid = shared_module('grid',
   'grid.c',

   dependencies : [dep_e, dep_intl],
   install_dir: views_install_dir,
   include_directories: config_dir,
   install: true,
   name_prefix: '',
   link_args: '-Wl,--unresolved-symbols=ignore-in-object-files'
)

install_data('grid.desktop',
  install_dir : views_install_dir
)

#### list view ####

list = shared_module('list',
   'list.c',

   dependencies : [dep_e, dep_intl],
   install_dir: views_install_dir,
   include_directories: config_dir,
   install: true,
   name_prefix: '',
   link_args: '-Wl,--unresolved-symbols=ignore-in-object-files'
)

install_data('list.desktop',
  install_dir : views_install_dir
)
