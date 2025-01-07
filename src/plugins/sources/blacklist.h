#ifndef BLACKLIST_H
#define BLACKLIST_H

#include <e.h>

Eet_Error save_blacklist(Eina_List *items);
Eet_Error read_blacklist(Eina_List **items);

#endif

