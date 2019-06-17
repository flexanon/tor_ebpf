
#include "core/or/or.h"
#include "core/or/plugin_helper.h"
#include "src/lib/string/printf.h"
#include "app/config/config.h"
#include "log_test_helpers.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
/*#include "plugin.h"*/

#include "test.h"

static smartlist_t *list_plugins = NULL;

static int
is_private_dir(const char* path)
{
  struct stat st;
  int r = stat(path, &st);
  if (r) {
    return 0;
  }
#if !defined (_WIN32)
  if ((st.st_mode & (S_IFDIR | 0777)) != (S_IFDIR | 0700)) {
    return 0;
  }
#endif
  return 1;
}

static void
test_plugin_helper_find_all_and_init(void *args) {
  (void)args;
  int ret;
  /** Creating subdir plugins and files */
  get_options_mutable()->PluginsDirectory = tor_strdup(get_fname("plugins"));
  ret = check_private_dir(get_options()->PluginsDirectory, CPD_CREATE, NULL);
  tt_int_op(ret, OP_EQ, 0);
  const char *plugin_dir_1 = "test_1";
  char *subpath_1 = get_plugindir_fname(plugin_dir_1);
  const char *plugin_dir_2 = "test_2";
  char *subpath_2 = get_plugindir_fname(plugin_dir_2);
  tt_assert(!check_or_create_plugin_subdir(plugin_dir_1));
  tt_assert(is_private_dir(subpath_1));
  tt_assert(!check_or_create_plugin_subdir(plugin_dir_2));
  tt_assert(is_private_dir(subpath_2));
  const char* plugin_fname_1 = "test_1.plugin";
  const char* plugin_fname_2 = "test_2.plugin";
  const char* str_test_1 = "test_1 protocol_relay replace test_1.o";
  const char* str_test_2 = "test_2 protocol_relay param 42 add test_2.o";
  ret = write_to_plugin_subdir(plugin_dir_1, plugin_fname_1, str_test_1, NULL);
  tt_int_op(ret, OP_EQ, 0);
  ret = write_to_plugin_subdir(plugin_dir_2, plugin_fname_2, str_test_2, NULL);
  tt_int_op(ret, OP_EQ, 0);
  /** Try to initialize and load plugins */
  list_plugins = smartlist_new();
  list_plugins = plugin_helper_find_all_and_init();
  tt_assert(list_plugins);
  tt_int_op(smartlist_len(list_plugins), OP_EQ, 2);
  plugin_info_list_t *plist1 = (plugin_info_list_t*)smartlist_get(list_plugins, 0);
  plugin_info_list_t *plist2 = (plugin_info_list_t*)smartlist_get(list_plugins, 1);
  printf("%s", plist1->pname); 
  tt_int_op(strcmp(plist1->pname, "test_1.plugin"), OP_EQ, 0); 
  tt_int_op(strcmp(plist2->pname, "test_2.plugin"), OP_EQ, 0); 
  tt_int_op(strcmp(((plugin_info_t*)smartlist_get(plist1->subplugins, 0))->subname, "test_1"), OP_EQ, 0);
  tt_int_op(strcmp(((plugin_info_t*)smartlist_get(plist2->subplugins, 0))->subname, "test_2"), OP_EQ, 0);
  tt_int_op(((plugin_info_t*)smartlist_get(plist1->subplugins, 0))->pfamily, OP_EQ,  PLUGIN_PROTOCOL_RELAY);
  tt_int_op(((plugin_info_t*)smartlist_get(plist1->subplugins, 0))->putype, OP_EQ,  PLUGIN_CODE_HIGHJACK);
  tt_int_op(((plugin_info_t*)smartlist_get(plist2->subplugins, 0))->putype, OP_EQ,  PLUGIN_CODE_ADD);
done:
  if(list_plugins)
    tor_free(list_plugins);
}

struct testcase_t plugin_tests[] = {
  {"plugin_helper_find_all_and_init",
   test_plugin_helper_find_all_and_init, TT_FORK, NULL, NULL},

  END_OF_TESTCASES
};
