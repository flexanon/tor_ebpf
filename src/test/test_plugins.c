
#include "core/or/or.h"
#include "core/or/plugin_helper.h"
#include "ubpf/vm/inc/ubpf.h"
#include "src/lib/string/printf.h"
#include "app/config/config.h"
#include "log_test_helpers.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
/*#include "plugin.h"*/

#include "test.h"

static smartlist_t *list_plugins = NULL;

static int dummy_load_elf_file(const char *code_filename, plugin_t *plugin, plugin_entry_point_t *entry_point) {
  (void) code_filename;
  (void) plugin;
  (void) entry_point;
  return 0;
}

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
  const char *plugin_dir_3 = "test_3";
  char *subpath_3 = get_plugindir_fname(plugin_dir_3);

  tt_assert(!check_or_create_plugin_subdir(plugin_dir_1));
  tt_assert(is_private_dir(subpath_1));
  tt_assert(!check_or_create_plugin_subdir(plugin_dir_2));
  tt_assert(is_private_dir(subpath_2));
  tt_assert(!check_or_create_plugin_subdir(plugin_dir_3));
  tt_assert(is_private_dir(subpath_3));
  const char* plugin_fname_1 = "test_1.plugin";
  const char* plugin_fname_2 = "test_2.plugin";
  const char* plugin_fname_3 = "test_3.plugin";

  const char* str_test_1 = "memory 256\ntest_1 protocol_relay replace test_1.o";
  const char* str_test_2 = "memory 256\ntest_2 protocol_relay param 42 add test_2.o\nother_test_2 protocol_relay replace other_test_2.o";
  const char* str_test_3 = "memory 256\ntest_3 protocol_circpad replace test_3.o";

  ret = write_to_plugin_subdir(plugin_dir_1, plugin_fname_1, str_test_1, NULL);
  tt_int_op(ret, OP_EQ, 0);
  ret = write_to_plugin_subdir(plugin_dir_1, "test_1.o", "", NULL);
  tt_int_op(ret, OP_EQ, 0);
  ret = write_to_plugin_subdir(plugin_dir_2, plugin_fname_2, str_test_2, NULL);
  tt_int_op(ret, OP_EQ, 0);
  ret = write_to_plugin_subdir(plugin_dir_2, "test_2.o", "", NULL);
  tt_int_op(ret, OP_EQ, 0);
  ret = write_to_plugin_subdir(plugin_dir_3, plugin_fname_3, str_test_3, NULL);
  tt_int_op(ret, OP_EQ, 0);
  ret = write_to_plugin_subdir(plugin_dir_3, "test_3.o", "", NULL);
  tt_int_op(ret, OP_EQ, 0);

  /** Try to initialize and load plugins */
  list_plugins = smartlist_new();
  MOCK(load_elf_file, dummy_load_elf_file);
  list_plugins = plugin_helper_find_all_and_init();
  UNMOCK(load_elf_file);
  tt_assert(list_plugins);
  tt_int_op(smartlist_len(list_plugins), OP_EQ, 3);
  plugin_t *plugin3 = (plugin_t*)smartlist_get(list_plugins, 0);
  plugin_t *plugin2 = (plugin_t*)smartlist_get(list_plugins, 1);
  plugin_t *plugin1 = (plugin_t*)smartlist_get(list_plugins, 2);
  tt_int_op(strcmp(plugin1->pname, "test_1.plugin"), OP_EQ, 0);
  tt_int_op(strcmp(plugin2->pname, "test_2.plugin"), OP_EQ, 0);
  tt_int_op(strcmp(plugin3->pname, "test_3.plugin"), OP_EQ, 0);
  tt_assert(plugin1->entry_points);
  tt_int_op(smartlist_len(plugin1->entry_points), OP_EQ, 1);
  tt_int_op(smartlist_len(plugin2->entry_points), OP_EQ, 2);

  tt_int_op(strcmp(((plugin_entry_point_t*)smartlist_get(plugin1->entry_points, 0))->entry_name, "test_1"), OP_EQ, 0);
  tt_int_op(plugin1->memory_size, OP_EQ, 256);
  tt_int_op(plugin2->memory_size, OP_EQ, 256);
  tt_int_op(plugin3->memory_size, OP_EQ, 256);
  /** Let's find entry points in our map */
  entry_point_map_t emap;
  memset(&emap, 0, sizeof(emap));
  emap.ptype = PLUGIN_DEV;
  emap.putype = PLUGIN_CODE_HIJACK;
  emap.pfamily = PLUGIN_PROTOCOL_RELAY;
  emap.entry_name = (char *) "test_1";
  entry_point_map_t *found;
  found = plugin_get(&emap);
  tt_assert(found->plugin == plugin1);
  tt_int_op(smartlist_len(found->plugin->entry_points), OP_EQ, 1);
  memset(&emap, 0, sizeof(emap));
  emap.ptype = PLUGIN_DEV;
  emap.putype = PLUGIN_CODE_HIJACK;
  emap.pfamily = PLUGIN_PROTOCOL_RELAY;
  emap.entry_name = (char *) "other_test_2";
  found = plugin_get(&emap);
  tt_assert(found->plugin == plugin2);
  tt_int_op(smartlist_len(found->plugin->entry_points), OP_EQ, 2);
  tt_int_op(strcmp(found->entry_point->entry_name, "other_test_2"), OP_EQ, 0);

  tt_int_op(strcmp(((plugin_entry_point_t*)smartlist_get(plugin2->entry_points, 0))->entry_name, "test_2"), OP_EQ, 0);
  tt_int_op(strcmp(((plugin_entry_point_t*)smartlist_get(plugin3->entry_points, 0))->entry_name, "test_3"), OP_EQ, 0);
done:
  if(list_plugins)
    tor_free(list_plugins);
}

struct testcase_t plugin_tests[] = {
  {"plugin_helper_find_all_and_init",
   test_plugin_helper_find_all_and_init, TT_FORK, NULL, NULL},

  END_OF_TESTCASES
};
