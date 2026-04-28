#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"
// Vienkāršas funkcijas ziņu sūtīšanai un saņemšanai, izmantojot TCP socketus
int send_msg(int sock, uint8_t type, uint8_t sender, uint8_t target,
             const void *payload, size_t len);

int recv_msg(int sock, msg_header_t *h, uint8_t *buf, size_t bufsize);

#endif
