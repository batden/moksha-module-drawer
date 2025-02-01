sources_install_dir = join_paths(lib_install_dir, 'plugins/sources')

#### directory_watcher view ####

directory_watcher = shared_module('directory_watcher',
   'directory_watcher.c',

   dependencies : [dep_e, dep_intl],
   install_dir: sources_install_dir,
   include_directories: config_dir,
   install: true,
   name_prefix: '',
   link_args: '-Wl,--unresolved-symbols=ignore-in-object-files'
)

install_data('directory_watcher.desktop',
  install_dir : sources_install_dir
)


#### history view ####

history = shared_module('history',
   'directory_watcher.c',
   'blacklist.c',

   dependencies : [dep_e, dep_intl],
   install_dir: sources_install_dir,
   include_directories: config_dir,
   install: true,
   name_prefix: '',
   link_args: '-Wl,--unresolved-symbols=ignore-in-object-files'
)

install_data('history.desktop',
  install_dir : sources_install_dir
)

#### launcher view ####

launcher = shared_module('launcher',
   'launcher.c',

   dependencies : [dep_e, dep_intl],
   install_dir: sources_install_dir,
   include_directories: config_dir,
   install: true,
   name_prefix: '',
   link_args: '-Wl,--unresolved-symbols=ignore-in-object-files'
)

install_data('launcher.desktop',
  install_dir : sources_install_dir
)

#### winlist view ####

winlist = shared_module('winlist',
   'winlist.c',

   dependencies : [dep_e, dep_intl],
   install_dir: sources_install_dir,
   include_directories: config_dir,
   install: true,
   name_prefix: '',
   link_args: '-Wl,--unresolved-symbols=ignore-in-object-files'
)

install_data('winlist.desktop',
  install_dir : sources_install_dir
)
