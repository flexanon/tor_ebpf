//
// Created by jdejaegh on 16/08/23.
//

/**
 * \file plugin_exchange.c
 * \brief Functions for exchanging plugins between peers.
 * 
 * The protocol starts when a circuit a CREATE cell is sent.
 * The origin sends a CREATE cell with a list of its available plugins.
 * When a CREATE cell arrives, a node can choose to request one or more
 * plugins by listing their names in a PLUGIN_REQUEST cell.
 * 
 * Upon reception of a PLUGIN_REQUEST cell, a node starts sending the plugins
 * listed in the cell payload with PLUGIN_TRANSFER cells.  Each PLUGIN_TRANSFER
 * cell contain a chunk of a given plugin file. 
 * Once all the plugin files are transferred, the node send a PLUGIN_TRANSFERRED
 * to let the peer know that the plugin has been completely transferred.
 *
 * If a node receives a CREATE cell that does not list all of the plugins it
 * knows, the node can start sending back the plugins missing from the peer.
 */

#include "core/or/or.h"
#include "core/or/var_cell_st.h"
#include "core/or/origin_circuit_st.h"
#include "core/or/or_circuit_st.h"
#include "core/or/cell_st.h"
#include "core/or/relay.h"
#include "core/or/onion.h"
#include "core/or/connection_or.h"
#include "core/or/circuitlist.h"
#include "core/or/channel.h"
#include "core/mainloop/connection.h"
#include "app/config/config.h"
#include "core/or/command.h"

#include <linux/limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include "core/or/plugin_exchange.h"

/**
 * Write the file contents to disk when receiving the cell.
 * If the cell is a TRANSFER_BACK, forward it
 * @param cell TRANSFER or TRANSFER_BACK cell
 * @param chan channel on which the cell arrived
 */
void
handle_plugin_transfer_cell(cell_t *cell, channel_t *chan)
{
  int len_name;
  int len_data;
  char file_name[CELL_PAYLOAD_SIZE];
  memset(file_name, 0, CELL_PAYLOAD_SIZE);
  uint8_t file_data[CELL_PAYLOAD_SIZE];
  memset(file_data, 0, CELL_PAYLOAD_SIZE);
  int idx = 0;
  char absolute_file_name[PATH_MAX];
  char absolute_dir_name[PATH_MAX];
  memset(absolute_file_name, 0, PATH_MAX);
  memset(absolute_dir_name, 0, PATH_MAX);

  memcpy(&len_name, &cell->payload[idx], sizeof (len_name));
  idx += sizeof (len_name);
  memcpy(file_name, &cell->payload[idx], len_name);
  idx += len_name;
  memcpy(&len_data, &cell->payload[idx], sizeof (len_data));
  idx += sizeof (len_data);

  // If transfer back
  if (cell->command == CELL_PLUGIN_TRANSFER_BACK) {
    log_debug(LD_PLUGIN_EXCHANGE, "cell->command == CELL_PLUGIN_TRANSFER_BACK");
    char dir[PATH_MAX];
    strcat(absolute_dir_name, get_options()->PluginsDirectory);
    strcat(absolute_dir_name, "/.");
    get_dir_name(file_name, dir, CELL_PAYLOAD_SIZE);
    strcat(absolute_dir_name, dir);
    log_debug(LD_PLUGIN_EXCHANGE, "Will check for %s", absolute_dir_name);
    struct stat st = {0};
    if (stat(absolute_dir_name, &st) == -1) {
      log_debug(LD_PLUGIN_EXCHANGE, "Creating plugin for TRANSFER_BACK: .%s",
                dir);
      mkdir(absolute_dir_name, 0700);
    }

    circuit_t *circ;
    log_debug(LD_PLUGIN_EXCHANGE, "Locating circ with circ_id %u",
              cell->circ_id);
    circ = circuit_get_by_circid_channel(cell->circ_id, chan);
    if (circ == NULL) {
      log_warn(LD_PLUGIN_EXCHANGE, "circ is NULL");
    }

    if (!CIRCUIT_IS_ORIGIN(circ)) {
      or_circuit_t * or_circ;
      or_circ = TO_OR_CIRCUIT(circ);
      log_debug(LD_PLUGIN_EXCHANGE, "Will pass on TRANSFER_BACK, got it on circ"
                                    " %u and chan %lu, will pass on circ %u",
                cell->circ_id, chan->global_identifier, or_circ->p_circ_id);
      cell->circ_id = or_circ->p_circ_id;

      circ = circuit_get_by_circid_channel(cell->circ_id, or_circ->p_chan);
      if (circ == NULL)
        log_warn(LD_PLUGIN_EXCHANGE, "circ == NULL!");

      log_debug(LD_PLUGIN_EXCHANGE, "Sending cell to circ %u chan %lu",
                cell->circ_id, or_circ->p_chan->global_identifier);

      append_cell_to_circuit_queue(circ, or_circ->p_chan, cell, CELL_DIRECTION_IN, 0);
    } else {
      log_debug(LD_PLUGIN_EXCHANGE, "I am ORIGIN, not transferring back any further.");
    }
  }

  // Open file in append mode (created if it does not exist)
  strcat(absolute_file_name, get_options()->PluginsDirectory);
  strcat(absolute_file_name, "/.");
  strcat(absolute_file_name, file_name);

  FILE *fptr = fopen(absolute_file_name, "a");
  unsigned long bytes_read;
  bytes_read = fwrite(&cell->payload[idx], 1, len_data, fptr);

  log_debug(LD_PLUGIN_EXCHANGE, "Wrote %lu bytes to %s", bytes_read, file_name);

  fclose(fptr);
}

/**
 * From a string of the form 'directory/file.txt', extract 'directory'.
 * Only extract the right-most part (before /).  The input must contain a /
 * @param path input path of the form 'directory/file.ext'
 * @param dir output variable that will hold 'directory'
 * @param max_len maximum length to consider
 */
void get_dir_name(const char * path, char * dir, int max_len) {
  memset(dir, 0, PATH_MAX);
  int i = 0;
  max_len = PATH_MAX > max_len ? max_len : PATH_MAX;

  while(path[i] != '/' && i < max_len) {
    i++;
  }
  memcpy(dir, path, i);
}

/**
 * Move the plugin to its final folder once the transfer is done.
 * If no more plugins are awaited, finalise the circuit creation.
 * @param cell TRANSFERRED cell
 * @param chan channel on which the cell arrived
 */
void
handle_plugin_transferred_cell(cell_t *cell, channel_t *chan)
{
  char dir_name[PATH_MAX];
  char new_dir_name[PATH_MAX];

  circuit_t *circ;
  circ = circuit_get_by_circid_channel(cell->circ_id, chan);
  if (circ == NULL)
    log_warn(LD_PLUGIN_EXCHANGE, "circ == NULL!");

  if(circ->missing_plugins == NULL) {
    circ->missing_plugins = smartlist_new();
  }

  memset(dir_name, 0, PATH_MAX);
  strcat(dir_name, get_options()->PluginsDirectory);
  strcat(dir_name, "/.");
  strcat(dir_name, (char*) cell->payload);

  /* If we receive a TRANSFERRED cell for a directory that does not exist
   * it means that it is an empty directory plugin coming from a node located
   * close to the exit.
   * We have to handle this edge case */

  struct stat st = {0};
  if (stat(dir_name, &st) == -1) {
    log_debug(LD_PLUGIN_EXCHANGE, "Got TRANSFERRED cell for unknown: .%s -> "
                                  "creating directory", dir_name);
    mkdir(dir_name, 0700);
  }

  memset(new_dir_name, 0, PATH_MAX);
  strcat(new_dir_name, get_options()->PluginsDirectory);
  strcat(new_dir_name, "/");
  strcat(new_dir_name, (char*) cell->payload);

  rename(dir_name, new_dir_name);
  log_notice(LD_PLUGIN_EXCHANGE, "Node (%s) completely received the plugin: %s"
                                 " (circ_id %u)",
             get_options()->Nickname, cell->payload, cell->circ_id);

  int nb_missing_plugins = smartlist_len(circ->missing_plugins);
  int expecting_plugin = nb_missing_plugins > 0 ? 1 : 0;
  log_debug(LD_PLUGIN_EXCHANGE, "Missing %d plugin(s)", nb_missing_plugins);

  mark_plugin_as_received((char *) cell->payload, circ);

  nb_missing_plugins = smartlist_len(circ->missing_plugins);

  for (int i=0; i<nb_missing_plugins; i++)
    log_debug(LD_PLUGIN_EXCHANGE, "Still missing plugin: %s",
              (char*) smartlist_get(circ->missing_plugins, i));

  if(nb_missing_plugins == 0 && expecting_plugin == 1) {
    log_debug(LD_PLUGIN_EXCHANGE, "I should now process the CREATE cell with "
                                  "those: %s", circ->saved_create_cell->plugins);
    continue_process_create_cell(TO_OR_CIRCUIT(circ), circ->saved_create_cell);
  } else if (expecting_plugin == 0) {
    log_debug(LD_PLUGIN_EXCHANGE, "I was not expecting plugin %s, so not sending"
                                  " any CREATED", (char*) cell->payload);
  }
}

/**
 * Continue transferring the TRANSFERRED_BACK cell if needed.
 * Then, handle the TRANSFERRED cell
 * @param cell TRANSFERRED_BACK cell to handle
 * @param chan channel on which the cell arrived
 */
void
handle_plugin_transferred_back_cell(cell_t *cell, channel_t *chan){
  circuit_t *circ;
  circ = circuit_get_by_circid_channel(cell->circ_id, chan);
  if (circ == NULL)
    log_warn(LD_PLUGIN_EXCHANGE, "circ == NULL!");

  cell_t new_cell;
  memcpy(&new_cell, cell, sizeof (new_cell));

  if (!CIRCUIT_IS_ORIGIN(circ)) {
    or_circuit_t * or_circ;
    or_circ = TO_OR_CIRCUIT(circ);
    log_debug(LD_PLUGIN_EXCHANGE, "Will pass on TRANSFER_BACK, got it on circ "
                                  "%u and chan %lu, will pass on circ %u",
              new_cell.circ_id, chan->global_identifier, or_circ->p_circ_id);
    new_cell.circ_id = or_circ->p_circ_id;

    circ = circuit_get_by_circid_channel(new_cell.circ_id, or_circ->p_chan);
    if (circ == NULL)
      log_warn(LD_PLUGIN_EXCHANGE, "circ == NULL!");

    log_debug(LD_PLUGIN_EXCHANGE, "Sending cell to circ %u chan %lu",
              new_cell.circ_id, or_circ->p_chan->global_identifier);

    append_cell_to_circuit_queue(circ, or_circ->p_chan, &new_cell, CELL_DIRECTION_IN, 0);
  } else {
    log_debug(LD_PLUGIN_EXCHANGE, "I am ORIGIN, not transferring back any further.");
  }

  handle_plugin_transferred_cell(cell, chan);
}

/**
 * Remove the plugin from the missing_plugins list of the circuit once it is
 * received.
 * Given the name, iterate over the list of missing plugins and remove the
 * corresponding one.
 * @param plugin_name name of the plugin to remove from the list
 * @param circ circ holding the list of missing plugin to remove the plugin from
 */
void
mark_plugin_as_received(char * plugin_name, circuit_t *circ)
{
  if (circ == NULL || circ->missing_plugins == NULL) {
    log_debug(LD_PLUGIN_EXCHANGE, "No list of missing plugins for that circuit, skip.");
    return;
  }

  char * current_elm;
  int list_len = smartlist_len(circ->missing_plugins);
  log_debug(LD_PLUGIN_EXCHANGE, "Marking %s as received", plugin_name);

  for(int i = 0; i< list_len; i++) {
    current_elm = smartlist_get(circ->missing_plugins, i);

    if (strcmp(current_elm, plugin_name) == 0) {
      log_debug(LD_PLUGIN_EXCHANGE, "Remove from smartlist: %s", plugin_name);
      smartlist_remove(circ->missing_plugins, current_elm);
      free(current_elm);
      return;
    }
  }

  log_debug(LD_PLUGIN_EXCHANGE, "Plugin %s not found in list of required plugins"
                                " for the circuit", plugin_name);

}

/**
 * Based on the payload and the plugins available on disk, send the plugins that
 * are on disk but not listed in payload.
 * Use circ with circ_id and chan to send the plugins to the peer
 * @param payload payload containing the list of plugins
 * @param circ_id circuit to use to send the plugins
 * @param chan channel to use to send the plugins
 */
void
send_missing_plugins_to_peer(uint8_t *payload, circid_t circ_id, channel_t *chan)
{
  smartlist_t * on_disk = smart_list_plugins_on_disk();
  smartlist_t * in_create = smart_list_plugins_in_payload(payload);

  int nb_plugins_on_disk = smartlist_len(on_disk);
  char * current_plugin_name;

  for(int i = 0; i < nb_plugins_on_disk; i++) {
    current_plugin_name = smartlist_get(on_disk, i);
    if (!is_str_in_smartlist(current_plugin_name, in_create)) {
      log_debug(LD_PLUGIN_EXCHANGE, "%s not in_create, sending it to peer: circ"
                                    " %u, chan %lu",
                current_plugin_name, circ_id, chan->global_identifier);
      send_plugin(current_plugin_name, circ_id, chan, CELL_PLUGIN_TRANSFER_BACK);
    }
  }

  // Free smartlist_t
  free_smartlist_and_elements(on_disk);
  free_smartlist_and_elements(in_create);
}

/**
 * Pop and free all the elements of the list and free the list itself
 * @param list list to free
 */
void
free_smartlist_and_elements(smartlist_t * list) {
  if (list == NULL)
    return;

  void * elm = smartlist_pop_last(list);
  while (elm != NULL) {
    free(elm);
    elm = smartlist_pop_last(list);
  }
  smartlist_free_(list);
}

/**
 * Check if str is in list (strmcp wise)
 * @param str string to find
 * @param list list to search in
 * @return 1 if str is found in list, 0 otherwise
 */
int
is_str_in_smartlist(char * str, smartlist_t * list) {
  int list_len = smartlist_len(list);
  char * current_str;

  for(int i = 0; i < list_len; i++) {
    current_str = smartlist_get(list, i);
    if (strcmp(str, current_str) == 0)
      return 1;
  }

  return 0;
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
    send_plugin(plugin_name, cell->circ_id, chan, CELL_PLUGIN_TRANSFER);
  }
}

/**
 * Send the plugin files and the TRANSFERRED cell once all the files are sent
 * @param plugin_name name of the plugin to send
 * @param circ_id id of the circuit to use to send the plugin
 * @param chan channel to use to send the plugin
 * @param command how to send the plugin: CELL_PLUGIN_TRANSFER
 *   or CELL_PLUGIN_TRANSFER_BACK
 */
void
send_plugin(char *plugin_name, circid_t circ_id, channel_t *chan,
            uint8_t command)
{
  cell_t transferred_cell;
  memset(&transferred_cell, 0, sizeof(transferred_cell));
  transferred_cell.command = command == CELL_PLUGIN_TRANSFER ? CELL_PLUGIN_TRANSFERRED : CELL_PLUGIN_TRANSFERRED_BACK;
  transferred_cell.circ_id = circ_id;

  circuit_t *circ;
  circ = circuit_get_by_circid_channel(circ_id, chan);
  cell_direction_t direction = circ->n_chan == chan ? CELL_DIRECTION_OUT : CELL_DIRECTION_IN;

  // iterate over the files to send them
  log_notice(
      LD_PLUGIN_EXCHANGE, "Node (%s) starting to send plugin: %s (circ_id %u)",
      get_options()->Nickname, plugin_name, circ_id);
  send_plugin_files(plugin_name, circ_id, chan, command);

  // Notify peer once the plugin is transferred
  memcpy(transferred_cell.payload, plugin_name, CELL_PAYLOAD_SIZE);
  log_notice(LD_PLUGIN_EXCHANGE, "Node (%s) plugin sent: %s (circ_id %u)",
             get_options()->Nickname, plugin_name, circ_id);
  append_cell_to_circuit_queue(circ, chan, &transferred_cell, direction, 0);
}

/**
 * Send the files for a given plugin.
 * The files in the folder corresponding to plugin_name are listed and
 * sent in CELL_PLUGIN_TRANSFER cells in response to cell on chan
 */
void
send_plugin_files(char *plugin_name, circid_t circ_id, channel_t *chan,
                  uint8_t command)
{
  log_debug(LD_PLUGIN_EXCHANGE, "Send plugin files for %s", plugin_name);
  struct dirent *de;
  unsigned long bytes_read;
  uint8_t relative_file_name[CELL_PAYLOAD_SIZE];
  int payload_idx;
  int len;

  cell_t transfer_cell;
  memset(&transfer_cell, 0, sizeof(transfer_cell));
  transfer_cell.command = command;
  transfer_cell.circ_id = circ_id;

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

    memcpy(&transfer_cell.payload[payload_idx], relative_file_name,
           strlen((char*)relative_file_name));
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
      log_debug(LD_PLUGIN_EXCHANGE, "Sending PLUGIN_TRANSFER cell (circID %u):"
                                    " %lu bytes of %s",
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
 * Return the number of missing plugins
 */
int
handle_plugin_offer_in_create_cell(const uint8_t *required_plugins,
                                   or_circuit_t *circ, channel_t *chan)
{

  if (required_plugins[0] == 0) {
    log_debug(LD_PLUGIN_EXCHANGE,
               "Received empty list of plugin in CREATE cell (circ_id %u)",
               circ->p_circ_id);
    return 0;
  }

  tor_assert(get_options()->PluginsDirectory);
  struct dirent *de;
  cell_t request_cell;
  memset(&request_cell, 0, sizeof(request_cell));
  circ->base_.missing_plugins = smartlist_new();

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
    while (required_plugins[payload_idx] != '\n' && !last) {
      if (required_plugins[payload_idx] == 0) {
        last = 1;
      } else {
        plugin_name[p_name_idx] = required_plugins[payload_idx];
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
        log_debug(LD_PLUGIN_EXCHANGE, "Already have this: %s (circ_id %u)",
                  plugin_name, circ->p_circ_id);
      } else {
        memset(dir, 0, PATH_MAX);
        strcat(dir, ".");
        strcat(dir, plugin_name);
        if (strcmp(dir, de->d_name) == 0) {
          found = 1;
          log_debug(LD_PLUGIN_EXCHANGE, "Plugin has already been requested: .%s"
                                        " (circ_id %u)",
                    plugin_name, circ->p_circ_id);
        }
      }
    }
    closedir(dr);

    // No need to check for cell overflow capacity as the request is at most
    // as long as the offer (cannot request more than what is offered)
    if (found == 0) {
      log_debug(LD_PLUGIN_EXCHANGE, "Will request plugin: %s (circ_id %u)",
                plugin_name, circ->p_circ_id);
      will_request = 1;

      // Put element in circuit info
      smartlist_add_strdup(circ->base_.missing_plugins, plugin_name);

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
    request_cell.circ_id = circ->p_circ_id;
    request_cell.command = CELL_PLUGIN_REQUEST;

    cell_direction_t direction = circ->base_.n_chan == chan ? CELL_DIRECTION_OUT : CELL_DIRECTION_IN;

    log_debug(LD_PLUGIN_EXCHANGE, "Sending PLUGIN REQUEST cell (circ_id %u): %s",
              request_cell.circ_id, request_cell.payload);

    append_cell_to_circuit_queue(TO_CIRCUIT(circ),
                                 circ->p_chan, &request_cell,
                                 direction, 0);
  }

  for (int i=0; i< smartlist_len(circ->base_.missing_plugins); i++)
    log_debug(LD_PLUGIN_EXCHANGE, "Missing plugin: %s",
              (char*) smartlist_get(circ->base_.missing_plugins, i));

  return smartlist_len(circ->base_.missing_plugins);
}

/**
 * Look into the plugin folder for plugins to offer to other relays.
 * Create null terminated CRLF separated list of plugin names in list_out
 *
 * LIMITATION: if there are more plugins than what can fit in a cell, some
 * are just ignored
 *
 * @param list_out put the plugin list here
 * @param max_size number of bytes available to list the plugins
 * @return number of bytes used to list the plugins
 */
uint16_t
list_plugins_on_disk(uint8_t *list_out, uint16_t max_size) {
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

/**
 * List all the plugin names present on disk, in the plugin directory.
 * The result is a smartlist containing the names of the plugins on disk.
 * @return pointer to a smartlist containing the plugin names
 */
smartlist_t *
smart_list_plugins_on_disk(void) {
  tor_assert(get_options()->PluginsDirectory);

  struct dirent *de;
  smartlist_t * list = smartlist_new();

  DIR *dr = opendir(get_options()->PluginsDirectory);
  while ((de = readdir(dr)) != NULL) {
    if (de->d_name[0] == '.')
      continue;
    smartlist_add_strdup(list, de->d_name);
  }
  closedir(dr);

  for(int j = 0; j < smartlist_len(list); j++)
    log_debug(LD_PLUGIN_EXCHANGE, "Plugin on disk: %s", (char*) smartlist_get(list, j));

  return list;
}

/**
 * List all the plugin names contained in payload.  The result is a smartlist
 * containing the names of the plugins in the payload, in the same order.
 * @param payload cell payload containing a null terminated CRLF separated list
 *   of plugin names
 * @return pointer to a smartlist containing the plugin names
 */
smartlist_t *
smart_list_plugins_in_payload(const uint8_t *payload) {
  smartlist_t * list = smartlist_new();
  char plugin_name[PATH_MAX];

  int i = 0;
  int start_idx = i;

  while(payload[i] != 0) {
    if (payload[i] == '\n') {
      memset(plugin_name, 0, PATH_MAX);
      memcpy(plugin_name, payload + start_idx, i-start_idx);
      smartlist_add_strdup(list, plugin_name);
      start_idx = i+1;
    }
    i++;
  }

  if (start_idx < i) {
    memset(plugin_name, 0, PATH_MAX);
    memcpy(plugin_name, payload + start_idx, i-start_idx);
    smartlist_add_strdup(list, plugin_name);
  }

  for(int j = 0; j < smartlist_len(list); j++)
    log_debug(LD_PLUGIN_EXCHANGE, "Plugin in payload: %s", (char*) smartlist_get(list, j));
  return list;
}