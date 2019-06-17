/**
 * \file plugin_helper.h
 * \brief Header file for plugin_helper.c
 */

#ifndef PLUGIN_HELPER_H
#define PLUGIN_HELPER_H

#include "or.h"
#include "plugin.h"

#define MAX_PATH 255
plugin_info_list_t* plugin_insert_transaction(const char *plugin_fname, const char *filename);

int insert_plugin_from_transaction_line(char *line, char *plugin_dirname,
    plugin_info_t *pinfo);

smartlist_t* plugin_helper_find_all_and_init(void);

void pinfo_free(plugin_info_t* pinfo);

const char *plugin_caller_id_to_string(caller_id_t  caller);
#endif
