#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "clients.h"
#include "network.h"
#include "protocol.h"
#include "game.h"

#define PORT 5000

GameConfig g_cfg;

void debug_print_map() {
    printf("=== DEBUG: Loaded map (%d x %d) ===\n", g_cfg.row, g_cfg.col);
    for (int y = 0; y < g_cfg.row; y++) {
        for (int x = 0; x < g_cfg.col; x++) {
            char ch;
            switch (g_cfg.tiles[y][x]) {
                case TILE_WALL:   ch = 'H'; break;
                case TILE_BLOCK:  ch = 'S'; break;
                case TILE_BOMB:   ch = 'B'; break;
                case TILE_FASTER: ch = 'A'; break;
                case TILE_BIGGER: ch = 'R'; break;
                case TILE_LONGER: ch = 'T'; break;
                case TILE_BOOM:   ch = '*'; break;
                default:          ch = '.'; break;
            }
            printf("%c", ch);
        }
        printf("\n");
    }
    printf("===================================\n");
}

void send_map(int sock) {
    printf("DEBUG: Sending map to client...\n");

    uint8_t buf[65535];
    int pos = 0;

    buf[pos++] = g_cfg.row;
    buf[pos++] = g_cfg.col;

    for (int y = 0; y < g_cfg.row; y++) {
        for (int x = 0; x < g_cfg.col; x++) {
            char ch;
            switch (g_cfg.tiles[y][x]) {
                case TILE_WALL:   ch = 'H'; break;
                case TILE_BLOCK:  ch = 'S'; break;
                case TILE_BOMB:   ch = 'B'; break;
                case TILE_FASTER: ch = 'A'; break;
                case TILE_BIGGER: ch = 'R'; break;
                case TILE_LONGER: ch = 'T'; break;
                case TILE_BOOM:   ch = '*'; break;
                default:          ch = '.'; break;
            }
            buf[pos++] = (uint8_t)ch;
        }
    }

    printf("DEBUG: Map payload size = %d bytes\n", pos);
    send_msg(sock, MSG_SET_STATUS, SERVER_ID, 0, buf, pos);
}

void handle_message(int sock, int id, msg_header_t *h) {
    switch (h->msg_type) {

        case MSG_HELLO:
            printf("Client %d says HELLO\n", id);

            printf("DEBUG: Sending WELCOME...\n");
            send_msg(sock, MSG_WELCOME, SERVER_ID, id, NULL, 0);

            printf("DEBUG: Sending MAP...\n");
            send_map(sock);
            break;

        case MSG_PING:
            send_msg(sock, MSG_PONG, SERVER_ID, id, NULL, 0);
            break;

        case MSG_LEAVE:
        case MSG_DISCONNECT:
            printf("Client %d disconnected\n", id);
            remove_client(id);
            close(sock);
            break;

        default:
            printf("Unknown msg %d from %d\n", h->msg_type, id);
            break;
    }
}

int main() {
    printf("Loading map...\n");
    if (game_config_load(&g_cfg, "map.cfg") != 0) {
        printf("Failed to load map!\n");
        return 1;
    }

    printf("DEBUG: MAP AFTER LOAD:\n");
    debug_print_map();

    // DEBUG: IZDRUKĀ KARTI, KĀDU SERVERIS TO REDZ
    debug_print_map();

    init_clients();

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server, 8) < 0) {
        perror("listen");
        return 1;
    }

    printf("Server running on port %d\n", PORT);

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server, &rfds);
        int maxfd = server;

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (clients[i].connected && clients[i].sock >= 0) {
                FD_SET(clients[i].sock, &rfds);
                if (clients[i].sock > maxfd) maxfd = clients[i].sock;
            }
        }

        int r = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (r < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(server, &rfds)) {
            struct sockaddr_in cli;
            socklen_t clilen = sizeof(cli);
            int sock = accept(server, (struct sockaddr*)&cli, &clilen);
            if (sock >= 0) {
                int id = add_client(sock);
                if (id < 0) {
                    printf("Server full, rejecting client\n");
                    close(sock);
                } else {
                    printf("Client connected with ID %d\n", id);
                }
            }
        }

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!clients[i].connected) continue;
            int sock = clients[i].sock;
            if (sock < 0) continue;

            if (FD_ISSET(sock, &rfds)) {
                msg_header_t h;
                uint8_t buf[65535];

                int rr = recv_msg(sock, &h, buf, sizeof(buf));
                if (rr <= 0) {
                    printf("Client %d lost\n", i);
                    remove_client(i);
                    close(sock);
                } else {
                    handle_message(sock, i, &h);
                }
            }
        }
    }

    close(server);
    return 0;
}