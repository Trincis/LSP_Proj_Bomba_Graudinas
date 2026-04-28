#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
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

Bomb bombs[MAX_BOMBS];
BOOM spradzieni[MAX_BOOM];

void assign_spawn(int id) {
    if (g_cfg.player_spawn_x[id] != -1) {
        p_x[id] = g_cfg.player_spawn_x[id];
        p_y[id] = g_cfg.player_spawn_y[id];
        return;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g_cfg.player_spawn_x[i] != -1) {
            p_x[id] = g_cfg.player_spawn_x[i];
            p_y[id] = g_cfg.player_spawn_y[i];
            return;
        }
    }

    p_x[id] = 1;
    p_y[id] = 1;
}

void send_map_to_all() {
    uint8_t buf[65535];
    int pos = 0;

    buf[pos++] = g_cfg.row;
    buf[pos++] = g_cfg.col;
    buf[pos++] = g_cfg.pl_speed;
    buf[pos++] = g_cfg.exp_distance;
    buf[pos++] = g_cfg.exp_danger;
    buf[pos++] = g_cfg.fuse_time;

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

void place_bomb(int id, int x, int y) {
    (void)id;

    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].aktivs) {
            bombs[i].aktivs = 1;
            bombs[i].x = x;
            bombs[i].y = y;
            bombs[i].timer = g_cfg.fuse_time;

            g_cfg.tiles[y][x] = TILE_BOMB;
            send_map_to_all();
            return;
        }
    }
}

void handle_message(int sock, int id, msg_header_t *h, uint8_t *payload) {

    if (h->msg_type == MSG_HELLO) {
        assign_spawn(id);
        send_msg(sock, MSG_WELCOME, SERVER_ID, id, NULL, 0);
        send_map_to_all();
        return;
    }

    if (h->msg_type == MSG_MOVE_ATTEMPT) {
        uint8_t dir = payload[0];

        int nx = p_x[id];
        int ny = p_y[id];

        if (dir == 'U') ny--;
        if (dir == 'D') ny++;
        if (dir == 'L') nx--;
        if (dir == 'R') nx++;

        if (nx < 0 || nx >= g_cfg.col || ny < 0 || ny >= g_cfg.row)
            return;

        TileType t = g_cfg.tiles[ny][nx];
        if (t == TILE_WALL || t == TILE_BLOCK || t == TILE_BOMB)
            return;

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (i != id && clients[i].connected) {
                if (p_x[i] == nx && p_y[i] == ny)
                    return;
            }
        }

        p_x[id] = nx;
        p_y[id] = ny;

        if(t == TILE_FASTER || t == TILE_BIGGER || t == TILE_LONGER){
            uint8_t bonusp[2];
            bonusp[0] = id; ///kurš
            bonusp [1] = t; ///ko

            for(int i = 0; i<MAX_PLAYERS; i++){
                if(clients[i].connected){
                    send_msg(clients[i].sock, MSG_BONUS_RETRIEVED, SERVER_ID, i, bonusp, 2);
                }
            }
            g_cfg.tiles[ny][nx] = TILE_FLOOR;
        }

        send_map_to_all();
        return;
    }

    if (h->msg_type == MSG_BOMB_ATTEMPT) {
        int bx = p_x[id];
        int by = p_y[id];

        if (g_cfg.tiles[by][bx] == TILE_FLOOR) {
            place_bomb(id, bx, by);
        }
        return;
    }
}

int main() {

    if (game_config_load(&g_cfg, "map.cfg") != 0) {
        printf("[SERVER] Failed to load map.cfg\n");
        return 1;
    }

    init_clients();
    for (int i = 0; i < MAX_PLAYERS; i++)
        p_x[i] = p_y[i] = -1;

    for (int i = 0; i < MAX_BOMBS; i++) {
        bombs[i].aktivs = 0;
        bombs[i].timer = 0;
    }

    for (int i = 0; i < MAX_BOOM; i++) {
        spradzieni[i].aktivs = 0;
        spradzieni[i].timer = 0;
    }

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) return 1;

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) return 1;
    if (listen(server_sock, 8) < 0) return 1;

    uint64_t last_tick = clock();

    while (1) {

        uint64_t now = clock();
        if (now - last_tick > (CLOCKS_PER_SEC / 1024)) {
            last_tick = now;
            printf("TICK\n");


            for (int i = 0; i < MAX_BOMBS; i++) {
                if (bombs[i].aktivs) {
                    bombs[i].timer--;
                    if (bombs[i].timer <= 0) {
                        printf("SPRAAGSTI: %d %d\n", bombs[i].x, bombs[i].y);
                        Spragsti(&g_cfg, &bombs[i], spradzieni, MAX_BOOM);
                        send_map_to_all();
                    }
                }
            }

            for (int i = 0; i < MAX_BOOM; i++) {
                if (spradzieni[i].aktivs) {
                    spradzieni[i].timer--;
                    if (spradzieni[i].timer <= 0) {
                        spradzieni[i].aktivs = 0;

                        int x = spradzieni[i].x;
                        int y = spradzieni[i].y;

                        if (g_cfg.tiles[y][x] == TILE_BOOM)
                            g_cfg.tiles[y][x] = TILE_FLOOR;
                    }
                }
            }

            send_map_to_all();
        }

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

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // 50ms timeout

        if (select(maxfd + 1, &rfds, NULL, NULL, &tv) < 0)
            break;

        if (FD_ISSET(server_sock, &rfds)) {
            int new_sock = accept(server_sock, NULL, NULL);
            if (new_sock >= 0) {
                add_client(new_sock);
            }
        }

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (clients[i].connected && FD_ISSET(clients[i].sock, &rfds)) {

                while (1) {
                    msg_header_t h;
                    uint8_t buf[65535];

                    int r = recv_msg(clients[i].sock, &h, buf, sizeof(buf));
                    if (r <= 0) {
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