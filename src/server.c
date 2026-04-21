#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#include "protocol.h"
#include "network.h"
#include "clients.h"

#define PORT 5000

void handle_message(int sock, msg_header_t *h, uint8_t *payload);

int main() {
    init_clients();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }

    if (listen(server_fd, 8) < 0) {
        perror("listen"); exit(1);
    }

    printf("Server running on port %d\n", PORT);

    while (1) {
        int client_sock = accept(server_fd, NULL, NULL);
        if (client_sock < 0) continue;

        int id = add_client(client_sock);
        if (id < 0) {
            printf("Server full, rejecting client\n");
            send_msg(client_sock, MSG_DISCONNECT, SERVER_ID, 255, NULL, 0);
            close(client_sock);
            continue;
        }

        printf("Client connected with ID %d\n", id);

        msg_header_t h;
        uint8_t payload[1024];

        while (1) {
            int r = recv_msg(client_sock, &h, payload, sizeof(payload));
            if (r <= 0) break;

            handle_message(client_sock, &h, payload);
        }

        printf("Client %d disconnected\n", id);
        remove_client(id);
    }

    return 0;
}

void handle_message(int sock, msg_header_t *h, uint8_t *payload) {
    switch (h->msg_type) {

    case MSG_HELLO: {
        int id = h->sender_id;
        client_t *c = get_client(id);
        if (!c) {
            printf("HELLO from unknown id %d\n", id);
            return;
        }

        // payload: 20 baiti client identifier + 30 baiti player name
        char client_id[21];
        char player_name[31];

        memcpy(client_id, payload, 20);
        client_id[20] = '\0';

        memcpy(player_name, payload + 20, 30);
        player_name[30] = '\0';

        strncpy(c->name, player_name, 30);
        c->name[30] = '\0';

        printf("HELLO from client %d: prog=\"%s\" name=\"%s\"\n",
               id, client_id, c->name);

        // WELCOME: server_id[20], status, count, (pag. count=0)
        uint8_t buf[64];
        char server_id[20] = "BomberServer 1.0";
        memcpy(buf, server_id, 20);
        buf[20] = 0;   // GAME_LOBBY
        buf[21] = 0;   // pagaidām nav citu klientu

        send_msg(sock, MSG_WELCOME, (uint8_t)id, (uint8_t)id, buf, 22);
        break;
    }

    case MSG_PING:
        send_msg(sock, MSG_PONG, SERVER_ID, h->sender_id, NULL, 0);
        break;

    default:
        printf("Unknown message type %d from %d\n", h->msg_type, h->sender_id);
        break;
    }
}