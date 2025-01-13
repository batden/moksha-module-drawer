#ifndef BLACKLIST_H
#define BLACKLIST_H

#include <e.h>

Eet_Error  save_blacklist(Eina_List *items);
Eet_Error  read_blacklist(Eina_List **items);
Eina_List* clone_blacklist(Eina_List *items);
void       remove_blacklist();
#endif

