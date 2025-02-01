##### edje_cc binary compiler tool ###
edje_cc = get_option('edje-cc')
if edje_cc == ''
edje_cc = find_program('edje_cc')
endif

cmd = [ edje_cc,
        '-id', join_paths(meson.project_source_root(), 'data/images'),
        '@INPUT@', '@OUTPUT@']
custom_target('e-module-drawer.edj',
              input      : join_paths(meson.project_source_root(), 'data/e-module-drawer.edc'),
              output     : 'e-module-drawer.edj',
              command    : cmd,
              install_dir: mod_install_dir,
              install    : true
             )
