/**
 * \file plugin_helper.h
 *
 * \brief helps on system tasks: reading a plugin from the disk,
 * 
 * future:
 *  asks to the directories new dev plugins 
 **/
#include "orconfig.h"
#include "core/or/or.h"
#include "app/config/config.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "src/lib/string/printf.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <libgen.h>
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <process.h>
#include <tchar.h>
#include <winbase.h>
#else /* !(defined(_WIN32)) */
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#endif /* defined(_WIN32) */

int insert_plugin_from_transaction_line(char *line, char *plugin_dirname,
    plugin_info_t *pinfo) {
  /* Part one: extract name */
  char *token = strsep(&line, " ");
  if (!token) {
    log_debug(LD_PLUGIN, "No token for name extracted!");
    return -1;
  }
  pinfo->subname = tor_strdup(token);
  /* Part one bis: extract param, if any */
  token = strsep(&line, " ");

  if (!token) {
    log_debug(LD_PLUGIN, "No family!");
    return -1;
  }
  /** plugin type */
  if (strncmp(token, "protocol_relay", 8) == 0) {
    pinfo->pfamily = PLUGIN_PROTOCOL_RELAY;
    pinfo->ptype = PLUGIN_DEV;
  }
  else {
    log_debug(LD_PLUGIN, "Plugin family unsupported %s", token);
  }
  token = strsep(&line, " ");
  if (!token) {
    log_debug(LD_PLUGIN, "No keyword");
    return -1;
  }
  /** Param is optional */
  if (strncmp(token, "param", 5) == 0) {
    token = strsep(&line, " ");
    char *errmsg = NULL;
    pinfo->param = (int) strtoul(token, &errmsg, 0);
    if (errmsg != NULL && strncmp(errmsg, "", 1) != 0) {
      log_debug(LD_PLUGIN, "Invalid parameter %s, num is %u!", token, pinfo->param);
      return -1;
    }
    token = strsep(&line, " ");
  }
  
  /* Part two: extract plugin type */
  if (strncmp(token, "replace", 7) == 0) {
    pinfo->putype = PLUGIN_CODE_HIGHJACK;
  } else if (strncmp(token, "del", 3) == 0) {
    pinfo->putype = PLUGIN_CODE_DEL;
  } else if (strncmp(token, "add", 3) == 0) {
    pinfo->putype = PLUGIN_CODE_ADD;
  } else {
    log_debug(LD_PLUGIN, "Cannot extract the type of the plugin: %s\n", token);
    return -1;
  }

  /* Part three: insert the plugin */
  token = strsep(&line, " ");
  if (!token) {
    log_debug(LD_PLUGIN, "No token for ebpf filename extracted!");
    return -1;
  }
  /** TODO handle memory later */
  pinfo->memory_needed = 0;
  /* Handle end of line */
  token[strcspn(token, "\r\n")] = 0;

  char elfpath[MAX_PATH];
  strlcpy(elfpath, plugin_dirname, MAX_PATH);
  strcat(elfpath, "/");
  strcat(elfpath, token);
  return plugin_plug_elf(pinfo, elfpath);
}

plugin_info_list_t* plugin_insert_transaction(const char *plugin_filepath, const char *filename) {
  FILE *file = fopen(plugin_filepath, "r");

  if (!file) {
    log_debug(LD_PLUGIN, "Failed to open %s: %s\n", plugin_filepath,
        strerror(errno));
    return NULL;
  }

  char *line = NULL;
  size_t len = 0;
  ssize_t read = 0;
  bool ok = true;
  char *plugin_dirname = dirname(tor_strdup(plugin_filepath));
  plugin_info_list_t *list_plugins = tor_malloc_zero(sizeof(plugin_info_list_t));
  list_plugins->subplugins = smartlist_new();
  list_plugins->pname = tor_strdup(filename);
  while (ok && (read = getline(&line, &len, file)) != -1) {
    /* Skip blank lines */
    if (len <= 1) {
      continue;
    }
    plugin_info_t *pinfo = tor_malloc_zero(sizeof(plugin_info_t));
    ok = insert_plugin_from_transaction_line(line, plugin_dirname, pinfo);
    if (ok)
      smartlist_add(list_plugins->subplugins, pinfo);
  }

  if (!ok) {
    /* Unplug previously plugged code */
    plugin_unplug(list_plugins);
    /** TODO: check if we need a pinfo_free() function */
    SMARTLIST_FOREACH(list_plugins->subplugins, plugin_info_t *,
        pinfo, tor_free(pinfo));
  }
  if (line) {
    tor_free(line);
  }

  fclose(file);

  return ok ? list_plugins : NULL;
}

/**
 * Just look into the directory plugin and initialize
 * all of them 
 */

/**
 * TODO Make it Win32 compatible
 */
smartlist_t* plugin_helper_find_all_and_init(void) {
  tor_assert(get_options()->PluginsDirectory);

  /** Look inside all plugin directories */
  struct dirent *de;
  struct dirent *de2;
  DIR *dr = opendir(get_options()->PluginsDirectory);
  DIR *dr2;
  smartlist_t *all_plugins = smartlist_new();
  if (!dr) {
    log_debug(LD_PLUGIN, "PluginsDirectory option is null");
    return NULL;
  }

  while ((de = readdir(dr)) != NULL) {
    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
        continue;
    struct stat sb;
    char *plugin_dir = NULL;
    tor_asprintf(&plugin_dir, "%s"PATH_SEPARATOR"%s",
        get_options()->PluginsDirectory, de->d_name);
    int ret = stat(plugin_dir, &sb);
    if (ret == 0 && S_ISDIR(sb.st_mode)) {
      plugin_info_list_t *pinfo_list = NULL;
      dr2 = opendir(plugin_dir);
      /* find the .plugin file */
      while ((de2 = readdir(dr2)) != NULL &&
          strcmpend(de2->d_name, ".plugin") != 0){
      }
      if (de2){
        char *filepath = NULL;
        tor_asprintf(&filepath, "%s"PATH_SEPARATOR"%s",
            plugin_dir, de2->d_name);
        /*reading .plugin files and loader .o files */
        pinfo_list = plugin_insert_transaction(filepath, de2->d_name);
        if (pinfo_list) {
          smartlist_add(all_plugins, pinfo_list);
        }
      }

      closedir(dr2);
      tor_free(plugin_dir);
    }
    else {
      if (ret == -1) {
        log_debug(LD_PLUGIN, "Stat returned -1: %s; d_type: %d DT_DIR %d\n",
            strerror(errno), de->d_type, DT_DIR);
      }
    }
  }
  log_info(LD_PLUGIN, "Loaded all plugins");
  return all_plugins;
}

void pinfo_free(plugin_info_t *pinfo) {
  tor_free(pinfo->subname);
  tor_free(pinfo);
}

const char *plugin_caller_id_to_string(caller_id_t caller) {
  switch (caller) {
    case RELAY_REPLACE_PROCESS_EDGE_SENDME: return "circuit sending sendme cells";
    case RELAY_PROCESS_EDGE_UNKNOWN: return "host-code unknown new protocol feature";
    default:
      return "unsupported";
  }
}

