//
// Created by jdejaegh on 16/08/23.
//

/**
 * \file plugin_exchange.c
 * \brief Functions for exchanging plugins between peers.
 * 
 * The protocol starts when a circuit is created (upon reception or emission
 * of a CREATED cell). 
 * Both peer send a PLUGIN_OFFER cell with a list of their available plugins.
 * When a PLUGIN_OFFER cell arrives, a node can choose to request one or more
 * plugins by listing their names in a PLUGIN_REQUEST cell.
 * 
 * Upon reception of a PLUGIN_REQUEST cell, a node starts sending the plugins
 * listed in the cell payload with PLUGIN_TRANSFER cells.  Each PLUGIN_TRANSFER
 * cell contain a chunk of a given plugin file. 
 * Once all the plugin files are transferred, the node send a PLUGIN_TRANSFERRED
 * to let the peer know that the plugin has been completely transferred.
 */

#include "core/or/or.h"
#include "core/or/var_cell_st.h"
#include "core/or/origin_circuit_st.h"
#include "core/or/cell_st.h"
#include "core/or/relay.h"
#include "core/or/onion.h"
#include "core/or/connection_or.h"
#include "core/or/circuitlist.h"
#include "core/or/channel.h"
#include "core/mainloop/connection.h"
#include "app/config/config.h"

#include <linux/limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include "core/or/plugin_exchange.h"

void
handle_plugin_transfer_cell(cell_t *cell)
{
  int len_name;
  int len_data;
  char file_name[CELL_PAYLOAD_SIZE];
  memset(file_name, 0, CELL_PAYLOAD_SIZE);
  uint8_t file_data[CELL_PAYLOAD_SIZE];
  memset(file_data, 0, CELL_PAYLOAD_SIZE);
  int idx = 0;
  char absolute_file_name[PATH_MAX];
  memset(absolute_file_name, 0, PATH_MAX);

  memcpy(&len_name, &cell->payload[idx], sizeof (len_name));
  idx += sizeof (len_name);
  memcpy(file_name, &cell->payload[idx], len_name);
  idx += len_name;
  memcpy(&len_data, &cell->payload[idx], sizeof (len_data));
  idx += sizeof (len_data);


  // Open file in append mode (created if it does not exist)
  strcat(absolute_file_name, get_options()->PluginsDirectory);
  strcat(absolute_file_name, "/.");
  strcat(absolute_file_name, file_name);

  log_debug(LD_PLUGIN_EXCHANGE, "About to write %d bytes to %s", len_data, file_name);

  FILE *fptr = fopen(absolute_file_name, "a");
  unsigned long bytes_read;
  bytes_read = fwrite(&cell->payload[idx], 1, len_data, fptr);

  log_debug(LD_PLUGIN_EXCHANGE, "Wrote %lu bytes to %s", bytes_read, file_name);

  fclose(fptr);
}

/**
 * Move the plugin to its final folder once the transfer is done
 */
void
handle_plugin_transferred_cell(cell_t *cell)
{
  char dir_name[PATH_MAX];
  char new_dir_name[PATH_MAX];

  memset(dir_name, 0, PATH_MAX);
  strcat(dir_name, get_options()->PluginsDirectory);
  strcat(dir_name, "/.");
  strcat(dir_name, (char*) cell->payload);

  memset(new_dir_name, 0, PATH_MAX);
  strcat(new_dir_name, get_options()->PluginsDirectory);
  strcat(new_dir_name, "/");
  strcat(new_dir_name, (char*) cell->payload);

  rename(dir_name, new_dir_name);
  log_notice(LD_PLUGIN_EXCHANGE, "Node (%s) completely received the plugin: %s (circ_id %u)",
             get_options()->Nickname, cell->payload, cell->circ_id);
}

/**
 * Send the plugins listed in the cell payload.
 * Once the a given plugin is completely sent, a CELL_PLUGIN_TRANSFERRED
 * is sent to acknowledge the end of the plugin transfer.
 */
void
handle_plugin_request_cell(cell_t *cell, channel_t *chan)
{
  int last = 0;
  char plugin_name[CELL_PAYLOAD_SIZE];
  int p_name_idx;
  int payload_idx = 0;

  cell_t transferred_cell;
  memset(&transferred_cell, 0, sizeof(transferred_cell));
  transferred_cell.command = CELL_PLUGIN_TRANSFERRED;
  transferred_cell.circ_id = cell->circ_id;

  circuit_t *circ;
  circ = circuit_get_by_circid_channel(transferred_cell.circ_id, chan);
  cell_direction_t direction = circ->n_chan == chan ? CELL_DIRECTION_OUT : CELL_DIRECTION_IN;

  while (!last) {
    // get plugin name
    memset(plugin_name, 0, CELL_PAYLOAD_SIZE);
    p_name_idx = 0;
    while (cell->payload[payload_idx] != '\n' && !last) {
      if (cell->payload[payload_idx] == 0) {
        last = 1;
      } else {
        plugin_name[p_name_idx] = cell->payload[payload_idx];
        p_name_idx++;
        payload_idx++;
      }
    }
    payload_idx++;

    // iterate over the files to send them
    log_notice(LD_PLUGIN_EXCHANGE, "Node (%s) starting to send plugin: %s (circ_id %u)",
               get_options()->Nickname, plugin_name, transferred_cell.circ_id);
    send_plugin_files(plugin_name, cell, chan);
    
    // Notify peer once the plugin is transferred
    memcpy(transferred_cell.payload, plugin_name, CELL_PAYLOAD_SIZE);
    log_notice(LD_PLUGIN_EXCHANGE, "Node (%s) plugin sent: %s (circ_id %u)",
               get_options()->Nickname, plugin_name, transferred_cell.circ_id);
    append_cell_to_circuit_queue(circ, chan, &transferred_cell,
                                 direction, 0);

  }
}

/**
 * Send the files for a given plugin.
 * The files in the folder corresponding to plugin_name are listed and
 * sent in CELL_PLUGIN_TRANSFER cells in response to cell on chan
 */
void
send_plugin_files(char *plugin_name, cell_t *cell, channel_t *chan)
{
  log_debug(LD_PLUGIN_EXCHANGE, "Send plugin files for %s", plugin_name);
  struct dirent *de;
  unsigned long bytes_read;
  uint8_t relative_file_name[CELL_PAYLOAD_SIZE];
  int payload_idx;
  int len;

  cell_t transfer_cell;
  memset(&transfer_cell, 0, sizeof(transfer_cell));
  transfer_cell.command = CELL_PLUGIN_TRANSFER;
  transfer_cell.circ_id = cell->circ_id;

  circuit_t *circ;
  circ = circuit_get_by_circid_channel(transfer_cell.circ_id, chan);
  cell_direction_t direction = circ->n_chan == chan ? CELL_DIRECTION_OUT : CELL_DIRECTION_IN;

  char dir_name[PATH_MAX];
  memset(dir_name, 0, PATH_MAX);
  strcat(dir_name, get_options()->PluginsDirectory);
  strcat(dir_name, "/");
  strcat(dir_name, plugin_name);

  char absolute_file_name[PATH_MAX];

  // List all the files in the directory of the plugin
  FILE *fptr;
  DIR *dr = opendir(dir_name);
  while ((de = readdir(dr)) != NULL) {

    if (!strcmp(de->d_name, ".") ||
        !strcmp(de->d_name, "..")) {
      continue;
    }

    memset(relative_file_name, 0, CELL_PAYLOAD_SIZE);
    strcat((char *)relative_file_name, plugin_name);
    strcat((char *)relative_file_name, "/");
    strcat((char *)relative_file_name, de->d_name);
    log_debug(LD_PLUGIN_EXCHANGE, "Sending file: %s", relative_file_name);

    payload_idx = 0;

    len = (int) strlen((char*)relative_file_name);
    memcpy(&transfer_cell.payload[payload_idx], &len,
           sizeof(len));
    payload_idx += sizeof(int);

    memcpy(&transfer_cell.payload[payload_idx], relative_file_name, strlen((char*)relative_file_name));
    payload_idx += (int) strlen((char*)relative_file_name);


    unsigned long remaining_space = CELL_PAYLOAD_SIZE-payload_idx-sizeof(int);

    memset(absolute_file_name, 0, PATH_MAX);
    strcat(absolute_file_name, dir_name);
    strcat(absolute_file_name, "/");
    strcat(absolute_file_name, de->d_name);

    fptr = fopen(absolute_file_name, "rb");

    // As long as we can read the maximum from the file, continue to send
    // cells with content of the file
    do {
      bytes_read = fread(&transfer_cell.payload[payload_idx + sizeof(int)], 1,
                         remaining_space, fptr);

      len = (int)bytes_read;
      memcpy(&transfer_cell.payload[payload_idx], &len, sizeof(len));

      // Actually send the cell here and zero what is needed for next iteration
      log_debug(LD_PLUGIN_EXCHANGE, "Sending PLUGIN_TRANSFER cell (circID: %u): %lu bytes of %s",
                transfer_cell.circ_id, bytes_read, relative_file_name);
      append_cell_to_circuit_queue(circ,
                                   chan, &transfer_cell,
                                   direction, 0);

      memset(&transfer_cell.payload[payload_idx], 0, remaining_space);

    } while (bytes_read == remaining_space);

    fclose(fptr);
    memset(transfer_cell.payload, 0, CELL_PAYLOAD_SIZE);

  }
}

/**
 * Receives a plugin offer and look which plugins are missing from the
 * directory.
 * Sends back a PLUGIN_REQUEST cell for the missing plugin
 */
void
handle_plugin_offer_cell(cell_t *cell, channel_t *chan)
{

  if (cell->payload[0] == 0) {
    log_notice(LD_PLUGIN_EXCHANGE,
               "Received empty plugin offer, quite useless you know (circ_id %u)",
               cell->circ_id);
    return;
  }

  tor_assert(get_options()->PluginsDirectory);
  struct dirent *de;
  cell_t request_cell;
  memset(&request_cell, 0, sizeof(request_cell));

  char plugin_name[CELL_PAYLOAD_SIZE-2];
  int found;
  int p_name_idx;
  int payload_idx = 0;
  unsigned long request_payload_idx = 0;
  int last = 0;
  int will_request = 0;
  char dir[PATH_MAX];

  while (!last) {
    memset(plugin_name, 0, CELL_PAYLOAD_SIZE - 2);
    p_name_idx = 0;
    while (cell->payload[payload_idx] != '\n' && !last) {
      if (cell->payload[payload_idx] == 0) {
        last = 1;
      } else {
        plugin_name[p_name_idx] = cell->payload[payload_idx];
        p_name_idx++;
        payload_idx++;
      }
    }
    payload_idx++;

    DIR *dr = opendir(get_options()->PluginsDirectory);
    found = 0;
    while (((de = readdir(dr)) != NULL)) {
      if (strcmp(plugin_name, de->d_name) == 0) {
        found = 1;
        log_debug(LD_PLUGIN_EXCHANGE, "Already have this: %s (circ_id %u)", plugin_name, cell->circ_id);
      } else {
        memset(dir, 0, PATH_MAX);
        strcat(dir, ".");
        strcat(dir, plugin_name);
        if (strcmp(dir, de->d_name) == 0) {
          found = 1;
          log_debug(LD_PLUGIN_EXCHANGE, "Plugin has already been requested: .%s (circ_id %u)", plugin_name, cell->circ_id);
        }
      }
    }
    closedir(dr);

    // No need to check for cell overflow capacity as the request is at most
    // as long as the offer (cannot request more than what is offered)
    if (found == 0) {
      log_debug(LD_PLUGIN_EXCHANGE, "Will request plugin: %s (circ_id %u)", plugin_name, cell->circ_id);
      will_request = 1;
      memcpy(&request_cell.payload[request_payload_idx],
             plugin_name, strlen(plugin_name));
      request_payload_idx += strlen(plugin_name);
      request_cell.payload[request_payload_idx] = '\n';
      request_payload_idx ++;

      memset(dir, 0, PATH_MAX);
      strcat(dir, get_options()->PluginsDirectory);
      strcat(dir, "/.");
      strcat(dir, plugin_name);
      log_debug(LD_PLUGIN_EXCHANGE, "Creating directory: %s", dir);
      mkdir(dir, 0700);
    }
  }
  request_payload_idx = request_payload_idx > 0 ? request_payload_idx-1 : 0;
  request_cell.payload[request_payload_idx] = 0;

  if (will_request) {
    request_cell.circ_id = cell->circ_id;
    request_cell.command = CELL_PLUGIN_REQUEST;

    circuit_t *circ;
    circ = circuit_get_by_circid_channel(request_cell.circ_id, chan);

    cell_direction_t direction = circ->n_chan == chan ? CELL_DIRECTION_OUT : CELL_DIRECTION_IN;

    log_debug(LD_PLUGIN_EXCHANGE, "Sending PLUGIN REQUEST cell (circ_id: %u): %s",
              request_cell.circ_id, request_cell.payload);
    append_cell_to_circuit_queue(circ,
                                 chan, &request_cell,
                                 direction, 0);
  }
}

/**
 * Look into the plugin folder for plugins to offer to other relays.
 * Create a single cell with available plugins and place it in the
 * plugin_offer param
 *
 * LIMITATION: if there are more plugins than what can fit in a cell, some
 * are just ignored
 *
 * @return the number of plugin offered
 */
int
create_plugin_offer(cell_t *plugin_offer, circid_t circ_id)
{
  tor_assert(get_options()->PluginsDirectory);

  struct dirent *de;

  memset(plugin_offer, 0, sizeof(*plugin_offer));
  plugin_offer->command = CELL_PLUGIN_OFFER;
  plugin_offer->circ_id = circ_id;

  unsigned long space_left = CELL_PAYLOAD_SIZE-2;
  int idx = 0;
  int offered = 0;

  DIR *dr = opendir(get_options()->PluginsDirectory);
  while ((de = readdir(dr)) != NULL) {
    if (de->d_name[0] == '.')
      continue;
    offered ++;
    unsigned int i = 0;
    unsigned long len = strlen(de->d_name);

    if (space_left > len+1) {
      while (i < len) {
        plugin_offer->payload[idx] = de->d_name[i];
        i++;
        idx++;
      }
      plugin_offer->payload[idx] = '\n';
      idx++;
      space_left -= (len+1);
    }
  }
  idx = idx > 0 ? (idx-1) : 0;
  plugin_offer->payload[idx] = 0;

  closedir(dr);

  if (offered > 0)
    log_debug(LD_PLUGIN_EXCHANGE, "Offering: %s (circ_id %u)", plugin_offer->payload, plugin_offer->circ_id);
  else
    log_debug(LD_PLUGIN_EXCHANGE, "Offering nothing (circ_id %u)", plugin_offer->circ_id);

  return offered;
}


uint16_t list_plugins_on_disk(uint8_t *list_out, uint16_t max_size) {
  tor_assert(get_options()->PluginsDirectory);

  struct dirent *de;

  int idx = 0;
  int offered = 0;
  uint16_t space_left = max_size;

  DIR *dr = opendir(get_options()->PluginsDirectory);
  while ((de = readdir(dr)) != NULL) {
    if (de->d_name[0] == '.')
      continue;
    offered ++;
    unsigned int i = 0;
    unsigned long len = strlen(de->d_name);

    if (space_left > len+1) {
      while (i < len) {
        list_out[idx] = de->d_name[i];
        i++;
        idx++;
      }
      list_out[idx] = '\n';
      idx++;
      space_left -= (len+1);
    } else {
      log_warn(LD_PLUGIN_EXCHANGE, "Some plugin could not be included in CREATE cell");
    }
  }
  idx = idx > 0 ? (idx-1) : 0;
  list_out[idx] = 0;

  closedir(dr);
  if (offered > 0)
    log_debug(LD_PLUGIN_EXCHANGE, "Offering plugins (%d bytes): %s",
              max_size - space_left, list_out);
  else
    log_debug(LD_PLUGIN_EXCHANGE, "Offering nothing");

  return max_size - space_left;

}