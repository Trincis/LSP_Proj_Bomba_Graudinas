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
// Izveidojam atsevišķu masīvu, kur glabāt aktuālās spēlētāju pozīcijas
int p_x[MAX_PLAYERS], p_y[MAX_PLAYERS];

void send_map_to_all() {
    uint8_t buf[65535];
    int pos = 0;
    buf[pos++] = (uint8_t)g_cfg.row;
    buf[pos++] = (uint8_t)g_cfg.col;

    for (int y = 0; y < g_cfg.row; y++) {
        for (int x = 0; x < g_cfg.col; x++) {
            buf[pos++] = (uint8_t)g_cfg.tiles[y][x];
        }
    }
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].connected) {
            buf[pos++] = (uint8_t)p_x[i];
            buf[pos++] = (uint8_t)p_y[i];
        } else {
            buf[pos++] = 255; buf[pos++] = 255;
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].connected) {
            send_msg(clients[i].sock, MSG_SET_STATUS, SERVER_ID, i, buf, pos);
        }
    }
}

void handle_message(int sock, int id, msg_header_t *h, uint8_t *payload) {
    switch (h->msg_type) {
        case MSG_HELLO:
            // Atrodam brīvu spawn punktu no map.cfg ielādētajiem
            if (p_x[id] == -1) {
                for (int s = 0; s < 8; s++) { // Pārbaudām spawn punktus 0-7
                    if (g_cfg.player_spawn_x[s] != -1) {
                        p_x[id] = g_cfg.player_spawn_x[s];
                        p_y[id] = g_cfg.player_spawn_y[s];
                        // Svarīgi: NEIZDZĒŠAM spawn punktu no g_cfg, lai serveris zinātu, kur tu esi
                        break;
                    }
                }
            }
            printf("DEBUG: Player %d connected at %d,%d\n", id, p_x[id], p_y[id]);
            send_msg(sock, MSG_WELCOME, SERVER_ID, id, NULL, 0);
            send_map_to_all();
            break;

        case MSG_MOVE_ATTEMPT: {
            if (p_x[id] == -1) break; // Ja spēlētājam nav pozīcijas, nevar kustēties
            
            uint8_t dir = payload[0];
            int nx = p_x[id];
            int ny = p_y[id];

            if (dir == 1) ny--;      // Up
            else if (dir == 2) ny++; // Down
            else if (dir == 3) nx--; // Left
            else if (dir == 4) nx++; // Right

            if (nx >= 0 && nx < g_cfg.col && ny >= 0 && ny < g_cfg.row) {
                TileType target = g_cfg.tiles[ny][nx];
                // Atļaujam iet pa grīdu vai bonusiem
                if (target == TILE_FLOOR || (target >= TILE_FASTER && target <= TILE_LONGER)) {
                    p_x[id] = nx;
                    p_y[id] = ny;
                    send_map_to_all();
                }
            }
            break;
        }

        case MSG_BOMB_ATTEMPT:
            if (g_cfg.tiles[p_y[id]][p_x[id]] == TILE_FLOOR) {
                g_cfg.tiles[p_y[id]][p_x[id]] = TILE_BOMB;
                send_map_to_all();
            }
            break;
    }
}

int main() {
    if (game_config_load(&g_cfg, "map.cfg") != 0) return 1;
    init_clients();
    for(int i=0; i<MAX_PLAYERS; i++) p_x[i] = p_y[i] = -1;

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_sock, 8);

    printf("Server running on port %d...\n", PORT);

    while (1) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(server_sock, &rfds);
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
            add_client(new_sock);
        }
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (clients[i].connected && FD_ISSET(clients[i].sock, &rfds)) {
                msg_header_t h; uint8_t buf[65535];
                if (recv_msg(clients[i].sock, &h, buf, sizeof(buf)) <= 0) {
                    remove_client(i);
                    p_x[i] = p_y[i] = -1;
                } else {
                    handle_message(clients[i].sock, i, &h, buf);
                }
            }
        }
    }
    return 0;
}