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
void handle_plugin_transferred_cell(cell_t *cell);
void handle_plugin_request_cell(cell_t *cell, channel_t *chan);
void send_plugin_files(char *plugin_name, cell_t *cell, channel_t *chan);
void handle_plugin_offer_cell(cell_t *cell, channel_t *chan);
int create_plugin_offer(cell_t *plugin_offer, circid_t circ_id);
#endif // PLUGIN_EXCHANGE_H
