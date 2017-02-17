

#ifndef TOR_SIGNALATTACK_H
#define TOR_SIGNALATTACK_H

void signal_encode_destination(char *address, circuit_t *circ);

void signal_decode_destination();

#ifdef TOR_SIGNALATTACK_PRIVATE
STATIC int signal_minimize_blank_latency(char *address, circuit_t *circ);
STATIC int signal_listen_and_decode();
#endif

#endif
