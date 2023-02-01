

#ifndef TOR_SIGNALATTACK_H
#define TOR_SIGNALATTACK_H
#include "core/or/or.h"
#include <event2/event.h>
#define CHANNEL_OBJECT_PRIVATE //get some channel internal function
#include "core/or/channel.h"
#include "core/or/or_circuit_st.h"
#include "core/or/origin_circuit_st.h"
#include "core/or/circuit_st.h"
#include "core/or/channeltls.h"
#include "core/or/circuitlist.h"
#include "feature/nodelist/node_st.h"
#include "feature/nodelist/routerinfo_st.h"
#include "core/mainloop/connection.h"
#include "core/or/or_connection_st.h"
#include "core/or/relay.h"
#include "orconfig.h"
#include "app/config/config.h"
#include "feature/nodelist/nodelist.h"
#include "src/feature/relay/router.h"
#include "feature/nodelist/routerinfo.h"
#include "lib/evloop/compat_libevent.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include "core/or/signal_attack.h"
#include "lib/crypt_ops/crypto_rand.h"

#define BANDWIDTH_EFFICIENT 0
#define MIN_BLANK 1
#define SIMPLE_WATERMARK 2
#define SIGNAL_ATTACK_MAX_BLANK 2000

//void signal_encode_destination(char *address, circuit_t *circ);
void signal_encode_destination(void *param);

int signal_listen_and_decode(circuit_t *circ);

void signal_free(circuit_t *circ);
void signal_free_all(void);
void signal_send_delayed_destroy_cb(evutil_socket_t fd,
    short events, void *arg);

#ifdef TOR_SIGNALATTACK_PRIVATE
STATIC void signal_minimize_blank_latency_cb(evutil_socket_t fd,
    short events, void *arg);
STATIC int signal_minimize_blank_latency(char *address, circuit_t *circ);
STATIC void signal_bandwidth_efficient_cb(evutil_socket_t fd,
    short events, void *arg);
STATIC int signal_bandwidth_efficient(char *address, circuit_t *circuit);
STATIC void subip_to_subip_bin(uint8_t, char *subip_bin);
STATIC void signal_encode_simple_watermark(circuit_t *circuit);
STATIC int delta_timing(struct timespec *t1, struct timespec *t2);
#endif

typedef struct signal_decode_t {
  circid_t circid;
  struct timespec first;
  smartlist_t *timespec_list;
  struct timespec last;
  int disabled;
} signal_decode_t;

typedef struct signal_encode_param_t {
  char *address;
  circuit_t *circ;
} signal_encode_param_t;

typedef struct signal_encode_state_t {
  int nb_calls;
  circuit_t *circ;
  int subip[4];
  char *address;
  struct event *ev;
} signal_encode_state_t;


void signal_encode_state_free(signal_encode_state_t *state);

void signal_set(int key, va_list *arguments);

#endif
