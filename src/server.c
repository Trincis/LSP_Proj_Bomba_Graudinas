#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#include "protocol.h"
#include "network.h"
#include "clients.h"
#include "game.h"

#define PORT 5000

#define DBG(...) fprintf(stderr, "[SERVER] " __VA_ARGS__)

#define GAME_LOBBY   0
#define GAME_RUNNING 1

#define MSG_MAP_SELECT   200

GameConfig g_cfg;
int p_x[MAX_PLAYERS], p_y[MAX_PLAYERS];

Bomb bombs[MAX_BOMBS];
BOOM spradzieni[MAX_BOOM];

int server_status = GAME_LOBBY;
int host_id = -1;
int ready_flags[MAX_PLAYERS];
int alive_flags[MAX_PLAYERS];

int initial_alive_count = 0;

const char *available_maps[] = {
    "maps/map.cfg",
    "maps/map2.cfg",
    "maps/map3.cfg"
};
int map_count = 3;
int selected_map = 0;

static struct timespec last_tick = {0};

void assign_spawn(int id) {
    int map_index = id + 1;
    p_x[id] = g_cfg.player_spawn_x[map_index];
    p_y[id] = g_cfg.player_spawn_y[map_index];
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
            buf[pos++] = (uint8_t)g_cfg.tiles[y][x];

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].connected && alive_flags[i]) {
            buf[pos++] = (uint8_t)p_x[i];
            buf[pos++] = (uint8_t)p_y[i];
        } else {
            buf[pos++] = 255;
            buf[pos++] = 255;
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected)
            send_msg(clients[i].sock, MSG_MAP, SERVER_ID, i, buf, pos);
}

void broadcast_ready(int pid) {
    uint8_t p[2] = { pid, ready_flags[pid] };
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected)
            send_msg(clients[i].sock, MSG_SET_READY, SERVER_ID, i, p, 2);
}

void broadcast_map_select() {
    uint8_t p[1] = { selected_map };
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected)
            send_msg(clients[i].sock, MSG_MAP_SELECT, SERVER_ID, i, p, 1);
}

int all_ready() {
    int any_connected = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].connected) {
            any_connected = 1;
            if (!ready_flags[i])
                return 0;
        }
    }

    return any_connected;
}

void start_game() {
    DBG("START_GAME loading map %d\n", selected_map);

    if (game_config_load(&g_cfg, available_maps[selected_map]) != 0) {
        DBG("ERROR loading map\n");
        return;
    }

    initial_alive_count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected)
            initial_alive_count++;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].connected) {
            assign_spawn(i);
            alive_flags[i] = 1;
        } else {
            alive_flags[i] = 0;
            p_x[i] = p_y[i] = 255;
        }
    }

    memset(bombs, 0, sizeof(bombs));
    memset(spradzieni, 0, sizeof(spradzieni));
    clock_gettime(CLOCK_MONOTONIC, &last_tick);

    server_status = GAME_RUNNING;

    uint8_t st = GAME_RUNNING;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected)
            send_msg(clients[i].sock, MSG_SET_STATUS, SERVER_ID, i, &st, 1);

    struct timespec ts = {0, 300000000}; // 300ms
    nanosleep(&ts, NULL);
    send_map_to_all();
}

int count_alive_players() {
    int alive = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected && alive_flags[i])
            alive++;
    return alive;
}

int get_last_alive() {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected && alive_flags[i])
            return i;
    return -1;
}

void handle_message(int sock, int id, msg_header_t *h, uint8_t *payload) {

    if (h->msg_type == MSG_HELLO) {

        if (host_id == -1) host_id = id;
        ready_flags[id] = 0;
        alive_flags[id] = 1;

        send_msg(sock, MSG_WELCOME, SERVER_ID, id, NULL, 0);

        uint8_t st = (uint8_t)server_status;
        send_msg(sock, MSG_SET_STATUS, SERVER_ID, id, &st, 1);

        broadcast_map_select();
        broadcast_ready(id);

        return;
    }

    if (server_status == GAME_LOBBY) {

        if (h->msg_type == MSG_SET_READY) {
            ready_flags[id] = payload[0];
            broadcast_ready(id);

            if (all_ready()) {
                start_game();
            }
            return;
        }

        if (h->msg_type == MSG_MAP_SELECT && id == host_id) {
            selected_map = payload[0] % map_count;
            broadcast_map_select();
            return;
        }

        return;
    }

    if (server_status == GAME_RUNNING) {

        if (!alive_flags[id]) {
            return;
        }

        if (h->msg_type == MSG_MOVE_ATTEMPT) {

            uint8_t d = payload[0];
            int nx = p_x[id], ny = p_y[id];

            if (d=='U') ny--;
            if (d=='D') ny++;
            if (d=='L') nx--;
            if (d=='R') nx++;

            if (nx<0 || ny<0 || nx>=g_cfg.col || ny>=g_cfg.row) return;
            if (g_cfg.tiles[ny][nx] == TILE_WALL) return;
            if (g_cfg.tiles[ny][nx] == TILE_BLOCK) return;
            if (g_cfg.tiles[ny][nx] == TILE_BOMB) return;

            TileType t = g_cfg.tiles[ny][nx];
            if (t == TILE_FASTER || t == TILE_BIGGER || t == TILE_LONGER) {

                uint8_t msg[2] = { id, t };

                for (int k = 0; k < MAX_PLAYERS; k++)
                    if (clients[k].connected)
                        send_msg(clients[k].sock, MSG_BONUS_RETRIEVED, SERVER_ID, k, msg, 2);

                g_cfg.tiles[ny][nx] = TILE_FLOOR;
            }

            p_x[id] = nx;
            p_y[id] = ny;

            if(g_cfg.tiles[ny][nx] == TILE_BOOM){
                uint8_t pl[1] = { (uint8_t)id };
                for(int j = 0; j<MAX_PLAYERS; j++)
                    if(clients[j].connected)
                        send_msg(clients[j].sock, MSG_DEATH, SERVER_ID, j, pl, 1);

                alive_flags[id] = 0;
                p_x[id] = p_y[id] = 255;
                send_map_to_all();
                return;
            }

            send_map_to_all();
            return;
        }

        if (h->msg_type == MSG_BOMB_ATTEMPT) {

            int x = p_x[id], y = p_y[id];
            if (x<0 || y<0 || x>=g_cfg.col || y>=g_cfg.row) return;
            if (g_cfg.tiles[y][x] != TILE_FLOOR) return;

            for (int i = 0; i < MAX_BOMBS; i++) {
                if (!bombs[i].aktivs) {
                    bombs[i].aktivs = 1;
                    bombs[i].x = x;
                    bombs[i].y = y;
                    bombs[i].timer = g_cfg.fuse_time;
                    g_cfg.tiles[y][x] = TILE_BOMB;
                    break;
                }
            }

            send_map_to_all();
            return;
        }
    }
}

void vaiMiris(){
    for(int i = 0; i<MAX_PLAYERS; i++){
        if(!clients[i].connected) continue;
        if(!alive_flags[i]) continue;

        if(g_cfg.tiles[p_y[i]][p_x[i]]==TILE_BOOM){
            uint8_t payload[1] = { (uint8_t)i };
            for(int j = 0; j<MAX_PLAYERS; j++)
                if(clients[j].connected)
                    send_msg(clients[j].sock, MSG_DEATH, SERVER_ID, j, payload, 1);

            alive_flags[i] = 0;
            p_x[i]=p_y[i] = 255;
        }
    }
}

void tick_logic() {
    if (server_status != GAME_RUNNING) return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long ms =
        (now.tv_sec - last_tick.tv_sec) * 1000 +
        (now.tv_nsec - last_tick.tv_nsec) / 1000000;

    if (ms < 150) return;

    last_tick = now;

    Spragsti(&g_cfg, bombs, spradzieni);
    vaiMiris();
    send_map_to_all();

    DBG("Alive flags: %d %d %d %d\n", alive_flags[0], alive_flags[1], alive_flags[2], alive_flags[3]);
    int alive_now = count_alive_players();

    if (alive_now == 1) {

        int winner = get_last_alive();
        if (winner >= 0) {
            uint8_t pl[1] = { (uint8_t)winner };
            for (int i = 0; i < MAX_PLAYERS; i++)
                if (clients[i].connected)
                    send_msg(clients[i].sock, MSG_WINNER, SERVER_ID, i, pl, 1);
        }

        server_status = GAME_LOBBY;

        for (int i = 0; i < MAX_PLAYERS; i++) {
            alive_flags[i] = 1;
            ready_flags[i] = 0;
        }

        uint8_t st = GAME_LOBBY;
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (clients[i].connected)
                send_msg(clients[i].sock, MSG_SET_STATUS, SERVER_ID, i, &st, 1);

        broadcast_ready(0);
        broadcast_map_select();
    }
}

int main() {

    init_clients();
    memset(ready_flags, 0, sizeof(ready_flags));
    memset(bombs, 0, sizeof(bombs));
    memset(spradzieni, 0, sizeof(spradzieni));
    memset(alive_flags, 0, sizeof(alive_flags));

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_sock, 8);

    while (1) {

        tick_logic();

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server_sock, &rfds);
        int maxfd = server_sock;

        for (int i = 0; i < MAX_PLAYERS; i++)
            if (clients[i].connected) {
                FD_SET(clients[i].sock, &rfds);
                if (clients[i].sock > maxfd) maxfd = clients[i].sock;
            }

        struct timeval tv = {0, 50000};
        int sel = select(maxfd+1, &rfds, NULL, NULL, &tv);
        if (sel < 0) continue;

        if (FD_ISSET(server_sock, &rfds)) {
            int ns = accept(server_sock, NULL, NULL);
            add_client(ns);
        }

        for (int i = 0; i < MAX_PLAYERS; i++)
            if (clients[i].connected && FD_ISSET(clients[i].sock, &rfds)) {

                while (1) {
                    msg_header_t h;
                    uint8_t buf[65535];

                    int r = recv_msg(clients[i].sock, &h, buf, sizeof(buf));
                    if (r <= 0) {
                        remove_client(i);
                        alive_flags[i] = 0;
                        p_x[i] = p_y[i] = 255;
                        break;
                    }

                    handle_message(clients[i].sock, i, &h, buf);

                    int more = 0;
                    ioctl(clients[i].sock, FIONREAD, &more);
                    if (more <= 0) break;
                }
            }
    }

    return 0;
}