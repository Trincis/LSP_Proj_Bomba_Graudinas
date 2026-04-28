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

#ifndef MSG_MAP_SELECT
#define MSG_MAP_SELECT 0x10  //0x10 lai nesakrīt kopā ar citiem MSG_ veida ziņojumiem
#endif

#define PORT 5000 // Noklusējuma ports serverim
#define DBG(...) fprintf(stderr, "[SERVER] " __VA_ARGS__)
// Spēles stāvoklis
static GameConfig g_cfg;
static int server_status = GAME_LOBBY;
static int host_id = -1;
// Spēlētāju stāvoklis
static uint8_t pl_ready[MAX_PLAYERS];
static int p_row[MAX_PLAYERS];
static int p_col[MAX_PLAYERS];
static uint8_t alive[MAX_PLAYERS];
// Bumbu stāvoklis
static Bomb bombs[MAX_BOMBS];
static BOOM spradzieni[MAX_BOOM];

static uint8_t bomb_count[MAX_PLAYERS];   // cik bumbas drīkst vienlaikus
static uint8_t pl_speed[MAX_PLAYERS];
static uint8_t pl_radiuss[MAX_PLAYERS];
static uint8_t pl_fuse[MAX_PLAYERS];

// Laika kontrole
static struct timespec last_tick = {0};

// kartes izvēle: pēc noklusējuma map1
static char selected_map[64] = "maps/map1.cfg";
// Funkciju deklarācijas
static void broadcast_to_all(uint8_t type, const void *payload, size_t len) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected)
            send_msg(clients[i].sock, type, SERVER_ID, BROADCAST_ID, payload, len);
}
// Spēlētāju sākuma pozīciju piešķiršana
static void assign_spawn(int id) {
    p_row[id] = g_cfg.player_spawn_y[id];
    p_col[id] = g_cfg.player_spawn_x[id];
}
// Kartes un spēles stāvokļa sūtīšana klientiem
static void send_map_to_one(int cid) {
    if (!clients[cid].connected) return;

    uint8_t buf[65535];
    int pos = 0;
    // Sūtām kartes konfigurāciju un spēlētāju starta pozīcijas
    buf[pos++] = g_cfg.row;
    buf[pos++] = g_cfg.col;
    buf[pos++] = g_cfg.pl_speed;
    buf[pos++] = g_cfg.exp_distance;
    buf[pos++] = g_cfg.exp_danger;
    buf[pos++] = g_cfg.fuse_time;
    // Sūtām kartes datus rindām un kolonnām
    for (int r = 0; r < g_cfg.row; r++)
        for (int c = 0; c < g_cfg.col; c++)
            buf[pos++] = g_cfg.tiles[r][c];
    // Sūtām spēlētāju starta pozīcijas
    for (int i = 0; i < MAX_PLAYERS; i++) {
        buf[pos++] = g_cfg.player_spawn_x[i];
        buf[pos++] = g_cfg.player_spawn_y[i];
    }
    // Nosūtām karti konkrētajam klientam
    send_msg(clients[cid].sock, MSG_MAP, SERVER_ID, cid, buf, pos);
}
// Sūta kartes datus visiem pieslēgtajiem klientiem
static void send_map_to_all(void) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected)
            send_map_to_one(i);
}
// Sūta spēles stāvokļa datus konkrētam klientam
static void send_sync_board_to_one(int cid) {
    if (!clients[cid].connected) return;
    // Sagatavo datus par spēlētāju stāvokli un bumbām
    uint8_t buf[65535];
    int pos = 0;

    buf[pos++] = server_status;
    // Sūtām spēlētāju stāvokli: ID, dzīvs/miris, pozīcija
    buf[pos++] = MAX_PLAYERS;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        buf[pos++] = i;
        buf[pos++] = alive[i];
        uint16_t cell = 0;
        if (alive[i])
            cell = make_cell_index(p_row[i], p_col[i], g_cfg.col);
        buf[pos++] = cell >> 8;
        buf[pos++] = cell & 0xFF;
    }
    // Sūtām bumbu stāvokli: aktivs/neaktīvs, pozīcija, taimeris
    buf[pos++] = MAX_BOMBS;
    for (int i = 0; i < MAX_BOMBS; i++) {
        buf[pos++] = bombs[i].aktivs;
        uint16_t cell = 0;
        if (bombs[i].aktivs)
            cell = make_cell_index(bombs[i].y, bombs[i].x, g_cfg.col);
        buf[pos++] = cell >> 8;
        buf[pos++] = cell & 0xFF;
        buf[pos++] = bombs[i].timer;
    }
    // Sūtām sprādzienu stāvokli: aktivs/neaktīvs, pozīcija, taimeris
    send_msg(clients[cid].sock, MSG_SYNC_BOARD, SERVER_ID, cid, buf, pos);
}
// Sūta spēles stāvokļa datus visiem pieslēgtajiem klientiem
static void send_sync_board_to_all(void) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].connected)
            send_sync_board_to_one(i);
}
// Apstrādā HELLO ziņojumu no klienta, reģistrē spēlētāju un nosūta WELCOME atbildi
static void handle_hello(int cid, msg_header_t *h, uint8_t *payload) {
    char name[31] = {0};
    // Ja HELLO ziņojumā ir spēlētāja vārds, to saglabājam
    if (h->payload_len >= 50)
        memcpy(name, payload + 20, 30);
    // Ja nav hosta, šis spēlētājs kļūst par hostu
    if (host_id == -1){
        host_id = cid;
        pl_ready[cid] = 1;
    }

    alive[cid] = 0;

    snprintf(clients[cid].name, sizeof(clients[cid].name), "%s", name);// Reģistrējam spēlētāju kā pieslēgtu

    uint8_t buf[2048];
    int pos = 0;
    // Sagatavojam WELCOME ziņojumu ar spēlētāja ID, hosta statusu, un citiem spēles datiem
    char server_id[20] = "LSP_Bomberman_2026";
    memcpy(buf + pos, server_id, 20); pos += 20;
    // Sūtām spēlētāja vārdu atpakaļ klientam
    buf[pos++] = server_status;

    int count_pos = pos++;
    uint8_t count = 0;
    // Sūtām informāciju par visiem pieslēgtajiem spēlētājiem (ID, dzīvs/miris, vārds)
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!clients[i].connected) continue;
        buf[pos++] = i;
        buf[pos++] = alive[i];
        memset(buf + pos, 0, 30);
        strncpy((char*)(buf + pos), clients[i].name, 30);// Spēlētāja vārds
        pos += 30;
        count++;
    }
    // Atjauninām pieslēgto spēlētāju skaitu WELCOME ziņojumā
    buf[count_pos] = count;
    // Nosūtām WELCOME ziņojumu klientam, kurš pievienojās
    send_msg(clients[cid].sock, MSG_WELCOME, SERVER_ID, cid, buf, pos);
    // Ja spēle jau ir sākusies, nosūtām jaunajam spēlētājam karti un spēles stāvokli
    if (server_status == GAME_RUNNING)
        send_sync_board_to_one(cid);
}
// Apstrādā klienta atvienošanos, atjaunina spēlētāju stāvokli un informē pārējos klientus
static void start_game(void) {
    if (game_config_load(&g_cfg, selected_map) != 0) {
        DBG("Map load error from %s\n", selected_map);
        return;
    }

    // noklusējuma īpašības
    for (int i = 0; i < MAX_PLAYERS; i++) {
        bomb_count[i] = 1; // sākumā 1 bumba
        pl_speed[i] = g_cfg.pl_speed;
        pl_radiuss[i] = g_cfg.exp_distance;
        pl_fuse[i] = g_cfg.fuse_time;
    }
    // piešķiram starta pozīcijas un stāvokli spēlētājiem
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].connected) {
            assign_spawn(i);
            alive[i] = 1;
        } else {
            alive[i] = 0;
            p_row[i] = p_col[i] = -1;
        }
    }
    // inicializējam bumbu un sprādzienu stāvokli
    memset(bombs, 0, sizeof(bombs));
    memset(spradzieni, 0, sizeof(spradzieni));
    memset(pl_ready, 0, sizeof(pl_ready));
    clock_gettime(CLOCK_MONOTONIC, &last_tick);

    server_status = GAME_RUNNING;

    uint8_t st = GAME_RUNNING;
    broadcast_to_all(MSG_SET_STATUS, &st, 1);

    send_map_to_all();

    // paziņojam par bonusiem (BONUS_AVAILABLE)
    for (int r = 0; r < g_cfg.row; r++) {
        for (int c = 0; c < g_cfg.col; c++) {
            TileType t = g_cfg.tiles[r][c];
            if (t == TILE_FASTER || t == TILE_BIGGER ||
                t == TILE_LONGER || t == TILE_MOREBOMBS) {
                uint8_t msg[3];
                msg[0] = (uint8_t)t; // bonusa veids
                uint16_t cell = make_cell_index(r, c, g_cfg.col);
                msg[1] = cell >> 8;
                msg[2] = cell & 0xFF;
                broadcast_to_all(MSG_BONUS_AVAILABLE, msg, 3);
            }
        }
    }

    send_sync_board_to_all();
}
// Apstrādā spēles beigas, nosūta uzvarētāja informāciju klientiem un atjauno servera stāvokli
static void end_game(int winner) {
    server_status = GAME_END;

    uint8_t st = GAME_END;
    broadcast_to_all(MSG_SET_STATUS, &st, 1);

    uint8_t w[1] = { winner };
    broadcast_to_all(MSG_WINNER, w, 1);

    server_status = GAME_LOBBY;

    memset(alive, 0, sizeof(alive));

    uint8_t st2 = GAME_LOBBY;
    broadcast_to_all(MSG_SET_STATUS, &st2, 1);
}
// Apstrādā spēlētāja kustības mēģinājumu, atjaunina spēlētāja pozīciju un informē citus klientus
static void handle_move_attempt(int cid, uint8_t dir) {
    if (!alive[cid]) return;

    int r = p_row[cid];
    int c = p_col[cid];

    if (dir == 'U') r--;
    else if (dir == 'D') r++;
    else if (dir == 'L') c--;
    else if (dir == 'R') c++;
    // Pārbauda, vai jaunā pozīcija ir derīga un nav aizņemta ar sienu, bloku vai bumbu
    if (r < 0 || r >= g_cfg.row || c < 0 || c >= g_cfg.col) return;

    TileType t = g_cfg.tiles[r][c];
    if (t == TILE_WALL || t == TILE_BLOCK || t == TILE_BOMB)
        return;
    // Ja uziet uz bonusa, to savāc un paziņo pārējiem
    if (t == TILE_FASTER || t == TILE_BIGGER || t == TILE_LONGER || t == TILE_MOREBOMBS) {
        uint8_t msg[4];
        msg[0] = cid;
        msg[1] = t;
        uint16_t cell = make_cell_index(r, c, g_cfg.col);// aprēķina šūnas indeksu
        msg[2] = cell >> 8;
        msg[3] = cell & 0xFF;
        broadcast_to_all(MSG_BONUS_RETRIEVED, msg, 4);// paziņo pārējiem, ka bonuss savākts

        if (t == TILE_MOREBOMBS) {
            if (bomb_count[cid] < 255)
                bomb_count[cid]++;   // +1 bumba
        }
        if(t == TILE_FASTER) pl_speed[cid]++;
        if(t == TILE_BIGGER) pl_radiuss[cid]++; 
        if(t == TILE_LONGER) pl_fuse[cid]++;

        g_cfg.tiles[r][c] = TILE_FLOOR;// noņem bonusu no kartes
    }

    p_row[cid] = r;
    p_col[cid] = c;
    // Paziņo pārējiem par spēlētāja kustību
    uint8_t buf[3];
    buf[0] = cid;
    uint16_t cell = make_cell_index(r, c, g_cfg.col);
    buf[1] = cell >> 8;
    buf[2] = cell & 0xFF;
    // Paziņo pārējiem par spēlētāja kustību
    broadcast_to_all(MSG_MOVED, buf, 3);
}
// Apstrādā bumbas likšanas mēģinājumu, atjaunina bumbu stāvokli un informē citus klientus
static void handle_bomb_attempt(int cid) {
    if (!alive[cid]) return;

    int r = p_row[cid];
    int c = p_col[cid];
    // Pārbauda, vai šūna ir tukša un nav aizņemta ar sienu, bloku, bumbu vai sprādzienu
    if (g_cfg.tiles[r][c] != TILE_FLOOR) return;

    // cik bumbas jau ir šim spēlētājam?
    int active_for_player = 0;
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].aktivs && bombs[i].owner == cid)
            active_for_player++;
    }
    if (active_for_player >= bomb_count[cid])
        return; // vairs nedrīkst likt
    // Atrod pirmo brīvo bumbu un aktivizē to
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].aktivs) {
            bombs[i].aktivs = 1;
            bombs[i].x = c;
            bombs[i].y = r;
            bombs[i].timer = pl_fuse[cid];
            bombs[i].owner = cid;
            bombs[i].radiuss = pl_radiuss[cid];
            g_cfg.tiles[r][c] = TILE_BOMB;

            uint8_t buf[3];
            buf[0] = cid;
            uint16_t cell = make_cell_index(r, c, g_cfg.col);
            buf[1] = cell >> 8;
            buf[2] = cell & 0xFF;
            broadcast_to_all(MSG_BOMB, buf, 3);
            break;
        }
    }
}
// Apstrādā bumbu taimerus, sprādzienu izplatīšanos un spēlētāju bojāeju
static void tick_explosions(void) {
    TileType old[256][256];
    for (int r = 0; r < g_cfg.row; r++)
        for (int c = 0; c < g_cfg.col; c++)
            old[r][c] = g_cfg.tiles[r][c];// saglabājam kartes stāvokli pirms sprādzieniem

    Spragsti(&g_cfg, bombs, spradzieni);
    // Pārbauda, kas ir mainījies kartē sprādzienu rezultātā un paziņo pārējiem klientiem
    for (int r = 0; r < g_cfg.row; r++) {
        for (int c = 0; c < g_cfg.col; c++) {
            TileType before = old[r][c];
            TileType after = g_cfg.tiles[r][c];
            // Ja sprādziens iznīcina bloku, paziņo pārējiem
            if (before == TILE_BLOCK && after == TILE_FLOOR) {
                uint16_t cell = make_cell_index(r, c, g_cfg.col);
                uint8_t buf[2] = { cell >> 8, cell & 0xFF };
                broadcast_to_all(MSG_BLOCK_DESTROYED, buf, 2);
            }
            // Ja parādās sprādziens, paziņo pārējiem
            if (before != TILE_BOOM && after == TILE_BOOM) {
                uint16_t cell = make_cell_index(r, c, g_cfg.col);
                uint8_t buf[3] = { g_cfg.exp_distance, cell >> 8, cell & 0xFF };
                broadcast_to_all(MSG_EXPLOSION_START, buf, 3);
            }
            // Ja sprādziens beidzas un šūna kļūst par grīdu, paziņo pārējiem
            if (before == TILE_BOOM && after == TILE_FLOOR) {
                uint16_t cell = make_cell_index(r, c, g_cfg.col);
                uint8_t buf[3] = { g_cfg.exp_distance, cell >> 8, cell & 0xFF };
                broadcast_to_all(MSG_EXPLOSION_END, buf, 3);
            }
        }
    }
    // Pārbauda, kuri spēlētāji atrodas sprādzienā un nosūta viņiem nāves ziņojumu
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!alive[i]) continue;
        int r = p_row[i];
        int c = p_col[i];
        if (g_cfg.tiles[r][c] == TILE_BOOM) {
            alive[i] = 0;
            uint8_t buf[1] = { i };
            broadcast_to_all(MSG_DEATH, buf, 1);
        }
    }
}
// Apstrādā spēles loģiku, kontrolē laiku un nosaka spēles beigas
static void tick_logic(void) {
    if (server_status != GAME_RUNNING) return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    // Kontrolē, lai sprādzieni tiktu apstrādāti aptuveni ik pēc 50 ms
    long ms = (now.tv_sec - last_tick.tv_sec) * 1000 +
              (now.tv_nsec - last_tick.tv_nsec) / 1000000;

    if (ms < 50) return;
    last_tick = now;

    tick_explosions();

    int alive_now = 0;
    int last = -1;
    // Pārbauda, cik spēlētāji vēl ir dzīvi, un ja ir tikai viens vai nav neviena, beidz spēli
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].connected && alive[i]) {
            alive_now++;
            last = i;
        }
    }
    // Ja ir tikai viens vai nav neviena dzīva spēlētāja, beidz spēli un paziņo uzvarētāju
    if (alive_now <= 1)
        end_game(last);
}
// Apstrādā ienākošos ziņojumus no klientiem un izsauc atbilstošas funkcijas atkarībā no ziņojuma veida
static void handle_message(int cid, msg_header_t *h, uint8_t *payload) {

    if (h->msg_type == MSG_HELLO) {
        handle_hello(cid, h, payload);
        return;
    }

    if (h->msg_type == MSG_LEAVE) {
        remove_client(cid);
        alive[cid] = 0;
        return;
    }

    if (h->msg_type == MSG_PING) {
        send_msg(clients[cid].sock, MSG_PONG, SERVER_ID, cid, NULL, 0);
        return;
    }

    if (h->msg_type == MSG_SYNC_REQUEST) {
        send_sync_board_to_one(cid);
        return;
    }

    // hosta kartes izvēle
    if (h->msg_type == MSG_MAP_SELECT && cid == host_id && server_status == GAME_LOBBY) {
        if (payload[0] == '1') strcpy(selected_map, "maps/map1.cfg");
        else if (payload[0] == '2') strcpy(selected_map, "maps/map2.cfg");
        else if (payload[0] == '3') strcpy(selected_map, "maps/map3.cfg");
        DBG("Host selected map: %s\n", selected_map);
        return;
    }
    // Spēles statusa maiņa (sākšana) - tikai hostam atļauts, un tikai lobby stadijā
    if (server_status == GAME_LOBBY) {

        if(h->msg_type == MSG_SET_READY){
            pl_ready[cid] = 1;
            uint8_t buf[2] = {cid, 1};
            broadcast_to_all(MSG_SET_READY, buf, 2);
            return;
        }

        if (h->msg_type == MSG_SET_STATUS && payload[0] == GAME_RUNNING) {
            if (cid == host_id){
                int aready = 1;
                for(int i=0; i<MAX_PLAYERS; i++){
                    if(clients[i].connected && !pl_ready[i]){
                        DBG("Player %d not ready\n", i);
                        aready=0;
                        break;
                    }
                }
                if(aready) start_game();
            }
            return;
        }

        return;
    }
    // Ja spēle ir sākusies, apstrādā spēlētāju kustības un bumbu likšanas mēģinājumus
    if (server_status == GAME_RUNNING) {

        if (!alive[cid]) return;

        if (h->msg_type == MSG_MOVE_ATTEMPT)
            handle_move_attempt(cid, payload[0]);

        if (h->msg_type == MSG_BOMB_ATTEMPT)
            handle_bomb_attempt(cid);

        return;
    }
}

int main(void) {

    init_clients();
    memset(alive, 0, sizeof(alive));
    memset(bombs, 0, sizeof(bombs));
    memset(spradzieni, 0, sizeof(spradzieni));
    memset(bomb_count, 0, sizeof(bomb_count));

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));// ļauj ātri restartēt serveri bez "Address already in use" kļūdas

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));// piesaista servera socketu norādītajam portam
    listen(server_sock, 8);

    DBG("Server running on port %d\n", PORT);

    while (1) {

        tick_logic();

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server_sock, &rfds);
        int maxfd = server_sock;
        // Pievieno klientu socketus fd_set struktūrai, lai varētu pārbaudīt ienākošos ziņojumus
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!clients[i].connected) continue;
            FD_SET(clients[i].sock, &rfds);
            if (clients[i].sock > maxfd) maxfd = clients[i].sock;
        }

        struct timeval tv = {0, 50000};
        int sel = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (sel < 0) continue;
        // Ja ir jauns savienojums, pieņem to un reģistrē kā jaunu klientu
        if (FD_ISSET(server_sock, &rfds)) {
            int ns = accept(server_sock, NULL, NULL);
            int cid = add_client(ns);
            DBG("Client %d connected\n", cid);
        }
        // Pārbauda, kuri klientu socketi ir aktīvi un apstrādā ienākošos ziņojumus
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!clients[i].connected) continue;
            if (!FD_ISSET(clients[i].sock, &rfds)) continue;

            while (1) {
                msg_header_t h;
                uint8_t buf[65535];
                // Saņem ziņojumu no klienta, un ja savienojums ir pārtrūcis, atzīmē klientu kā atvienotu
                int r = recv_msg(clients[i].sock, &h, buf, sizeof(buf));
                if (r <= 0) {
                    DBG("Client %d disconnected\n", i);
                    remove_client(i);
                    alive[i] = 0;
                    break;
                }

                handle_message(i, &h, buf);

                int more = 0;
                ioctl(clients[i].sock, FIONREAD, &more);
                if (more <= 0) break;
            }
        }
    }

    close(server_sock);
    return 0;
}