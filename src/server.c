#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>

#include "protocol.h"
#include "network.h"
#include "clients.h"
#include "game.h"

#define PORT 5000

GameConfig g_cfg;
int p_x[MAX_PLAYERS], p_y[MAX_PLAYERS];

static void assign_spawn(int id) {
    for (int s = 0; s < MAX_PLAYERS; s++) {
        if (g_cfg.player_spawn_x[s] != -1) {
            p_x[id] = g_cfg.player_spawn_x[s];
            p_y[id] = g_cfg.player_spawn_y[s];
            printf("[SERVER] Spawn player %d at %d,%d\n", id, p_x[id], p_y[id]);
            return;
        }
    }
}

void send_map_to_all() {
    uint8_t buf[65535];
    int pos = 0;

    buf[pos++] = g_cfg.row;
    buf[pos++] = g_cfg.col;

    for (int y = 0; y < g_cfg.row; y++)
        for (int x = 0; x < g_cfg.col; x++)
            buf[pos++] = g_cfg.tiles[y][x];

    for (int i = 0; i < MAX_PLAYERS; i++) {
        buf[pos++] = (clients[i].connected ? p_x[i] : 255);
        buf[pos++] = (clients[i].connected ? p_y[i] : 255);
    }

    printf("[SERVER] Sending MAP to all players\n");

    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected)
            send_msg(clients[i].sock, MSG_MAP, SERVER_ID, 254, buf, pos);
}

void handle_message(int sock, int id, msg_header_t *h, uint8_t *payload) {
    printf("[SERVER] Received msg %d from player %d\n", h->msg_type, id);

    if (h->msg_type == MSG_HELLO) {
        printf("[SERVER] HELLO from %d\n", id);
        assign_spawn(id);
        send_msg(sock, MSG_WELCOME, SERVER_ID, 254, NULL, 0);
        send_map_to_all();
        return;
    }

    if (h->msg_type == MSG_MOVE_ATTEMPT) {
        uint8_t dir = payload[0];
        printf("[SERVER] MOVE_ATTEMPT from %d dir=%c\n", id, dir);

        int nx = p_x[id];
        int ny = p_y[id];

        if (dir == 'U') ny--;
        if (dir == 'D') ny++;
        if (dir == 'L') nx--;
        if (dir == 'R') nx++;

        if (nx >= 0 && nx < g_cfg.col && ny >= 0 && ny < g_cfg.row) {
            if (g_cfg.tiles[ny][nx] == TILE_FLOOR) {
                p_x[id] = nx;
                p_y[id] = ny;
                printf("[SERVER] Player %d moved to %d,%d\n", id, nx, ny);
                send_map_to_all();
            }
        }
    }
}

int main() {
    printf("[SERVER] Loading map...\n");

    if (game_config_load(&g_cfg, "map.cfg") != 0) {
        printf("[SERVER] Failed to load map.cfg\n");
        return 1;
    }

    init_clients();
    for (int i = 0; i < MAX_PLAYERS; i++)
        p_x[i] = p_y[i] = -1;

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    printf("[SERVER] Socket created: %d\n", server_sock);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_sock, 8);

    printf("[SERVER] Running on port %d...\n", PORT);

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server_sock, &rfds);
        int maxfd = server_sock;

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (clients[i].connected) {
                FD_SET(clients[i].sock, &rfds);
                if (clients[i].sock > maxfd) maxfd = clients[i].sock;
            }
        }

        select(maxfd + 1, &rfds, NULL, NULL, NULL);

        if (FD_ISSET(server_sock, &rfds)) {
            int new_sock = accept(server_sock, NULL, NULL);
            int id = add_client(new_sock);
            printf("[SERVER] New client id=%d sock=%d\n", id, new_sock);
        }

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (clients[i].connected && FD_ISSET(clients[i].sock, &rfds)) {
                msg_header_t h;
                uint8_t buf[65535];
                int r = recv_msg(clients[i].sock, &h, buf, sizeof(buf));

                if (r <= 0) {
                    printf("[SERVER] Client %d disconnected (r=%d errno=%d)\n", i, r, errno);
                    remove_client(i);
                    p_x[i] = p_y[i] = -1;
                } else {
                    handle_message(clients[i].sock, i, &h, buf);
                }
            }
        }
    }
}