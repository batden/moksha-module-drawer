#include "directory_watcher.h"

/* Local Structures */
typedef struct _Instance Instance;
typedef struct _Conf Conf;
typedef struct _Dirwatcher_Priv Dirwatcher_Priv;

typedef enum
{
   SORT_NAME,
   SORT_ATIME,
   SORT_MTIME,
   SORT_CTIME,
   SORT_SIZE,
} Sort_Type;

struct _Instance
{
   Drawer_Source      *source;
   Conf               *conf;
   Eina_List          *items;
   E_Menu             *menu;
   struct
   {
      E_Config_DD     *conf;
   } edd;
   Ecore_File_Monitor *monitor;
   const char         *description;
};

struct _Conf
{
   const char         *id;
   const char         *dir;
   const char         *fm;
   Sort_Type           sort_type;
   Eina_Bool           sort_dir;
};

struct _Dirwatcher_Priv
{
   Eina_Bool           dir : 1;
   Eina_Bool           link : 1;
   Eina_Bool           mount : 1;
   const char         *mime;
   Instance           *inst;
};

struct _E_Config_Dialog_Data
{
   Instance           *inst;
   char               *dir;
   char               *fm;
   int                 sort_dir;
   int                 sort_type;
};

EAPI Drawer_Plugin_Api drawer_plugin_api = { DRAWER_PLUGIN_API_VERSION, "Directory Watcher" };

static void _dirwatcher_directory_activate(Instance *inst, E_Zone *zone, const char *path);
static void _dirwatcher_description_create(Instance *inst);
static void _dirwatcher_source_items_free(Instance *inst);
static Drawer_Source_Item * _dirwatcher_source_item_fill(Instance *inst, const char *file);
static void _dirwatcher_event_update_free(void *data __UNUSED__, void *event);
static void _dirwatcher_event_update_icon_free(void *data __UNUSED__, void *event);

static void _dirwatcher_monitor_cb(void *data, Ecore_File_Monitor *em __UNUSED__, Ecore_File_Event event __UNUSED__,
                                   const char *path);
static void _dirwatcher_cb_menu_open_dir(void *data, E_Menu *m, E_Menu_Item *mi);
static void _dirwatcher_conf_activation_cb(void *data1, void *data2 __UNUSED__);

static void * _dirwatcher_cf_create_data(E_Config_Dialog *cfd);
static void _dirwatcher_cf_free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static void _dirwatcher_cf_fill_data(E_Config_Dialog_Data *cfdata);
static Evas_Object * _dirwatcher_cf_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static int _dirwatcher_cf_basic_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static int _dirwatcher_cb_sort(const void *data1, const void *data2);
static int _dirwatcher_cb_sort_dir(const Drawer_Source_Item *si1, const Drawer_Source_Item *si2);

static E_Config_Dialog *_cfd = NULL;

EAPI void *
drawer_plugin_init(Drawer_Plugin *p, const char *id)
{
   Instance *inst = E_NEW(Instance, 1);
   char buf[128];

   inst->source = DRAWER_SOURCE(p);

   /* Define EET Data Storage */
   inst->edd.conf = E_CONFIG_DD_NEW("Conf", Conf);
#undef T
#undef D
#define T Conf
#define D inst->edd.conf
   E_CONFIG_VAL(D, T, id, STR);
   E_CONFIG_VAL(D, T, dir, STR);
   E_CONFIG_VAL(D, T, fm, STR);
   E_CONFIG_VAL(D, T, sort_type, INT);
   E_CONFIG_VAL(D, T, sort_dir, INT);

   snprintf(buf, sizeof(buf), "module.drawer/%s.dirwatcher", id);
   inst->conf = e_config_domain_load(buf, inst->edd.conf);
   if (!inst->conf)
     {
        char buf2[PATH_MAX];

        snprintf(buf2, sizeof(buf2), "%s/Desktop", e_user_homedir_get());

        inst->conf = E_NEW(Conf, 1);
        inst->conf->sort_dir = EINA_TRUE;
        inst->conf->dir = eina_stringshare_add(buf2);
        inst->conf->fm = eina_stringshare_add("");
        inst->conf->id = eina_stringshare_add(id);

        e_config_save_queue();
     }

   inst->monitor = ecore_file_monitor_add(inst->conf->dir,
                                          _dirwatcher_monitor_cb, inst);
   _dirwatcher_description_create(inst);

   return inst;
}

EAPI int
drawer_plugin_shutdown(Drawer_Plugin *p)
{
   Instance *inst = p->data;

   if (inst->monitor)
      ecore_file_monitor_del(inst->monitor);

   eina_stringshare_del(inst->description);
   eina_stringshare_del(inst->conf->id);
   eina_stringshare_del(inst->conf->dir);
   eina_stringshare_del(inst->conf->fm);

   E_CONFIG_DD_FREE(inst->edd.conf);
   E_FREE(inst->conf);
   E_FREE(inst);

   return 1;
}

EAPI Eina_List *
drawer_source_list(Drawer_Source *s)
{
   Eina_List *files;
   Instance *inst = NULL;
   Drawer_Event_Source_Main_Icon_Update *ev;
   char *file;

   if (!(inst = DRAWER_PLUGIN(s)->data)) return NULL;
   if (!(ecore_file_is_dir(inst->conf->dir))) return NULL;

   _dirwatcher_source_items_free(inst);

   files = ecore_file_ls(inst->conf->dir);
   EINA_LIST_FREE(files, file)
   {
      if (file[0] == '.') goto end;
      Drawer_Source_Item *si = _dirwatcher_source_item_fill(inst, file);
      if (si)
         inst->items = eina_list_append(inst->items, si);
end:
      free(file);
   }

   inst->items = eina_list_sort(inst->items,
                                eina_list_count(inst->items), _dirwatcher_cb_sort);

   ev = E_NEW(Drawer_Event_Source_Main_Icon_Update, 1);
   ev->source = inst->source;
   ev->id = eina_stringshare_add(inst->conf->id);
   ev->si = inst->items->data;
   ecore_event_add(DRAWER_EVENT_SOURCE_MAIN_ICON_UPDATE,
                   ev, _dirwatcher_event_update_icon_free, NULL);

   return inst->items;
}

EAPI void
drawer_source_activate(Drawer_Source *s, Drawer_Source_Item *si, E_Zone *zone)
{
   Dirwatcher_Priv *p = NULL;
   Instance *inst = DRAWER_PLUGIN(s)->data;

   if (si->data_type != SOURCE_DATA_TYPE_FILE_PATH) return;

   p = si->priv;
   if (p->dir)
      return _dirwatcher_directory_activate(p->inst, zone, si->data);
   if (si->data)
     {
        Efreet_Desktop *desktop;

        if ((e_util_glob_case_match(si->data, "*.desktop")) ||
            (e_util_glob_case_match(si->data, "*.directory")))
          {
             desktop = efreet_desktop_new(si->data);
             if (!desktop) return;

             e_exec(e_util_zone_current_get(e_manager_current_get()),
                    desktop, NULL, NULL, NULL);
             if (p->mime)
                e_exehist_mime_desktop_add(p->mime, desktop);

             efreet_desktop_free(desktop);
             return;
          } else if (p->mime)
          {
             desktop = e_exehist_mime_desktop_get(p->mime);
             if (desktop)
               {
                  char pcwd[PATH_MAX];
                  Eina_List *files = NULL;

                  if (!getcwd(pcwd, sizeof(pcwd))) return;
                  if (!chdir(inst->conf->dir)) return;

                  files = eina_list_append(files, si->data);
                  e_exec(zone, desktop, NULL, files, "drawer");
                  eina_list_free(files);

                  if (!chdir(pcwd)) return;
                  return;
               }
             else
               e_util_open((char *)si->data, NULL);
          }
        return;
     }
}

EAPI void
drawer_source_trigger(Drawer_Source *s, E_Zone *zone)
{
   Instance *inst = DRAWER_PLUGIN(s)->data;

   _dirwatcher_directory_activate(inst, zone, inst->conf->dir);
}

EAPI void
drawer_source_context(Drawer_Source *s, Drawer_Source_Item *si __UNUSED__, E_Zone *zone, Drawer_Event_View_Context *ev)
{
   Instance *inst  = DRAWER_PLUGIN(s)->data;
   E_Menu_Item *mi = NULL;

   inst->menu = e_menu_new();

   mi = e_menu_item_new(inst->menu);
   e_menu_item_label_set(mi, D_("Open Containing Directory"));
   e_util_menu_item_theme_icon_set(mi, "folder");
   e_menu_item_callback_set(mi, _dirwatcher_cb_menu_open_dir, inst);

   e_menu_activate(inst->menu, zone, ev->x, ev->y, 1, 1, E_MENU_POP_DIRECTION_AUTO);
}

EAPI Evas_Object *
drawer_plugin_config_get(Drawer_Plugin *p, Evas *evas)
{
   Evas_Object *button = e_widget_button_add(evas, D_("Directory Watcher settings"), NULL, _dirwatcher_conf_activation_cb, p, NULL);

   return button;
}

EAPI void
drawer_plugin_config_save(Drawer_Plugin *p)
{
   Instance *inst = p->data;
   char buf[128];

   snprintf(buf, sizeof(buf), "module.drawer/%s.dirwatcher", inst->conf->id);
   e_config_domain_save(buf, inst->edd.conf, inst->conf);
}

EAPI const char *
drawer_source_description_get(Drawer_Source *s)
{
   Instance *inst = DRAWER_PLUGIN(s)->data;

   return inst->description;
}

static void
_dirwatcher_directory_activate(Instance *inst, E_Zone *zone, const char *path)
{
   char exec[PATH_MAX];

   if (inst->conf->fm && (inst->conf->fm[0] != '\0'))
     {
        snprintf(exec, PATH_MAX, "%s \"%s\"", inst->conf->fm, path);
        e_exec(zone, NULL, exec, NULL, NULL);
     } else
     {
        E_Action *act = NULL;

        act = e_action_find("fileman");
        if (act)
           if (act && act->func.go)
              act->func.go(E_OBJECT(e_manager_current_get()), path);
     }
}

static void
_dirwatcher_description_create(Instance *inst)
{
   char buf[1024];
   char path[PATH_MAX];
   const char *homedir;

   eina_stringshare_del(inst->description);
   homedir = e_user_homedir_get();
   if (!(strncmp(inst->conf->dir, homedir, PATH_MAX)))
     {
        snprintf(buf, sizeof(buf), D_("Home"));
     } else if (!(strncmp(inst->conf->dir, homedir, strlen(homedir))))
     {
        snprintf(path, sizeof(path), "%s", inst->conf->dir);
        snprintf(buf, sizeof(buf), "%s", path + strlen(homedir) + 1);
     } else
     {
        snprintf(buf, sizeof(buf), "%s", inst->conf->dir);
     }
   inst->description = eina_stringshare_add(buf);
}

static void
_dirwatcher_source_items_free(Instance *inst)
{
   while (inst->items)
     {
        Drawer_Source_Item *si = NULL;

        si = inst->items->data;
        inst->items = eina_list_remove_list(inst->items, inst->items);
        eina_stringshare_del(si->label);
        eina_stringshare_del(si->description);
        eina_stringshare_del(si->category);

        E_FREE(si->priv);
        E_FREE(si);
     }
}

static Drawer_Source_Item *
_dirwatcher_source_item_fill(Instance *inst, const char *file)
{
   Drawer_Source_Item *si = E_NEW(Drawer_Source_Item, 1);
   Dirwatcher_Priv *p = E_NEW(Dirwatcher_Priv, 1);
   char buf[PATH_MAX];
   const char *mime, *file_path;

   si->priv = p;

   snprintf(buf, sizeof(buf), "%s/%s", inst->conf->dir, file);
   if ((e_util_glob_case_match(buf, "*.desktop")) ||
       (e_util_glob_case_match(buf, "*.directory")))
     {
        Efreet_Desktop *desktop = efreet_desktop_new(buf);
        if (!desktop) return NULL;
        si->label = eina_stringshare_add(desktop->name);
        efreet_desktop_free(desktop);
     } else
     {
        si->label = eina_stringshare_add(file);
     }

   file_path = eina_stringshare_add(buf);

   mime = e_fm_mime_filename_get(file_path);
   if (mime)
     {
        snprintf(buf, sizeof(buf), "%s (%s)", mime,
                 e_util_size_string_get(ecore_file_size(file_path)));
        p->mime = mime;
     } else if (ecore_file_is_dir(file_path))
     {
        snprintf(buf, sizeof(buf), D_("Directory (%s)"),
                 e_util_size_string_get(ecore_file_size(file_path)));
        p->dir = EINA_TRUE;
     } else
     {
        snprintf(buf, sizeof(buf), "%s (%s)", basename((char *) file_path),
                 e_util_size_string_get(ecore_file_size(file_path)));
     }
   si->description = eina_stringshare_add(buf);

   p->inst = inst;
   si->data = (char *) file_path;
   si->data_type = SOURCE_DATA_TYPE_FILE_PATH;
   si->source = inst->source;

   return si;
}

static void
_dirwatcher_event_update_free(void *data __UNUSED__, void *event)
{
   Drawer_Event_Source_Update *ev = event;
   eina_stringshare_del(ev->id);
   free(ev);
}

static void
_dirwatcher_event_update_icon_free(void *data __UNUSED__, void *event)
{
   Drawer_Event_Source_Main_Icon_Update *ev = event;
   eina_stringshare_del(ev->id);
   free(ev);
}

static void
_dirwatcher_monitor_cb(void *data, Ecore_File_Monitor *em __UNUSED__, Ecore_File_Event event __UNUSED__,
                       const char *path)
{
   Instance *inst = data;
   Drawer_Event_Source_Update *ev = E_NEW(Drawer_Event_Source_Update, 1);
   char *base = basename((char *) path);
   if (base[0] == '.') return;

   ev->source = inst->source;
   ev->id = eina_stringshare_add(inst->conf->id);
   ecore_event_add(DRAWER_EVENT_SOURCE_UPDATE,
                   ev, _dirwatcher_event_update_free, NULL);
}

static void
_dirwatcher_cb_menu_open_dir(void *data, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__)
{
   Instance *inst = NULL;

   if (!(inst = data)) return;
   _dirwatcher_directory_activate(inst, e_util_zone_current_get(e_manager_current_get()),
                                  inst->conf->dir);
}

static void
_dirwatcher_conf_activation_cb(void *data1, void *data2 __UNUSED__)
{
   Drawer_Plugin *p = NULL;
   Instance *inst = NULL;
   E_Config_Dialog_View *v = NULL;
   char buf[PATH_MAX];

   p = data1;
   inst = p->data;
   /* is this config dialog already visible ? */
   if (e_config_dialog_find("Drawer_Dirwatcher", "_e_module_drawer_cfg_dlg"))
      return;

   v = E_NEW(E_Config_Dialog_View, 1);
   if (!v) return;

   v->create_cfdata = _dirwatcher_cf_create_data;
   v->free_cfdata = _dirwatcher_cf_free_data;
   v->basic.create_widgets = _dirwatcher_cf_basic_create;
   v->basic.apply_cfdata = _dirwatcher_cf_basic_apply;

   /* Icon in the theme */
   snprintf(buf, sizeof(buf), "%s/e-module-drawer.edj", drawer_module_dir_get());

   /* create new config dialog */
   _cfd = e_config_dialog_new(e_container_current_get(e_manager_current_get()),
                              D_("Drawer Plugin : Directory Watcher"), "Drawer_Dirwatcher",
                              "_e_module_drawer_cfg_dlg", buf, 0, v, inst);

   e_dialog_resizable_set(_cfd->dia, 1);
}

static void *
_dirwatcher_cf_create_data(E_Config_Dialog *cfd)
{
   E_Config_Dialog_Data *cfdata = E_NEW(E_Config_Dialog_Data, 1);
   cfdata->inst = cfd->data;
   _dirwatcher_cf_fill_data(cfdata);
   return cfdata;
}

static void
_dirwatcher_cf_free_data(E_Config_Dialog *cfd __UNUSED__, E_Config_Dialog_Data *cfdata)
{
   if (cfdata->dir) E_FREE(cfdata->dir);
   if (cfdata->fm) E_FREE(cfdata->fm);

   _cfd = NULL;
   E_FREE(cfdata);
}

static void
_dirwatcher_cf_fill_data(E_Config_Dialog_Data *cfdata)
{
   cfdata->dir = strdup(cfdata->inst->conf->dir);
   cfdata->fm = strdup(cfdata->inst->conf->fm);
   cfdata->sort_dir = cfdata->inst->conf->sort_dir;
}

static Evas_Object *
_dirwatcher_cf_basic_create(E_Config_Dialog *cfd __UNUSED__, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *o, *of, *ob;
   E_Radio_Group *gr;

   o = e_widget_list_add(evas, 0, 0);

   of = e_widget_framelist_add(evas, D_("Watch path"), 1);
   ob = e_widget_entry_add(evas, &cfdata->dir, NULL, NULL, NULL);
   e_widget_framelist_object_append(of, ob);

   e_widget_list_object_append(o, of, 1, 0, 0.5);

   of = e_widget_framelist_add(evas, D_("Custom file manager"), 1);
   ob = e_widget_entry_add(evas, &cfdata->fm, NULL, NULL, NULL);
   e_widget_framelist_object_append(of, ob);

   e_widget_list_object_append(o, of, 1, 0, 0.5);

   of = e_widget_framelist_add(evas, D_("Sort Options"), 0);
   ob = e_widget_check_add(evas, D_("Sort directories first"), &cfdata->sort_dir);
   e_widget_framelist_object_append(of, ob);

   gr = e_widget_radio_group_new(&cfdata->sort_type);
   ob = e_widget_radio_add(evas, D_("Sort by name"), SORT_NAME, gr);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, D_("Sort by access time"), SORT_ATIME, gr);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, D_("Sort by modification time"), SORT_MTIME, gr);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, D_("Sort by change time"), SORT_CTIME, gr);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, D_("Sort by size"), SORT_SIZE, gr);
   e_widget_framelist_object_append(of, ob);

   e_widget_list_object_append(o, of, 1, 1, 0.5);

   return o;
}

static int
_dirwatcher_cf_basic_apply(E_Config_Dialog *cfd __UNUSED__, E_Config_Dialog_Data *cfdata)
{
   Instance *inst  = cfdata->inst;
   Drawer_Event_Source_Update *ev;
   char *path;

   eina_stringshare_del(cfdata->inst->conf->dir);
   eina_stringshare_del(cfdata->inst->conf->fm);
   cfdata->inst->conf->sort_dir = cfdata->sort_dir;
   cfdata->inst->conf->sort_type = cfdata->sort_type;

   path = ecore_file_realpath(cfdata->dir);
   cfdata->inst->conf->dir = eina_stringshare_add(path);
   cfdata->inst->conf->fm = eina_stringshare_add(cfdata->fm);
   E_FREE(path);

   if (inst->monitor)
      ecore_file_monitor_del(inst->monitor);
   inst->monitor = ecore_file_monitor_add(inst->conf->dir,
                                          _dirwatcher_monitor_cb, inst);

   _dirwatcher_description_create(inst);

   ev = E_NEW(Drawer_Event_Source_Update, 1);
   ev->source = inst->source;
   ev->id = eina_stringshare_add(inst->conf->id);
   ecore_event_add(DRAWER_EVENT_SOURCE_UPDATE,
                   ev, _dirwatcher_event_update_free, NULL);

   e_config_save_queue();
   return 1;
}

static int
_dirwatcher_cb_sort_dir(const Drawer_Source_Item *si1, const Drawer_Source_Item *si2)
{
   int d1, d2;

   d1 = ecore_file_is_dir(si1->data);
   d2 = ecore_file_is_dir(si2->data);

   if (d1 && d2)
      return strcmp(si1->data, si2->data);

   if (d1)
      return -1;
   if (d2)
      return 1;
   return 0;
}

static int
_dirwatcher_cb_sort(const void *data1, const void *data2)
{
   const Drawer_Source_Item *si1 = NULL, *si2 = NULL;
   Instance *inst;
   const char *name1;
   const char *name2;
   struct stat st1, st2;
   long long size1, size2;

   si1 = data1;
   si2 = data2;
   inst = ((Dirwatcher_Priv *) si1->priv)->inst;
   switch (inst->conf->sort_type)
     {
        case SORT_NAME:
           if (inst->conf->sort_dir)
             {
                int ret = _dirwatcher_cb_sort_dir(si1, si2);
                if (ret) return ret;
             }

           name1 = ecore_file_file_get(si1->data);
           name2 = ecore_file_file_get(si2->data);
           return strcmp(name1, name2);
        case SORT_MTIME:
           if (inst->conf->sort_dir)
             {
                int ret = _dirwatcher_cb_sort_dir(si1, si2);
                if (ret) return ret;
             }

           if (stat(si1->data, &st1) < 0) return 0;
           if (stat(si2->data, &st2) < 0) return 0;
           return st1.st_mtime - st2.st_mtime;
        case SORT_CTIME:
           if (inst->conf->sort_dir)
             {
                int ret = _dirwatcher_cb_sort_dir(si1, si2);
                if (ret) return ret;
             }

           if (stat(si1->data, &st1) < 0) return 0;
           if (stat(si2->data, &st2) < 0) return 0;
           return st1.st_ctime - st2.st_ctime;
        case SORT_ATIME:
           if (inst->conf->sort_dir)
             {
                int ret = _dirwatcher_cb_sort_dir(si1, si2);
                if (ret) return ret;
             }

           if (stat(si1->data, &st1) < 0) return 0;
           if (stat(si2->data, &st2) < 0) return 0;
           return st1.st_atime - st2.st_atime;
        case SORT_SIZE:
           if (inst->conf->sort_dir)
             {
                int ret = _dirwatcher_cb_sort_dir(si1, si2);
                if (ret) return ret;
             }

           size1 = ecore_file_size(si1->data);
           size2 = ecore_file_size(si2->data);
           return size1 - size2;
     }

   return 0;
}
