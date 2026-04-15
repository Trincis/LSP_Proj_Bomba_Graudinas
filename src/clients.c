#include "clients.h"
#include <string.h>
#include <unistd.h>

client_t clients[MAX_PLAYERS];

void init_clients() {
    memset(clients, 0, sizeof(clients));
}

int add_client(int sock) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!clients[i].connected) {
            clients[i].connected = true;
            clients[i].sock = sock;
            clients[i].id = i;
            clients[i].ready = false;
            return i;
        }
    }
    return -1;
}

void remove_client(int id) {
    if (id < 0 || id >= MAX_PLAYERS) return;
    close(clients[id].sock);
    clients[id].connected = false;
}

client_t* get_client(int id) {
    if (id < 0 || id >= MAX_PLAYERS) return NULL;
    return &clients[id];
}
