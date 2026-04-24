#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "clients.h"

client_t clients[MAX_PLAYERS];

void init_clients() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        clients[i].sock = -1;
        clients[i].connected = false;
        clients[i].name[0] = '\0';
        clients[i].id = i;
        clients[i].ready = false;
    }
}

int add_client(int sock) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!clients[i].connected) {
            clients[i].sock = sock;
            clients[i].connected = true;
            clients[i].ready = false;
            return i;
        }
    }
    return -1;
}

void remove_client(int id) {
    if (id >= 0 && id < MAX_PLAYERS) {
        clients[id].sock = -1;
        clients[id].connected = false;
        clients[id].ready = false;
        clients[id].name[0] = '\0';
    }
}

client_t* get_client(int id) {
    if (id >= 0 && id < MAX_PLAYERS) {
        return &clients[id];
    }
    return NULL;
}
