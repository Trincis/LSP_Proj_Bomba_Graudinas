#ifndef CLIENTS_H
#define CLIENTS_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

typedef struct {
    int sock;
    bool connected;
    char name[31];
    int id;
    bool ready;
} client_t;

extern client_t clients[MAX_PLAYERS];

void init_clients();
int add_client(int sock);
void remove_client(int id);
client_t* get_client(int id);

#endif