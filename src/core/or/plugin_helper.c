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
#include "ubpf/vm/plugin_memory.h"
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

/**
 * Read a line of the .plugin file.
 * We may expect this logic to _completely_ change in the future :)
 */

static int insert_plugin_from_transaction_line(char *line, char *plugin_dirname,
    entry_info_t *einfo, plugin_t *plugin) {
  /* Part one: extract name */
  char *token = strsep(&line, " ");
  if (!token) {
    log_debug(LD_PLUGIN, "No token for name extracted!");
    return -1;
  }
  einfo->entry_name = tor_strdup(token);
  /* Part one bis: extract param, if any */
  token = strsep(&line, " ");

  if (!token) {
    log_debug(LD_PLUGIN, "No family!");
    return -1;
  }
  /** plugin type */
  if (strncmp(token, "protocol_relay", 14) == 0) {
    einfo->pfamily = PLUGIN_PROTOCOL_RELAY;
    einfo->ptype = PLUGIN_DEV;
  }
  else if (strncmp(token, "protocol_circpad", 16) == 0) {
    einfo->pfamily = PLUGIN_PROTOCOL_CIRCPAD;
    einfo->ptype = PLUGIN_DEV;
  }
  else if (strncmp(token, "protocol_conn_edge", 18) == 0) {
    einfo->pfamily = PLUGIN_PROTOCOL_CONN_EDGE;
    einfo->ptype = PLUGIN_DEV;
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
    einfo->param = (int) strtoul(token, &errmsg, 0);
    if (errmsg != NULL && strncmp(errmsg, "", 1) != 0) {
      log_debug(LD_PLUGIN, "Invalid parameter %s, num is %u!", token, einfo->param);
      return -1;
    }
    token = strsep(&line, " ");
  }
  /* extract plugin type */
  if (strncmp(token, "replace", 7) == 0) {
    einfo->putype = PLUGIN_CODE_HIJACK;
  } else if (strncmp(token, "del", 3) == 0) {
    einfo->putype = PLUGIN_CODE_DEL;
  } else if (strncmp(token, "add", 3) == 0) {
    einfo->putype = PLUGIN_CODE_ADD;
  } else {
    log_debug(LD_PLUGIN, "Cannot extract the type of the plugin: %s\n", token);
    return -1;
  }

  /* insert the plugin */
  token = strsep(&line, " ");
  if (!token) {
    log_debug(LD_PLUGIN, "No token for ebpf filename extracted!");
    return -1;
  }
  /* Handle end of line */
  token[strcspn(token, "\r\n")] = 0;

  char elfpath[MAX_PATH];
  strlcpy(elfpath, plugin_dirname, MAX_PATH);
  strcat(elfpath, "/");
  strcat(elfpath, token);
  return plugin_plug_elf(plugin, einfo, elfpath);
}

/**
 * Read the .plugin file and load all .o objects 
 */

plugin_t* plugin_insert_transaction(const char *plugin_filepath, const char *filename) {
  FILE *file = fopen(plugin_filepath, "r");

  if (!file) {
    log_debug(LD_PLUGIN, "Failed to open %s: %s\n", plugin_filepath,
        strerror(errno));
    return NULL;
  }

  char *line = NULL;
  size_t len = 0;
  ssize_t ret = 0;
  bool ok = true;
  char *plugin_dirname = dirname(tor_strdup(plugin_filepath));
  /** Read plugin info first */
  int memory_needed = 0;
  char *line2 = NULL;
  if ((ret = getline(&line, &len, file)) != -1) {
    line2 = line;
    char *token = strsep(&line2, " ");
    if (!token) {
      log_debug(LD_PLUGIN, "No token for memory instracted?");
      ok = false;
    }
    if (strncmp(token, "memory", 6) == 0) {
      token = strsep(&line2, "\n");
      char *errmsg = NULL;
      /* can read hexa or base 10 */
      memory_needed = (int) strtoul(token, &errmsg, 0);
      if (errmsg != NULL && strncmp(errmsg, "", 1) != 0) {
        log_debug(LD_PLUGIN, "Invalid parameter %s, val is %d. Errmsg: %s", token, memory_needed, errmsg);
        ok = false;
      }
    }
    else {
      log_debug(LD_PLUGIN, "Expected 'memory' token, got %s", token);
      ok = false;
    }
  }
  else {
    ok = false;
  }
  /** we have the memory size; let's create the plugin */
  plugin_t *plugin = plugin_memory_init(memory_needed);
  plugin->entry_points = smartlist_new();
  plugin->pname = tor_strdup(filename);
  entry_info_t einfo;
  while (ok && (ret = getline(&line, &len, file)) != -1) {
    /* Skip blank lines */
    if (len <= 1) {
      continue;
    }
    memset(&einfo, 0, sizeof(einfo));
    /* insert_plugin_from_transaction return -1 on issue */
    if (insert_plugin_from_transaction_line(line, plugin_dirname, &einfo, plugin))
      ok = false;
  }

  if (!ok) {
    /* Unplug previously plugged code */
    plugin_unplug(plugin);
  }
  if (line) {
    tor_free(line);
  }

  fclose(file);

  return ok ? plugin : NULL;
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
      plugin_t *plugin = NULL;
      dr2 = opendir(plugin_dir);
      /* find the .plugin file */
      while ((de2 = readdir(dr2)) != NULL &&
          strcmpend(de2->d_name, ".plugin") != 0){
      }
      if (de2){
        char *filepath = NULL;
        tor_asprintf(&filepath, "%s"PATH_SEPARATOR"%s",
            plugin_dir, de2->d_name);
        /* reading .plugin files and loader .o files */
        plugin = plugin_insert_transaction(filepath, de2->d_name);
        if (plugin) {
          smartlist_add(all_plugins, plugin);
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

/**
 * Unplug the plugin -- i.e., free the map, destroy the ebpf vm and free the
 * plugin
 */

void plugin_unplug(plugin_t *plugin) {
}


const char *plugin_caller_id_to_string(caller_id_t caller) {
  switch (caller) {
    case RELAY_REPLACE_PROCESS_EDGE_SENDME: return "circuit sending sendme cells";
    case RELAY_REPLACE_STREAM_DATA_RECEIVED: return "stream has received data";
    case RELAY_PROCESS_EDGE_UNKNOWN: return "host-code unknown new protocol feature";
    case RELAY_RECEIVED_CONNECTED_CELL: return "plugin call when the Tor client received a Connected Cell";
    case RELAY_SENDME_CIRCUIT_DATA_RECEIVED: return "plugin called in the control-flow algs when circuit has received data";
    case CIRCPAD_PROTOCOL_INIT: return "initializing global circpad machines";
    case CIRCPAD_PROTOCOL_MACHINEINFO_SETUP: return "calling a plugin while setting up a machine to a circuit";
    case CIRCPAD_EVENT_CIRC_HAS_BUILT: return "calling a plugin in the circpad module when a circuit has built";
    case CIRCPAD_EVENT_CIRC_HAS_OPENED: return "calling a plugin in the circpad module when a circuit has opened";
    case CONNECTION_EDGE_ADD_TO_SENDING_BEGIN: return "calling a plugin after sending a begin_cell";
      
    default:
      return "unsupported";
  }
}

