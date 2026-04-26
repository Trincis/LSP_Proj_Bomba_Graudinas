#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#include "protocol.h"
#include "network.h"
#include "clients.h"
#include "game.h"

#define PORT 5000

GameConfig g_cfg;
int p_x[MAX_PLAYERS], p_y[MAX_PLAYERS];

void assign_spawn(int id) {
    if (g_cfg.player_spawn_x[id] != -1) {
        p_x[id] = g_cfg.player_spawn_x[id];
        p_y[id] = g_cfg.player_spawn_y[id];
        printf("[SERVER] Spawn player %d at %d,%d\n", id, p_x[id], p_y[id]);
        return;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g_cfg.player_spawn_x[i] != -1) {
            p_x[id] = g_cfg.player_spawn_x[i];
            p_y[id] = g_cfg.player_spawn_y[i];
            printf("[SERVER] Spawn player %d at %d,%d (fallback)\n", id, p_x[id], p_y[id]);
            return;
        }
    }

    p_x[id] = 1;
    p_y[id] = 1;
    printf("[SERVER] Spawn player %d at default 1,1\n", id);
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

    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected)
            send_msg(clients[i].sock, MSG_MAP, SERVER_ID, i, buf, pos);
}

void handle_message(int sock, int id, msg_header_t *h, uint8_t *payload) {
    printf("[SERVER DEBUG] msg=%d from player=%d\n", h->msg_type, id);

    if (h->msg_type == MSG_HELLO) {
        assign_spawn(id);
        send_msg(sock, MSG_WELCOME, SERVER_ID, id, NULL, 0);
        send_map_to_all();
        return;
    }

    if (h->msg_type == MSG_MOVE_ATTEMPT) {
        printf("[SERVER DEBUG] MOVE_ATTEMPT dir=%c\n", payload[0]);
        uint8_t dir = payload[0];

        int nx = p_x[id];
        int ny = p_y[id];

        if (dir == 'U') ny--;
        if (dir == 'D') ny++;
        if (dir == 'L') nx--;
        if (dir == 'R') nx++;

        if (nx < 0 || nx >= g_cfg.col || ny < 0 || ny >= g_cfg.row)
            return;

        if (g_cfg.tiles[ny][nx] != TILE_FLOOR)
            return;

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (i != id && clients[i].connected) {
                if (p_x[i] == nx && p_y[i] == ny)
                    return;
            }
        }

        p_x[id] = nx;
        p_y[id] = ny;
        send_map_to_all();
        return;
    }

    if (h->msg_type == MSG_BOMB_ATTEMPT) {
        int bx = p_x[id];
        int by = p_y[id];

        if (bx >= 0 && bx < g_cfg.col && by >= 0 && by < g_cfg.row) {
            g_cfg.tiles[by][bx] = TILE_BOMB;
            send_map_to_all();
        }
        return;
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
    if (server_sock < 0) {
        perror("socket");
        return 1;
    }
    printf("[SERVER] Socket created: %d\n", server_sock);

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(server_sock, 8) < 0) {
        perror("listen");
        return 1;
    }

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

        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(server_sock, &rfds)) {
            int new_sock = accept(server_sock, NULL, NULL);
            if (new_sock >= 0) {
                int id = add_client(new_sock);
                printf("[SERVER] New client id=%d sock=%d\n", id, new_sock);
            }
        }

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (clients[i].connected && FD_ISSET(clients[i].sock, &rfds)) {

                while (1) {
                    msg_header_t h;
                    uint8_t buf[65535];

                    int r = recv_msg(clients[i].sock, &h, buf, sizeof(buf));
                    if (r <= 0) {
                        printf("[SERVER] Client %d disconnected\n", i);
                        remove_client(i);
                        p_x[i] = p_y[i] = -1;
                        break;
                    }

                    handle_message(clients[i].sock, i, &h, buf);

                    int more = 0;
                    ioctl(clients[i].sock, FIONREAD, &more);
                    if (more <= 0)
                        break;
                }
            }
        }
    }

    close(server_sock);
    return 0;
}