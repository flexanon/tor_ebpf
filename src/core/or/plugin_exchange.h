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

void handle_plugin_transfer_cell(cell_t *cell);
void handle_plugin_transferred_cell(cell_t *cell, channel_t *chan);
void handle_plugin_request_cell(cell_t *cell, channel_t *chan);
void send_plugin_files(char *plugin_name, cell_t *cell, channel_t *chan);
int handle_plugin_offer_in_create_cell(const uint8_t *required_plugins,
                                        or_circuit_t *circ, channel_t *chan);
int create_plugin_offer(cell_t *plugin_offer, circid_t circ_id);
uint16_t list_plugins_on_disk(uint8_t *list_out, uint16_t max_size);
void mark_plugin_as_received(char * plugin_name, circuit_t *circ);
#endif // PLUGIN_EXCHANGE_H
