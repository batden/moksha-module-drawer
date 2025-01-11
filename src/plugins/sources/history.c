#include "history.h"

/* Local Structures */
typedef enum _History_Sort_Type
{
   HISTORY_SORT_EXE,
   HISTORY_SORT_DATE,
   HISTORY_SORT_POPULARITY
} History_Sort_Type;

typedef struct _Instance Instance;
typedef struct _Conf Conf;
typedef struct _Blacklist_Item Blacklist_Item;

struct _Instance
{
   Drawer_Source        *source;

   Eina_List            *items, *handlers;
   Eina_List            *blacklist_items;
   Conf                 *conf;
   E_Menu               *menu;
   struct
   {
      E_Config_DD       *conf;
   } edd;
   const char           *description;
};

struct _Conf
{
   const char           *id;
   History_Sort_Type     sort_type;
   int                   blacklist;
};

struct _E_Config_Dialog_Data
{
   Instance             *inst;

   Evas_Object          *ilist;
   E_Confirm_Dialog     *dialog_delete;
   int                   sort_type;
   int                   blacklist;
};

struct _Blacklist_Item
{
   Eina_List            **items;
   Drawer_Source_Item   *si;
};

EINTERN int _e_history_log_dom = -1;

static void _history_description_create(Instance *inst);

static Drawer_Source_Item *_history_source_item_fill(Instance *inst, Efreet_Desktop *desktop, const char *file);
static void _history_source_items_free(Instance *inst);
static void _history_event_update_free(void *data __UNUSED__, void *event);
static void _history_event_update_icon_free(void *data __UNUSED__, void *event);
static Eina_Bool _history_efreet_desktop_list_change_cb(void *data, int ev_type __UNUSED__, void *event __UNUSED__);
static void _history_cb_menu_item_blacklist(void *data, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__);
static void _history_cb_menu_item_properties(void *data, E_Menu *m, E_Menu_Item *mi);
static void _history_cb_menu_item_remove(void *data, E_Menu *m, E_Menu_Item *mi);
static void _history_conf_activation_cb(void *data1, void *data2 __UNUSED__);

static void *_history_cf_create_data(E_Config_Dialog *cfd);
static void _history_cf_free_data(E_Config_Dialog *cfd __UNUSED__, E_Config_Dialog_Data *cfdata);
static void _history_cf_fill_data(E_Config_Dialog_Data *cfdata);
static Evas_Object *_history_cf_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static int _history_cf_basic_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);

EAPI Drawer_Plugin_Api drawer_plugin_api = { DRAWER_PLUGIN_API_VERSION, "History" };

static E_Config_Dialog *_cfd = NULL;

EAPI void *
drawer_plugin_init(Drawer_Plugin *p, const char *id)
{
   Instance *inst = NULL;
   char buf[128];

   _e_history_log_dom = eina_log_domain_register("History", EINA_COLOR_ORANGE);
   eina_log_domain_level_set("History", EINA_LOG_LEVEL_DBG);
   INF("History Init");
   inst = E_NEW(Instance, 1);

   inst->source = DRAWER_SOURCE(p);

   /* Define EET Data Storage */
   inst->edd.conf = E_CONFIG_DD_NEW("Conf", Conf);
#undef T
#undef D
#define T Conf
#define D inst->edd.conf
   E_CONFIG_VAL(D, T, id, STR);
   E_CONFIG_VAL(D, T, sort_type, INT);
   E_CONFIG_VAL(D, T, blacklist, INT);

   snprintf(buf, sizeof(buf), "module.drawer/%s.history", id);
   inst->conf = e_config_domain_load(buf, inst->edd.conf);
   if (!inst->conf)
     {
        inst->conf = E_NEW(Conf, 1);
        inst->conf->id = eina_stringshare_add(id);
        inst->conf->sort_type = HISTORY_SORT_POPULARITY;
        inst->conf->blacklist = 1;

        e_config_save_queue();
     }
  // FIXME: this is for temporary testing
  if (read_blacklist(&inst->blacklist_items) == EET_ERROR_BAD_OBJECT)
    {
       inst->blacklist_items = eina_list_append(inst->blacklist_items,
                                                strdup("nm-applet"));
                                                save_blacklist(inst->blacklist_items);
     }

#if 0
   inst->handlers = eina_list_append(inst->handlers,
                                     ecore_event_handler_add(EFREET_EVENT_DESKTOP_LIST_CHANGE,
                                                             _history_efreet_desktop_list_change_cb, inst));
#endif
   inst->handlers = eina_list_append(inst->handlers,
                                     ecore_event_handler_add(E_EVENT_EXEHIST_UPDATE,
                                                             _history_efreet_desktop_list_change_cb, inst));
   _history_description_create(inst);

   return inst;
}

EAPI int
drawer_plugin_shutdown(Drawer_Plugin *p)
{
   Instance *inst = NULL;

   inst = p->data;
   // Be sure to save blacklist file
   save_blacklist(inst->blacklist_items);

   _history_source_items_free(inst);

   E_FREE_LIST(inst->blacklist_items, free);
   eina_stringshare_del(inst->description);
   eina_stringshare_del(inst->conf->id);

   E_CONFIG_DD_FREE(inst->edd.conf);
   E_FREE_LIST(inst->handlers, ecore_event_handler_del);
   E_FREE(inst->conf);
   E_FREE(inst);

   eina_log_domain_unregister(_e_history_log_dom);
   _e_history_log_dom = -1;

   return 1;
}

EAPI Evas_Object *
drawer_plugin_config_get(Drawer_Plugin *p, Evas *evas)
{
   Evas_Object *button;

   button = e_widget_button_add(evas, D_("History settings"), NULL,
                                _history_conf_activation_cb, p, NULL);

   return button;
}

EAPI void
drawer_plugin_config_save(Drawer_Plugin *p)
{
   Instance *inst;
   char buf[128];

   inst = p->data;
   snprintf(buf, sizeof(buf), "module.drawer/%s.history", inst->conf->id);
   e_config_domain_save(buf, inst->edd.conf, inst->conf);
}

static const char *
_normalize_exe(const char *exe)
{
   char *base, *buf, *cp, *space = NULL;
   const char *ret;
   char name_desk[64];
   Eina_Bool flag = EINA_FALSE;
   Eina_Bool found = EINA_FALSE;
   int pos = 0;

   buf = strdup(exe);
   base = basename(buf);
   if ((base[0] == '.') && (base[1] == '\0'))
     {
        free(buf);
        return NULL;
     }

   cp = base;
   while (*cp)
     {
        if (isspace(*cp))
          {
             if (!space)
                space = cp;
             if (flag)
                flag = EINA_FALSE;
          } else if (!flag)
          {
             /* usually a variable in the desktop exe field */
             if (space && *cp == '%')
               {
                  flag = EINA_TRUE;
               } else
               {
                  char lower = tolower(*cp);

                  space = NULL;
                  if (lower != *cp)
                     *cp = lower;
               }
          }
       /* hack for libreoffice --apps and others */
       /* it normalizes to libreoffice-app style */
       if (*cp == '-')
         {
            cp++; pos++;
            if (*cp == '-')
              {
                found = EINA_TRUE;
                while (!isspace(*cp))
                  {
                    base[pos - 2] = *cp;
                    cp++; pos++;
                  }
                base[pos - 2] = '\0';
              }
          }
       cp++;
       pos++;
     }

   if (space) *space = '\0';

   if (found)
     {
       sprintf(name_desk, "%s.desktop", base);
       ret = eina_stringshare_add(name_desk);
     }
   else
     ret = eina_stringshare_add(base);

   free(buf);
   return ret;
}

EAPI Eina_List *
drawer_source_list(Drawer_Source *s)
{
   Instance *inst = NULL;
   Eina_List *hist = NULL, *l;
   Drawer_Event_Source_Main_Icon_Update *ev;
   const char *file;
   Eina_Compare_Cb cmp_func = (Eina_Compare_Cb)strcmp;

   if (!(inst = DRAWER_PLUGIN(s)->data)) return NULL;

   _history_source_items_free(inst);

   switch (inst->conf->sort_type)
     {
        case HISTORY_SORT_EXE:
           hist = e_exehist_sorted_list_get(E_EXEHIST_SORT_BY_EXE, 0);
           break;
        case HISTORY_SORT_DATE:
           hist = e_exehist_sorted_list_get(E_EXEHIST_SORT_BY_DATE, 0);
           break;
        case HISTORY_SORT_POPULARITY:
           hist = e_exehist_sorted_list_get(E_EXEHIST_SORT_BY_POPULARITY, 0);
           break;
     }

   if (!hist) return NULL;

   EINA_LIST_FOREACH(hist, l, file)
     {
        Drawer_Source_Item *si = NULL;
        Efreet_Desktop *desktop = efreet_util_desktop_exec_find(file);
        if (!desktop)
          {
             const char *norm_exe = _normalize_exe(file);
             desktop = efreet_util_desktop_exec_find(norm_exe);
             if (!desktop)
               desktop =  efreet_util_desktop_file_id_find(norm_exe);
             eina_stringshare_del(norm_exe);
          }
        /* FIXME: SKIP files*/
        if (desktop)
          {
             if (inst->conf->blacklist && !eina_list_search_unsorted_list(inst->blacklist_items, cmp_func, file))
               {
                  /* Instead of desktops, work with executables directly */
                 si = _history_source_item_fill(inst, desktop, file);
                 inst->items = eina_list_append(inst->items, si);
               } else if (!inst->conf->blacklist)
               {
                 si = _history_source_item_fill(inst, desktop, file);
                 inst->items = eina_list_append(inst->items, si);
               }
          }
     }
   // FIXME: WHY
   if (!inst->items->data) return NULL;

   ev = E_NEW(Drawer_Event_Source_Main_Icon_Update, 1);
   ev->source = inst->source;
   ev->id = eina_stringshare_add(inst->conf->id);
   ev->si = inst->items->data;
   ecore_event_add(DRAWER_EVENT_SOURCE_MAIN_ICON_UPDATE, ev,
                   _history_event_update_icon_free, NULL);

   return inst->items;
}

EAPI void
drawer_source_activate(Drawer_Source *s __UNUSED__, Drawer_Source_Item *si, E_Zone *zone)
{
   Efreet_Desktop *desktop;

   switch (si->data_type)
     {
       case SOURCE_DATA_TYPE_DESKTOP:
          desktop = si->data;

          if (desktop->type == EFREET_DESKTOP_TYPE_APPLICATION)
            {
               e_exec(zone, desktop, NULL, NULL, "drawer");
            } else if (desktop->type == EFREET_DESKTOP_TYPE_LINK)
            {
              if (!strncasecmp(desktop->url, "file:", 5))
                {
                   E_Action *act;

                   act = e_action_find("fileman");
                   if (act)
                      act->func.go(NULL, desktop->url + 5);
                }
            }
           break;
        case SOURCE_DATA_TYPE_FILE_PATH:
           e_exec(zone, NULL, si->data, NULL, "drawer");
           break;
        default:
           break;
     }
}

EAPI void
drawer_source_context(Drawer_Source *s, Drawer_Source_Item *si, E_Zone *zone, Drawer_Event_View_Context *ev)
{
   Instance *inst = NULL;
   E_Menu_Item *mi = NULL;

   inst = DRAWER_PLUGIN(s)->data;

   inst->menu = e_menu_new();

   mi = e_menu_item_new(inst->menu);
   e_menu_item_label_set(mi, D_("Change Item Properties"));
   e_util_menu_item_theme_icon_set(mi, "configure");
   e_menu_item_callback_set(mi, _history_cb_menu_item_properties, si);

   if (inst->conf->blacklist)
     {
        Blacklist_Item *bl = E_NEW(Blacklist_Item, 1);
        bl->items = &inst->blacklist_items;
        bl->si = si;
        mi = e_menu_item_new(inst->menu);
        e_menu_item_label_set(mi, D_("Blacklist Item"));
        e_util_menu_item_theme_icon_set(mi, "edit-clear");
        e_menu_item_callback_set(mi, _history_cb_menu_item_blacklist, bl);
     }

   mi = e_menu_item_new(inst->menu);
   e_menu_item_label_set(mi, D_("Remove Item"));
   e_util_menu_item_theme_icon_set(mi, "list-remove");
   e_menu_item_callback_set(mi, _history_cb_menu_item_remove, si);

   e_menu_activate(inst->menu, zone, ev->x, ev->y, 1, 1, E_MENU_POP_DIRECTION_AUTO);
}

EAPI const char *
drawer_source_description_get(Drawer_Source *s)
{
   Instance *inst;

   inst = DRAWER_PLUGIN(s)->data;
   return inst->description;
}

static void
_history_description_create(Instance *inst)
{
   eina_stringshare_del(inst->description);
   switch (inst->conf->sort_type)
     {
        case HISTORY_SORT_EXE:
           inst->description = eina_stringshare_add("Programs in history");
           break;
        case HISTORY_SORT_DATE:
           inst->description = eina_stringshare_add("Recently used programs");
           break;
        case HISTORY_SORT_POPULARITY:
           inst->description = eina_stringshare_add("Most used programs");
           break;
        default:
           break;
     }
}

static Drawer_Source_Item *
_history_source_item_fill(Instance *inst, Efreet_Desktop *desktop, const char *file)
{
   Drawer_Source_Item *si = NULL;

   si = E_NEW(Drawer_Source_Item, 1);

   if (desktop)
     {
        si->data = desktop;
        si->data_type = SOURCE_DATA_TYPE_DESKTOP;
        si->label = eina_stringshare_add(desktop->name);
        si->description = eina_stringshare_add(desktop->comment);
     } else
     {
        si->data = (char *) eina_stringshare_add(file);
        si->data_type = SOURCE_DATA_TYPE_FILE_PATH;
        si->label = eina_stringshare_add(file);
     }

   si->priv = (char *) eina_stringshare_add(file);
   si->source = inst->source;

   return si;
}

static void
_history_source_items_free(Instance *inst)
{
   EINA_SAFETY_ON_NULL_RETURN(inst);
   if (!inst->items) return;

   while (inst->items)
     {
        Drawer_Source_Item *si = NULL;

        si = inst->items->data;
        inst->items = eina_list_remove_list(inst->items, inst->items);
        switch (si->data_type)
          {
             case SOURCE_DATA_TYPE_DESKTOP:
                efreet_desktop_free(si->data);
                break;
             case SOURCE_DATA_TYPE_FILE_PATH:
                eina_stringshare_del(si->data);
                break;
             default:
                break;
          }
        eina_stringshare_del(si->label);
        eina_stringshare_del(si->description);
        eina_stringshare_del(si->category);
        eina_stringshare_del(si->priv);

        free(si);
     }
}

static void
_history_event_update_free(void *data __UNUSED__, void *event)
{
   Drawer_Event_Source_Update *ev;

   ev = event;
   eina_stringshare_del(ev->id);
   free(ev);
}

static void
_history_event_update_icon_free(void *data __UNUSED__, void *event)
{
   Drawer_Event_Source_Main_Icon_Update *ev;

   ev = event;
   eina_stringshare_del(ev->id);
   free(ev);
}

static Eina_Bool
_history_efreet_desktop_list_change_cb(void *data, int ev_type __UNUSED__, void *event __UNUSED__)
{
   Instance *inst = data;
   Drawer_Event_Source_Update *ev;

   ev = E_NEW(Drawer_Event_Source_Update, 1);
   ev->source = inst->source;
   ev->id = eina_stringshare_add(inst->conf->id);
   ecore_event_add(DRAWER_EVENT_SOURCE_UPDATE, ev, _history_event_update_free, NULL);

   return EINA_TRUE;
}

static void
_history_cb_menu_item_properties(void *data, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__)
{
   Drawer_Source_Item *si = data;

   if (si->data_type != SOURCE_DATA_TYPE_DESKTOP) return;
   e_desktop_edit(e_container_current_get(e_manager_current_get()), si->data);
}

static void
_history_cb_menu_item_blacklist(void *data, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__)
{
   Blacklist_Item *bl = data;

   *bl->items = eina_list_append(*bl->items, strdup(bl->si->priv));
   //FIXME: Do I really need to save here
   save_blacklist(*bl->items);
   ecore_event_add(E_EVENT_EXEHIST_UPDATE, NULL, NULL, NULL);
   E_FREE(bl);
}

static void
_history_cb_menu_item_remove(void *data, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__)
{
   Drawer_Source_Item *si = data;

   e_exehist_del((const char *) si->priv);
}

static void
_history_conf_activation_cb(void *data1, void *data2 __UNUSED__)
{
   Drawer_Plugin *p = NULL;
   Instance *inst = NULL;
   E_Config_Dialog_View *v = NULL;
   char buf[PATH_MAX];

   p = data1;
   inst = p->data;
   /* is this config dialog already visible ? */
   if (e_config_dialog_find("Drawer_History", "_e_module_drawer_cfg_dlg")) return;

   v = E_NEW(E_Config_Dialog_View, 1);
   if (!v) return;

   v->create_cfdata = _history_cf_create_data;
   v->free_cfdata = _history_cf_free_data;
   v->basic.create_widgets = _history_cf_basic_create;
   v->basic.apply_cfdata = _history_cf_basic_apply;

   /* Icon in the theme */
   snprintf(buf, sizeof(buf), "%s/e-module-drawer.edj", drawer_module_dir_get());

   /* create new config dialog */
   _cfd = e_config_dialog_new(e_container_current_get(e_manager_current_get()),
                              D_("Drawer Plugin : History"),
                              "Drawer_History",
                              "_e_module_drawer_cfg_dlg",
                              buf, 0, v, inst);

   e_dialog_resizable_set(_cfd->dia, 1);
}

/* Local Functions */
static void *
_history_cf_create_data(E_Config_Dialog *cfd)
{
   E_Config_Dialog_Data *cfdata = NULL;

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
   cfdata->inst = cfd->data;
   _history_cf_fill_data(cfdata);
   return cfdata;
}

static void
_history_cf_free_data(E_Config_Dialog *cfd __UNUSED__, E_Config_Dialog_Data *cfdata)
{
   _cfd = NULL;
   E_FREE(cfdata);
}

static void
_history_cf_fill_data(E_Config_Dialog_Data *cfdata)
{
   cfdata->sort_type = cfdata->inst->conf->sort_type;
   cfdata->blacklist = cfdata->inst->conf->blacklist;
}

static Evas_Object *
_history_cf_basic_create(E_Config_Dialog *cfd __UNUSED__, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *o, *of, *ob;
   E_Radio_Group *rg;

   o = e_widget_list_add(evas, 0, 0);

   rg = e_widget_radio_group_new(&(cfdata->sort_type));
   of = e_widget_framelist_add(evas, D_("Sorting options"), 0);
   ob = e_widget_radio_add(evas, D_("Sort applications by executable"), HISTORY_SORT_EXE, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, D_("Sort applications by date"), HISTORY_SORT_DATE, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, D_("Sort applications by popularity"), HISTORY_SORT_POPULARITY, rg);
   e_widget_framelist_object_append(of, ob);
   e_widget_list_object_append(o, of, 1, 1, 0.5);
   of = e_widget_framelist_add(evas, D_("Blacklist"), 0);
   ob = e_widget_check_add(evas, D_("Enable Blacklist"), &(cfdata->blacklist));
   e_widget_framelist_object_append(of, ob);

   e_widget_list_object_append(o, of, 1, 1, 0.5);

   return o;
}

static int
_history_cf_basic_apply(E_Config_Dialog *cfd __UNUSED__, E_Config_Dialog_Data *cfdata)
{
   Instance *inst = NULL;
   Drawer_Event_Source_Update *ev;

   inst = cfdata->inst;
   cfdata->inst->conf->sort_type = cfdata->sort_type;
inst->conf->blacklist = cfdata->inst->conf->blacklist = cfdata->blacklist;


   _history_description_create(inst);

   ev = E_NEW(Drawer_Event_Source_Update, 1);
   ev->source = inst->source;
   ev->id = eina_stringshare_add(inst->conf->id);
   ecore_event_add(DRAWER_EVENT_SOURCE_UPDATE, ev, _history_event_update_free, NULL);

   e_config_save_queue();
   return 1;
}
