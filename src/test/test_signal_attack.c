#include "orconfig.h"
#include "test/test.h"
#define TOR_SIGNALATTACK_PRIVATE
#define CHANNEL_OBJECT_PRIVATE
#include <time.h>
#include "core/or/or.h"
#include "app/config/config.h"
#include "core/or/relay.h"
#include "core/or/signal_attack.h"
#include "core/or/circuitlist.h"
#include "core/mainloop/connection.h"

#include "test/fakechans.h"
#include <stdio.h>


#define ONE_OVER_10SIX 1E-6


/*static int mock_nbr_called = 0;*/

static int
connection_flush_mock(connection_t *conn) {
  (void) conn;
  return 0;
}

static ssize_t
channel_flush_some_cells_mock(channel_t *chan, ssize_t num_cells)
{
  (void) chan;
  (void) num_cells;
  return 0;
}
static int
mock_relay_send_command_from_edge(streamid_t stream_id, circuit_t *circ,
    uint8_t relay_command, const char *payload, size_t payload_len,
    crypt_path_t *cpath_layer, const char *filename, int lineno) {
  /*mock_nbr_called++;*/
  (void) stream_id;
  (void) circ;
  (void) relay_command;
  (void) payload;
  (void) payload_len;
  (void) cpath_layer;
  (void) filename;
  (void) lineno;
  return 0;
}

/*static int*/
/*mock_relay_send_command_from_edge_decode(streamid_t stream_id, circuit_t *circ,*/
    /*uint8_t relay_command, const char *payload, size_t payload_len,*/
    /*crypt_path_t *cpath_layer, const char *filename, int lineno) {*/
  /*mock_nbr_called++;*/
  /*(void) stream_id;*/
  /*(void) relay_command;*/
  /*(void) payload;*/
  /*(void) payload_len;*/
  /*(void) cpath_layer;*/
  /*(void) filename;*/
  /*(void) lineno;*/
  /*return signal_listen_and_decode(circ);*/
/*}*/
/*static channel_t **/
/*new_fake_channel(void)*/
/*{*/
  /*channel_t *chan = tor_malloc_zero(sizeof(channel_t));*/
  /*channel_init(chan);*/
  /*return chan;*/
/*}*/

static circuit_t *
fake_or_circuit_new(circid_t circ_id) {
  channel_t *chan = new_fake_channel();
  chan->magic = TLS_CHAN_MAGIC;
  or_circuit_t *circ = or_circuit_new(0, chan);
  circ->base_.n_circ_id = circ_id;
  return TO_CIRCUIT(circ);
}

static void
fake_circ_free(circuit_t *circ) {
  signal_free(circ);
  free_fake_channel(TO_OR_CIRCUIT(circ)->p_chan);
  smartlist_free(circ->plugins);
  tor_free(circ);
}

 /*This function test the time elapsed by the encoding of a message using the function */
 /*signal_minimize_blank_latency*/

/*static void*/
/*test_elapsed_signal_encoding() {*/
  /*struct timespec time_now, time_then;*/
  /*double elapsed_ms;*/
  /*char *addresses[] = {*/
    /*"172.124.243.122",*/
    /*"122.1.23.255",*/
    /*"32.32.32.32",*/
    /*"129.0.23.23",*/
  /*};*/
  /*int should_call[4] = {*/
    /*174+126+245+124,*/
    /*124+3+25+257,*/
    /*34+34+34+34,*/
    /*131+2+25+25,*/
  /*};*/
  /*int should_call_bw_efficient[4] = {*/
    /*3+2+3+2+3+3+2+2\*/
      /*+2+3+3+3+3+3+2+2\*/
      /*+3+3+3+3+2+2+3+3\*/
      /*+2+3+3+3+3+2+3+2,*/
    /*2+3+3+3+3+2+3+2\*/
      /*+2+2+2+2+2+2+2+3\*/
      /*+2+2+2+3+2+3+3+3\*/
      /*+3+3+3+3+3+3+3+3,*/
    /*2+2+3+2+2+2+2+2\*/
      /*+2+2+3+2+2+2+2+2\*/
      /*+2+2+3+2+2+2+2+2\*/
      /*+2+2+3+2+2+2+2+2,*/
    /*2+3+2+2+2+2+2+3\*/
      /*+2+2+2+2+2+2+2+2\*/
      /*+2+2+2+3+2+3+3+3\*/
      /*+2+2+2+3+2+3+3+3,*/
  /*};*/
  /*circuit_t *fake_circ = fake_or_circuit_new(42);*/
  /*[> Replace subcall of signal_minimize_blank_latency by a dummy function <]*/
  /*MOCK(relay_send_command_from_edge_, mock_relay_send_command_from_edge);*/
  /*for (int j = 0; j < 2; j++) {*/
    /*for (int i = 0; i < 4; i++) {*/
      /*char *address = tor_malloc(strlen(addresses[i]+1));*/
      /*strcpy(address, addresses[i]);*/
    /*//call signal encoding function*/
      /*clock_gettime(CLOCK_REALTIME, &time_now);*/
      /*int r;*/
      /*if (j == 0) {*/
        /*r = signal_minimize_blank_latency(address, fake_circ);*/
        /*tt_int_op(mock_nbr_called, ==, should_call[i]);*/
      /*}*/
      /*else {*/
        /*r = signal_bandwidth_efficient(address, fake_circ);*/
        /*tt_int_op(mock_nbr_called, ==, should_call_bw_efficient[i]);*/
      /*}*/
      /*mock_nbr_called = 0;*/
      /*tt_int_op(r, ==, 0);*/
      /*clock_gettime(CLOCK_REALTIME, &time_then);*/
      /*elapsed_ms = (time_then.tv_sec-time_now.tv_sec)*1000.0 +\*/
                   /*(time_then.tv_nsec-time_now.tv_nsec)*ONE_OVER_10SIX;*/
    //set time_then
    //assert that elapsed time is > 3*default time used
      /*if (j == 0)*/
        /*tt_int_op(elapsed_ms, >=, 3*(get_options_mutable()->SignalBlankIntervalMS));*/
      /*else*/
        /*tt_int_op(elapsed_ms, >=, 32*(get_options_mutable()->SignalBlankIntervalMS));*/
      /*fake_circ->n_circ_id++;*/
      /*free(address);*/
    /*}*/
  /*}*/
 /*done:*/
  /*UNMOCK(relay_send_command_from_edge_);*/
  /*tor_free(TO_ORIGIN_CIRCUIT(fake_circ)->cpath);*/
  /*tor_free(fake_circ);*/
/*}*/

static void
test_circ_memleak() {
  get_options_mutable()->SignalMethod = 2;
  MOCK(relay_send_command_from_edge_, mock_relay_send_command_from_edge);
  MOCK(channel_flush_some_cells, channel_flush_some_cells_mock);
  MOCK(connection_flush, connection_flush_mock);
  circuit_t *fake_circ1 = fake_or_circuit_new(0);
  circuit_t *fake_circ2 = fake_or_circuit_new(10);
  circuit_t *fake_circ3 = fake_or_circuit_new(50);
  circuit_t *fake_circ4 = fake_or_circuit_new(1002303);
  signal_encode_param_t param;
  param.address = tor_strdup("127.0.0.1");
  param.circ = fake_circ1;
  signal_decode_t *circ_timing1 = fake_circ1->circ_timing;
  circ_timing1->circid = fake_circ1->n_circ_id;
  
  signal_encode_destination(&param);

  signal_listen_and_decode(fake_circ1);
  signal_listen_and_decode(fake_circ1);
  signal_listen_and_decode(fake_circ1);

  signal_decode_t *circ_timing2 = fake_circ2->circ_timing;
  signal_decode_t *circ_timing3 = fake_circ3->circ_timing;
  signal_decode_t *circ_timing4 = fake_circ4->circ_timing;
  tt_int_op(circ_timing1->circid, OP_EQ, circ_timing1->circid);
 done:
  UNMOCK(relay_send_command_from_edge_);
  UNMOCK(channel_flush_some_cells);
  UNMOCK(connection_flush);
  tor_free(param.address);
  fake_circ_free(fake_circ1);
  fake_circ_free(fake_circ2);
  fake_circ_free(fake_circ3);
  fake_circ_free(fake_circ4);
}

/*static void*/
/*test_signal_decoding() {*/
  /*circuit_t *fake_circ[] = { */
    /*fake_or_circuit_new(1),*/
    /*fake_or_circuit_new(0),*/
    /*fake_or_circuit_new(2),*/
  /*};*/
  /*int r;*/
  /*char *addresses[] = {*/
    /*"182.232.10.82",*/
    /*"0.122.232.12",*/
    /*"12.23.12.124",*/
  /*};*/
  /*int should_call_min_blank[3] = {*/
    /*184+234+12+84,*/
    /*2+124+234+14,*/
    /*14+25+14+126,*/
  /*};*/
  /*//todo*/
  /*int should_call_bw_efficient[3] = {*/
    /*3+2+3+3+2+3+3+2\*/
      /*+3+3+3+2+3+2+2+2\*/
      /*+2+2+2+2+3+2+3+2\*/
      /*+2+3+2+3+2+2+3+2,*/
    /*2+2+2+2+2+2+2+2\*/
      /*+2+3+3+3+3+2+3+2\*/
      /*+3+3+3+2+3+2+2+2\*/
      /*+2+2+2+2+3+3+2+2,*/
    /*2+2+2+2+3+3+2+2\*/
      /*+2+2+2+3+2+3+3+3\*/
      /*+2+2+2+2+3+3+2+2\*/
      /*+2+3+3+3+3+3+2+2,*/
  /*};*/
  /*MOCK(relay_send_command_from_edge_, mock_relay_send_command_from_edge_decode);*/
  /*for (int j = 0; j < 2; j++) {*/
    /*if (j == 1)*/
      /*get_options_mutable()->SignalMethod = BANDWIDTH_EFFICIENT;*/
    /*for (int i = 0; i < 3; i++) {*/
      /*char *address = tor_malloc(strlen(addresses[i]+1));*/
      /*strcpy(address, addresses[i]);*/
      /*if (j == 0) {*/
        /*r = signal_minimize_blank_latency(address, fake_circ[i]);*/
        /*tt_int_op(mock_nbr_called, ==, should_call_min_blank[i]);*/
      /*}*/
      /*else {*/
        /*r = signal_bandwidth_efficient(address, fake_circ[i]);*/
        /*tt_int_op(mock_nbr_called, ==, should_call_bw_efficient[i]);*/
      /*}*/
      /*tt_int_op(r, ==, 0);*/
      /*sleep(1);*/
      /*r = signal_listen_and_decode(fake_circ[i]);*/
      /*mock_nbr_called = 0;*/
      /*tt_int_op(r, ==, 1); //successfully decode the address*/
    /*}*/
  /*}*/


 /*done:*/
  /*UNMOCK(relay_send_command_from_edge_);*/
  /*for (int i = 0; i < 3; i++) {*/
    /*signal_free(fake_circ[i]);*/
    /*tor_free(TO_ORIGIN_CIRCUIT(fake_circ[i])->cpath);*/
    /*tor_free(fake_circ[i]);*/
  /*}*/
/*}*/

struct testcase_t signal_attack_tests[] = {
  //{ "elapsed_signal_encoding", test_elapsed_signal_encoding, 0, NULL, NULL},
  { "circ_memleak", test_circ_memleak, 0, NULL, NULL},
  //{ "signal_decoding", test_signal_decoding, 0, NULL, NULL},
  END_OF_TESTCASES
};


