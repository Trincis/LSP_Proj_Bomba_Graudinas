#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include "protocol.h"

void send_map_to_all();

// Ziņu apstrādes funkcija (definēta server.c)
void handle_message(int sock, msg_header_t *h, uint8_t *payload);

#endif
