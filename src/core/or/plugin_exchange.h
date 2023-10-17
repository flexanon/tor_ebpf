//
// Created by jdejaegh on 16/08/23.
//

/**
 * \file plugin_exchange.h
 * \brief Header file for plugin_exchange.c.
 **/

#ifndef PLUGIN_EXCHANGE_H
#define PLUGIN_EXCHANGE_H
#include "core/or/channel.h"

void handle_plugin_transfer_cell(cell_t *cell, channel_t *chan);
void handle_plugin_transferred_cell(cell_t *cell, channel_t *chan);
void handle_plugin_request_cell(cell_t *cell, channel_t *chan);
void send_plugin_files(char *plugin_name, circid_t circ_id, channel_t *chan,
                       uint8_t command);
int handle_plugin_offer_in_create_cell(const uint8_t *required_plugins,
                                        or_circuit_t *circ, channel_t *chan);
int create_plugin_offer(cell_t *plugin_offer, circid_t circ_id);
uint16_t list_plugins_on_disk(uint8_t *list_out, uint16_t max_size);
void mark_plugin_as_received(char * plugin_name, circuit_t *circ);
smartlist_t * smart_list_plugins_in_payload(const uint8_t *payload);
smartlist_t * smart_list_plugins_on_disk(void);
void send_missing_plugins_to_peer(uint8_t *payload, circid_t circ_id,
                                  channel_t *chan);
void send_plugin(char *plugin_name, circid_t circ_id, channel_t *chan,
                 uint8_t command);
int is_str_in_smartlist(char * str, smartlist_t * list);
void free_smartlist_and_elements(smartlist_t * list);
void get_dir_name(const char * path, char * dir, int max_len);
void handle_plugin_transferred_back_cell(cell_t *cell, channel_t *chan);
#endif // PLUGIN_EXCHANGE_H
