/**
 * \file plugin_helper.h
 * \brief Header file for plugin_helper.c
 */

#ifndef PLUGIN_HELPER_H
#define PLUGIN_HELPER_H

#include "or.h"
#include "plugin.h"

#define MAX_PATH 255
plugin_t* plugin_insert_transaction(const char *plugin_fname, const char *filename, uint64_t looking_for_uid);

smartlist_t* plugin_helper_find_all_and_init(uint64_t* uids, uint16_t uids_len);

const char *plugin_caller_id_to_string(caller_id_t  caller);

plugin_t* plugin_helper_find_from_uid(uint64_t uid);

void plugin_unplug(plugin_t *plugin);
#endif
