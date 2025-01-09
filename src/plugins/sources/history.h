#ifndef HISTORY_H
#define HISTORY_H

#include "../../Drawer.h"
#include "blacklist.h"

extern int _e_history_log_dom;
#undef DBG
#undef INF
#undef WRN
#undef ERR
#undef CRI
#define DBG(...)            EINA_LOG_DOM_DBG(_e_history_log_dom, __VA_ARGS__)
#define INF(...)            EINA_LOG_DOM_INFO(_e_history_log_dom, __VA_ARGS__)
#define WRN(...)            EINA_LOG_DOM_WARN(_e_history_log_dom, __VA_ARGS__)
#define ERR(...)            EINA_LOG_DOM_ERR(_e_history_log_dom, __VA_ARGS__)
#define CRI(...)            EINA_LOG_DOM_CRIT(_e_history_log_dom, __VA_ARGS__)

EAPI extern Drawer_Plugin_Api drawer_plugin_api;

EAPI void *drawer_plugin_init(Drawer_Plugin *p, const char *id);
EAPI int   drawer_plugin_shutdown(Drawer_Plugin *p);

EAPI Eina_List *drawer_source_list(Drawer_Source *s);
EAPI void  drawer_source_activate(Drawer_Source *s, Drawer_Source_Item *si, E_Zone *zone);

EAPI Evas_Object * drawer_plugin_config_get(Drawer_Plugin *p, Evas *evas);
EAPI void drawer_plugin_config_save(Drawer_Plugin *p);
EAPI void drawer_source_context(Drawer_Source *s, Drawer_Source_Item *si, E_Zone *zone, Drawer_Event_View_Context *ev);
EAPI const char * drawer_source_description_get(Drawer_Source *s);

#endif
