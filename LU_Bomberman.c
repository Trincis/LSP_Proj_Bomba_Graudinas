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
    int px[MAX_PLAYERS], py[MAX_PLAYERS];

    int got_welcome = 0;
    int got_map = 0;

    while(!got_welcome || !got_map){
        msg_header_t h;
        uint8_t buff[65536];

        if(recv_msg(sock, &h, buff, sizeof(buff)) > 0){

            if(h.msg_type == MSG_WELCOME){
                id = h.target_id;
                got_welcome = 1;
            }

            if(h.msg_type == MSG_MAP){
                int pos = 0;
                config.row = buff[pos++];
                config.col = buff[pos++];

                for(int y=0; y<config.row; y++)
                    for(int x=0; x<config.col; x++)
                        config.tiles[y][x] = buff[pos++];

                for(int i=0; i<MAX_PLAYERS; i++){
                    px[i] = buff[pos++];
                    py[i] = buff[pos++];
                }

                got_map = 1;
            }
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
    timeout(50);

    while(1){

        // 🔥 VISPIRMS APSTRĀDĀ VISAS SERVERA ZIŅAS
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv = {0,0};

        if(select(sock+1, &fds, NULL, NULL, &tv) > 0){

            while(1){
                msg_header_t h;
                uint8_t sbuff[65536];

                int r = recv_msg(sock, &h, sbuff, sizeof(sbuff));
                if(r <= 0) break;

                if(h.msg_type == MSG_MAP){
                    int pos = 0;
                    config.row = sbuff[pos++];
                    config.col = sbuff[pos++];

                    for(int y=0; y<config.row; y++)
                        for(int x=0; x<config.col; x++)
                            config.tiles[y][x] = sbuff[pos++];

                    for(int i=0; i<MAX_PLAYERS; i++){
                        px[i] = sbuff[pos++];
                        py[i] = sbuff[pos++];
                    }

                    fprintf(stderr, "[CLIENT DEBUG] GOT MAP, MY POS = (%d,%d)\n", px[id], py[id]);
                }

                int more = 0;
                ioctl(sock, FIONREAD, &more);
                if(more <= 0) break;
            }
        }

        // 🔥 TAGAD NOLASI TAUSTIŅU
        int ch = wgetch(win);

        if(ch == 'q') break;

        if(ch == 'w'){ uint8_t v='U'; fprintf(stderr,"[CLIENT DEBUG] SEND U\n"); send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
        if(ch == 's'){ uint8_t v='D'; fprintf(stderr,"[CLIENT DEBUG] SEND D\n"); send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
        if(ch == 'a'){ uint8_t v='L'; fprintf(stderr,"[CLIENT DEBUG] SEND L\n"); send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
        if(ch == 'd'){ uint8_t v='R'; fprintf(stderr,"[CLIENT DEBUG] SEND R\n"); send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }

        // 🔥 TIKAI TAGAD ZĪMĒ KARTI
        werase(win);

        for(int y=0; y<config.row; y++){
            for(int x=0; x<config.col; x++){
                char c='.';
                switch(config.tiles[y][x]){
                    case TILE_WALL:   c='H'; break;
                    case TILE_BLOCK:  c='S'; break;
                    case TILE_BOMB:   c='B'; break;
                    case TILE_FASTER: c='A'; break;
                    case TILE_BIGGER: c='R'; break;
                    case TILE_LONGER: c='T'; break;
                    case TILE_BOOM:   c='*'; break;
                }
                mvwaddch(win, y, x*2, c);
            }
        }

        for(int i=0; i<MAX_PLAYERS; i++){
            if(px[i] != 255 && py[i] != 255){
                mvwaddch(win, py[i], px[i]*2, (i==id ? '@' : '0'+i));
            }
        }

        wrefresh(win);
    }

cleanup:
    send_msg(sock, MSG_LEAVE, id, SERVER_ID, NULL, 0);
    close(sock);
    delwin(win);
    endwin();
    return 0;
}
