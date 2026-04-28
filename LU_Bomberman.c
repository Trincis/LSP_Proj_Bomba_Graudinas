/*
klient =  ./client 2> client.log
serveris =   ./bomberman_server
*/

/**
LU_Bomberman.c

Programma ir spēle balstīta uz retro spēli Bomberman

kustās ar wasd
bumbas liek ar space
no spēles iziet ar q

Spēles laukuma konfigurācija - fails "map.cfg"

Autori: Barbara Terēze Graudiņa, bg24008
        Katrīna Anna Graudiņa, kg21076

Programma izveidota: ??.04.2026
**/

#include "src/game.h"
#include "src/network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>

/**
Kartes simboli:
H - ciets
S - miksts
1...8 speletajs
B - bumba
R - radiusa palielinasana
T - bumbas atskaites laika palielinasana
**/

int main(int argc, char *argv[]){
    char serverIP[64];
    if(argc>1) strcpy(serverIP, argv[1]);
    else strcpy(serverIP, "127.0.0.1");

    int port = 5000;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock<0){
        fprintf(stderr, "Neizveidoja socketu\n");
        return 1;
    }

    struct sockaddr_in srv = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    inet_pton(AF_INET, serverIP, &srv.sin_addr);

    if(connect(sock, (struct sockaddr *)&srv, sizeof(srv))<0){
        fprintf(stderr, "nepieslēdzās serverim\n");
        return 1;
    }

    send_msg(sock, MSG_HELLO, 0, SERVER_ID, NULL, 0);

    GameConfig config;
    memset(&config, 0, sizeof(config));

    int id = -1;
    int px[MAX_PLAYERS], py[MAX_PLAYERS], patrums[MAX_PLAYERS], pradiuss[MAX_PLAYERS];

    int got_welcome = 0;
    int got_map = 0;

    while(!got_welcome || !got_map){
        msg_header_t h;
        uint8_t buff[65536];

        if(recv_msg(sock, &h, buff, sizeof(buff)) <= 0){
            fprintf(stderr, "Savienojums pārtrūka\n");
            close(sock);
            return 1;
        }

        if(h.msg_type == MSG_WELCOME){
            id = h.target_id;
            got_welcome = 1;
        }

        if(h.msg_type == MSG_MAP){
            int pos = 0;
            config.row = buff[pos++];
            config.col = buff[pos++];
            config.pl_speed = buff[pos++];
            config.exp_distance = buff[pos++];
            config.exp_danger = buff[pos++];
            config.fuse_time = buff[pos++];

            for(int y=0; y<config.row; y++)
                for(int x=0; x<config.col; x++)
                    config.tiles[y][x] = buff[pos++];

            for(int i=0; i<MAX_PLAYERS; i++){
                px[i] = buff[pos++];
                py[i] = buff[pos++];
                patrums[i]  = config.pl_speed;
                pradiuss[i] = config.exp_distance;
            }

            got_map = 1;
        }
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    clear();
    refresh();

    WINDOW *win = newwin(config.row+2, config.col*2+1, 1, 0);
    keypad(win, TRUE);
    nodelay(win, FALSE);
    wtimeout(win, 50);

    while (1) {

        // 1) NOLASI TAUSTIŅU
        int ch = wgetch(win);

        if(ch == 'q') break;

        if(ch == 'w'){ uint8_t v='U'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
        if(ch == 's'){ uint8_t v='D'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
        if(ch == 'a'){ uint8_t v='L'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
        if(ch == 'd'){ uint8_t v='R'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
        if(ch == ' '){ uint8_t v=0;  send_msg(sock, MSG_BOMB_ATTEMPT, id, SERVER_ID, &v, 1); }

        // 2) GAIDĀM MAP NO SERVERA
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);

        struct timeval tv = {0, 200000}; // 200ms timeout

        int sel = select(sock+1, &fds, NULL, NULL, &tv);

        if(sel > 0 && FD_ISSET(sock, &fds)){
            while(1){
                msg_header_t h;
                uint8_t sbuff[65536];

                int r = recv_msg(sock, &h, sbuff, sizeof(sbuff));
                if(r <= 0) break;

                if(h.msg_type == MSG_MAP){
                    int pos = 0;
                    config.row = sbuff[pos++];
                    config.col = sbuff[pos++];
                    config.pl_speed = sbuff[pos++];
                    config.exp_distance = sbuff[pos++];
                    config.exp_danger = sbuff[pos++];
                    config.fuse_time = sbuff[pos++];


                    for(int y=0; y<config.row; y++)
                        for(int x=0; x<config.col; x++)
                            config.tiles[y][x] = sbuff[pos++];

                    for(int i=0; i<MAX_PLAYERS; i++){
                        px[i] = sbuff[pos++];
                        py[i] = sbuff[pos++];
                    }
                }
                if(h.msg_type == MSG_BONUS_RETRIEVED){
                    uint8_t pid = sbuff[0];
                    uint8_t btype = sbuff[1];

                    if(btype == TILE_FASTER) patrums[pid]++;
                    if(btype == TILE_LONGER) config.fuse_time++;///vai tas tā domāts?
                    if(btype == TILE_BIGGER) pradiuss[pid]++; 
                }

                int more = 0;
                ioctl(sock, FIONREAD, &more);
                if(more <= 0) break;
            }
        }

        // 3) ZĪMĒ KARTI
        werase(win);

        for(int y=0; y<config.row; y++){
            for(int x=0; x<config.col; x++){
                char c = config.tiles[y][x];
                mvwaddch(win, y, x*2, c);
            }
        }

        for(int i=0; i<MAX_PLAYERS; i++){
            if(px[i] != 255 && py[i] != 255){
                mvwaddch(win, py[i], px[i]*2, (i==id ? '@' : '0'+i));
            }
        }

        mvwprintw(win, config.row+1, 0 ,"Speed: %d Radius: %d", patrums[id], pradiuss[id]);
        wrefresh(win);
    }


    send_msg(sock, MSG_LEAVE, id, SERVER_ID, NULL, 0);
    close(sock);
    delwin(win);
    endwin();
    return 0;
}