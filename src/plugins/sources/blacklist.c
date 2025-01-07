#include "blacklist.h"

#define DRAWER_MOD_NAME "bldrawer"
#define BLACKLIST_NAME "blacklist"

/* convenience macros to compress code */
#define PATH_MAX_ERR                                              \
do {                                                              \
  ERR("PATH_MAX exceeded. Need Len %d, PATH_MAX %d", len, PATH_MAX); \
  memset(path,0,PATH_MAX);                                        \
  success = EINA_FALSE;                                           \
 } while(0)


/* Private Funcions */
Eina_Bool _set_blacklist_path(char *path);
Eina_Bool _mkpath(const char *path);
Eina_Bool _set_data_path(char *path, size_t path_size);


Eina_Bool
_set_blacklist_path(char *path)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(path, EINA_FALSE);

   char temp_str[PATH_MAX] = { 0 };
   Eina_Bool success = EINA_TRUE;

   if (_set_data_path(path, PATH_MAX))
     {
        const int len = snprintf(NULL, 0, "%s%s/%s", path, DRAWER_MOD_NAME, BLACKLIST_NAME) + 1;

        if (len <= PATH_MAX)
          {
             strncpy(temp_str, path, PATH_MAX - 1);
             int ret = snprintf(path, PATH_MAX, "%s%s/", temp_str, DRAWER_MOD_NAME);
             if (ret < 0) PATH_MAX_ERR;
             success = _mkpath(path);
             strncat(path, BLACKLIST_NAME, PATH_MAX - strlen(path) - 1);
          } else
          {
             PATH_MAX_ERR;
          }
     } else
     {
        success = EINA_FALSE;
     }

   return success;
}

Eina_Bool
_mkpath(const char *path)
{
   //EINA_SAFETY_ON_NULL_RETURN_VAL(path, EINA_FALSE);
   if (!ecore_file_exists(path))
      return ecore_file_mkdir(path);
   return EINA_TRUE;
}

Eina_Bool
_set_data_path(char *path, size_t path_size)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(path, EINA_FALSE);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(path_size == 0, EINA_FALSE);

   const char *temp = efreet_data_home_get();
   char *data_home = eina_file_path_sanitize(temp);

   snprintf(path, path_size, "%s", data_home);
   free(data_home);

   // Ensure the path ends with a '/'
   size_t len = strlen(path);
   if (len + 1 < path_size && path[len - 1] != '/')
      strncat(path, "/", path_size - len - 1);
   else if (path[len - 1] != '/')
      // Not enough space to add '/' safely
      return EINA_FALSE;

   return EINA_TRUE;
}

Eet_Error
save_blacklist(Eina_List *items)
{
   Eet_File *blacklist_file = NULL;
   Eina_List *l = NULL;
   char str[3];
   char *exe;
   char blacklist_path[PATH_MAX] = { 0 };
   unsigned int i = 1;
   Eet_Error ret = EET_ERROR_NONE;

   /* Open blacklist file */
   if (!_set_blacklist_path(blacklist_path))
     {
        printf("blacklist File Creation Error: %s\n", blacklist_path);
        return EET_ERROR_BAD_OBJECT;
     }
   blacklist_file = eet_open(blacklist_path, EET_FILE_MODE_WRITE);

   if (blacklist_file)
     {
        /* If we have no items in blacklist wrap it up and return */
        if (!items)
           return eet_close(blacklist_file);
        /* Otherwise write each item */
        EINA_LIST_FOREACH(items, l, exe)
        {
           eina_convert_itoa(i++, str);
           eet_write(blacklist_file, str, exe, strlen(exe) + 1, 1);
        }
        /* and wrap it up */
        ret = eet_close(blacklist_file);
     } else
     {
        printf("Unable to open blacklist file: %s", blacklist_path);
        return EET_ERROR_BAD_OBJECT;
     }
   return ret;
}

Eet_Error
read_blacklist(Eina_List **items)
{
   Eet_File *blacklist_file = NULL;
   Eina_List *l = NULL;
   char blacklist_path[PATH_MAX] = { 0 };
   char *ret = NULL, **list;
   int i, num, size;

   /* Open blacklist file */
   if (!_set_blacklist_path(blacklist_path))
     {
        printf("blacklist File Creation Error: %s\n", blacklist_path);
        return EET_ERROR_BAD_OBJECT;
     }
   blacklist_file = eet_open(blacklist_path, EET_FILE_MODE_READ);
   if (!blacklist_file)
     {
        printf("Failed to open blacklist file: %s\n", blacklist_path);
        return EET_ERROR_BAD_OBJECT;
     }
   /* Read each item */
   list = eet_list(blacklist_file, "*", &num);
   if (list)
     {
        for (i = 0; i < num; i++)
          {
             ret = eet_read(blacklist_file, list[i], &size);
             *items = eina_list_append(*items, strdup(ret));
             free(ret);
          }
        free(list);
     }
   return eet_close(blacklist_file);
}
