#include "src/game.h"
#include "src/network.h"
#include "src/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DBG(...) fprintf(stderr, "[CLIENT] " __VA_ARGS__)
#ifndef MSG_MAP_SELECT
#define MSG_MAP_SELECT 0x10
#endif

int main(int argc, char *argv[]){
    char serverIP[64];
    // Ja IP adrese norādīta kā argumenta, izmanto to, citādi izmanto localhost
    if(argc>1) strcpy(serverIP, argv[1]);
    else strcpy(serverIP, "127.0.0.1");

    int port = 5000;

    int sock = socket(AF_INET, SOCK_STREAM, 0);// Izveido TCP socketu
    // Pārbauda, vai socket izveidošana izdevās
    if(sock<0){
        fprintf(stderr, "Socket error\n");
        return 1;
    }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    inet_pton(AF_INET, serverIP, &srv.sin_addr);// Pārvērš IP adresi binārā formā un sagatavo sockaddr_in struktūru savienojumam
    // Mēģina izveidot savienojumu ar serveri, un pārbauda, vai tas izdevās
    if(connect(sock, (struct sockaddr *)&srv, sizeof(srv))<0){
        fprintf(stderr, "Connection failed\n");
        return 1;
    }

    DBG("Connected\n");

    uint8_t hello[50] = {0};
    memcpy(hello, "LU_Client_2026", 14);// Sagatavo HELLO ziņu ar spēlētāja vārdu
    memcpy(hello+20, "Player", 6);// Spēlētāja vārds tiek sūtīts HELLO ziņojumā, sākot no 20. bita, lai atstātu vietu citiem datiem nākotnē
    send_msg(sock, MSG_HELLO, 0, SERVER_ID, hello, 50);

    GameConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    int id = -1;
    int is_host = 0;
    int game_status = GAME_LOBBY;

    int px[MAX_PLAYERS], py[MAX_PLAYERS];
    memset(px, 255, sizeof(px));
    memset(py, 255, sizeof(py));

    int got_welcome = 0;
    int got_map = 0;

    int selected_map = 1; // 1..3

    // sagaidām WELCOME
    while(!got_welcome){
        msg_header_t h;
        uint8_t buf[65536];

        int r = recv_msg(sock, &h, buf, sizeof(buf));
        if(r <= 0){// Ja savienojums ir pārtrūcis, iziet no programmas
            fprintf(stderr, "Disconnected before WELCOME\n");
            return 1;
        }
        // Apstrādā WELCOME ziņojumu, iegūstot spēlētāja ID, hosta statusu, un citu informāciju
        if(h.msg_type == MSG_WELCOME){
            id = h.target_id;
            is_host = (id == 0);
            got_welcome = 1;
        }
        // Apstrādā SET_STATUS ziņojumu, atjauninot spēles statusu
        if(h.msg_type == MSG_SET_STATUS){
            game_status = buf[0];
        }
        // Apstrādā MAP_SELECT ziņojumu, atjauninot izvēlēto karti
        if(h.msg_type == MSG_MAP){
            int pos = 0;
            cfg.row          = buf[pos++];// Pārkopē kartes datus no saņemtā bufera uz config struktūru
            cfg.col          = buf[pos++];
            cfg.pl_speed     = buf[pos++];
            cfg.exp_distance = buf[pos++];
            cfg.exp_danger   = buf[pos++];
            cfg.fuse_time    = buf[pos++];
            // Pārkopē kartes datus no saņemtā bufera uz config struktūru
            for(int y=0;y<cfg.row;y++)
                for(int x=0;x<cfg.col;x++)
                    cfg.tiles[y][x] = buf[pos++];
            // Pārkopē spēlētāju pozīcijas no saņemtā bufera uz px un py masīviem
            for(int i=0;i<MAX_PLAYERS;i++){
                px[i] = buf[pos++];
                py[i] = buf[pos++];
            }

            got_map = 1;
        }
    }

    initscr();// Inicializē ncurses režīmu
    cbreak();// Iespējo cbreak režīmu, kas ļauj lasīt ievadi bez enter nospiešanas
    noecho();// Izslēdz ievades atspoguļošanu ekrānā
    keypad(stdscr, TRUE);// Iespējo speciālo taustiņu atpazīšanu (piemēram, bultiņas)
    curs_set(0);// Paslēpj kursoru

lobby_start:
    game_status = GAME_LOBBY;
    got_map = 0;

    clear();
    refresh();

    while(game_status == GAME_LOBBY){

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv = {0, 100000};

        if(select(sock+1, &fds, NULL, NULL, &tv) > 0){
            while(1){
                msg_header_t h;
                uint8_t buf[65536];

                int r = recv_msg(sock, &h, buf, sizeof(buf));
                if(r <= 0) break;

                if(h.msg_type == MSG_SET_STATUS){
                    game_status = buf[0];
                }

                if(h.msg_type == MSG_MAP){
                    int pos = 0;
                    cfg.row          = buf[pos++];
                    cfg.col          = buf[pos++];
                    cfg.pl_speed     = buf[pos++];
                    cfg.exp_distance = buf[pos++];
                    cfg.exp_danger   = buf[pos++];
                    cfg.fuse_time    = buf[pos++];

                    for(int y=0;y<cfg.row;y++)
                        for(int x=0;x<cfg.col;x++)
                            cfg.tiles[y][x] = buf[pos++];

                    for(int i=0;i<MAX_PLAYERS;i++){
                        px[i] = buf[pos++];
                        py[i] = buf[pos++];
                    }

                    got_map = 1;
                }

                if(h.msg_type == MSG_BONUS_AVAILABLE){
                    uint8_t t = buf[0];
                    uint16_t cell = (buf[1]<<8) | buf[2];
                    uint16_t ry, cx;
                    split_cell_index(cell, cfg.col, &ry, &cx);
                    cfg.tiles[ry][cx] = (TileType)t;
                }

                int more=0;
                ioctl(sock, FIONREAD, &more);
                if(more<=0) break;
            }
        }

        clear();
        mvprintw(0,0,"Bomberman LOBBY");
        mvprintw(1,0,"Your ID: %d   Host: %d", id, is_host);
        mvprintw(3,0,"Press S to start (host only)");
        mvprintw(4,0,"Press Q to quit");

        if(is_host){
            mvprintw(6,0,"Map selection (host):");
            mvprintw(7,0,"1 - map1.cfg");
            mvprintw(8,0,"2 - map2.cfg");
            mvprintw(9,0,"3 - map3.cfg");
            mvprintw(11,0,"Current: %d", selected_map);
        }

        refresh();

        int ch = getch();

        if(ch=='q'){
            endwin();
            close(sock);
            return 0;
        }

        if(is_host){
            if(ch=='1' || ch=='2' || ch=='3'){
                selected_map = ch - '0';
                uint8_t v = ch;
                send_msg(sock, MSG_MAP_SELECT, id, SERVER_ID, &v, 1);
            }
            if(ch=='s'){
                uint8_t st = GAME_RUNNING;
                send_msg(sock, MSG_SET_STATUS, id, SERVER_ID, &st, 1);
            }
        }
    }

    while(!got_map){
        msg_header_t h;
        uint8_t buf[65536];

        int r = recv_msg(sock, &h, buf, sizeof(buf));
        if(r<=0){
            endwin();
            close(sock);
            return 1;
        }

        if(h.msg_type == MSG_MAP){
            int pos = 0;
            cfg.row          = buf[pos++];
            cfg.col          = buf[pos++];
            cfg.pl_speed     = buf[pos++];
            cfg.exp_distance = buf[pos++];
            cfg.exp_danger   = buf[pos++];
            cfg.fuse_time    = buf[pos++];

            for(int y=0;y<cfg.row;y++)
                for(int x=0;x<cfg.col;x++)
                    cfg.tiles[y][x] = buf[pos++];

            for(int i=0;i<MAX_PLAYERS;i++){
                px[i] = buf[pos++];
                py[i] = buf[pos++];
            }

            got_map = 1;
        }
    }

    WINDOW *win = newwin(cfg.row+2, cfg.col*2+1, 1, 0);
    keypad(win, TRUE);
    wtimeout(win, 50);

    while(1){

        int ch = wgetch(win);

        if(px[id] != 255 && py[id] != 255){
            if(ch=='w'){ uint8_t v='U'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v,1); }
            if(ch=='s'){ uint8_t v='D'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v,1); }
            if(ch=='a'){ uint8_t v='L'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v,1); }
            if(ch=='d'){ uint8_t v='R'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v,1); }
            if(ch==' '){ uint8_t v=0;   send_msg(sock, MSG_BOMB_ATTEMPT, id, SERVER_ID, &v,1); }
        }

        if(ch=='q'){
            endwin();
            close(sock);
            return 0;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv = {0, 10000};

        if(select(sock+1, &fds, NULL, NULL, &tv) > 0){
            while(1){
                msg_header_t h;
                uint8_t buf[65536];

                int r = recv_msg(sock, &h, buf, sizeof(buf));
                if(r<=0) break;

                if(h.msg_type == MSG_MOVED){
                    int pid = buf[0];
                    uint16_t cell = (buf[1]<<8) | buf[2];
                    uint16_t ry, cx;
                    split_cell_index(cell, cfg.col, &ry, &cx);
                    py[pid] = ry;
                    px[pid] = cx;
                }

                if(h.msg_type == MSG_DEATH){
                    int pid = buf[0];
                    px[pid] = py[pid] = 255;
                }

                if(h.msg_type == MSG_BLOCK_DESTROYED){
                    uint16_t cell = (buf[0]<<8) | buf[1];
                    uint16_t ry, cx;
                    split_cell_index(cell, cfg.col, &ry, &cx);
                    cfg.tiles[ry][cx] = TILE_FLOOR;
                }

                if(h.msg_type == MSG_BOMB){
                    int pid = buf[0];
                    (void)pid;
                    uint16_t cell = (buf[1]<<8) | buf[2];
                    uint16_t ry, cx;
                    split_cell_index(cell, cfg.col, &ry, &cx);
                    cfg.tiles[ry][cx] = TILE_BOMB;
                }

                if(h.msg_type == MSG_EXPLOSION_START){
                    uint8_t dist = buf[0];
                    (void)dist;
                    uint16_t cell = (buf[1]<<8) | buf[2];
                    uint16_t ry, cx;
                    split_cell_index(cell, cfg.col, &ry, &cx);
                    cfg.tiles[ry][cx] = TILE_BOOM;
                }

                if(h.msg_type == MSG_EXPLOSION_END){
                    uint8_t dist = buf[0];
                    (void)dist;
                    uint16_t cell = (buf[1]<<8) | buf[2];
                    uint16_t ry, cx;
                    split_cell_index(cell, cfg.col, &ry, &cx);
                    if(cfg.tiles[ry][cx] == TILE_BOOM)
                        cfg.tiles[ry][cx] = TILE_FLOOR;
                }

                if(h.msg_type == MSG_BONUS_RETRIEVED){
                    int pid = buf[0];
                    (void)pid;
                    TileType t = buf[1];
                    (void)t;
                    uint16_t cell = (buf[2]<<8) | buf[3];
                    uint16_t ry, cx;
                    split_cell_index(cell, cfg.col, &ry, &cx);
                    cfg.tiles[ry][cx] = TILE_FLOOR;
                }

                if(h.msg_type == MSG_BONUS_AVAILABLE){
                    uint8_t t = buf[0];
                    uint16_t cell = (buf[1]<<8) | buf[2];
                    uint16_t ry, cx;
                    split_cell_index(cell, cfg.col, &ry, &cx);
                    cfg.tiles[ry][cx] = (TileType)t;
                }

                if(h.msg_type == MSG_MAP){
                    int pos = 0;
                    cfg.row          = buf[pos++];
                    cfg.col          = buf[pos++];
                    cfg.pl_speed     = buf[pos++];
                    cfg.exp_distance = buf[pos++];
                    cfg.exp_danger   = buf[pos++];
                    cfg.fuse_time    = buf[pos++];

                    for(int y=0;y<cfg.row;y++)
                        for(int x=0;x<cfg.col;x++)
                            cfg.tiles[y][x] = buf[pos++];

                    for(int i=0;i<MAX_PLAYERS;i++){
                        px[i] = buf[pos++];
                        py[i] = buf[pos++];
                    }
                }

                if(h.msg_type == MSG_WINNER){
                    clear();
                    if(buf[0] == id)
                        mvprintw(LINES/2, COLS/2-5, "YOU WIN!");
                    else
                        mvprintw(LINES/2, COLS/2-5, "YOU DIED");

                    refresh();
                    sleep(4);
                    delwin(win);
                    goto lobby_start;
                }

                int more=0;
                ioctl(sock, FIONREAD, &more);
                if(more<=0) break;
            }
        }

        werase(win);

        for(int y=0;y<cfg.row;y++){
            for(int x=0;x<cfg.col;x++){
                char c='.';
                switch(cfg.tiles[y][x]){
                    case TILE_WALL:       c='H'; break;
                    case TILE_BLOCK:      c='S'; break;
                    case TILE_BOMB:       c='B'; break;
                    case TILE_FASTER:     c='A'; break;
                    case TILE_BIGGER:     c='R'; break;
                    case TILE_LONGER:     c='T'; break;
                    case TILE_MOREBOMBS:  c='N'; break;
                    case TILE_BOOM:       c='*'; break;
                    default:              c='.'; break;
                }
                mvwaddch(win, y, x*2, c);
            }
        }

        for(int i=0;i<MAX_PLAYERS;i++){
            if(px[i]!=255 && py[i]!=255){
                mvwaddch(win, py[i], px[i]*2, (i==id?'@':'0'+i));
            }
        }

        wrefresh(win);
    }

    endwin();
    close(sock);
    return 0;
}