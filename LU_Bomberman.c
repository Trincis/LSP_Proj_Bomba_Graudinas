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
#include <arpa/inet.h>
#include <unistd.h>
/**
Kartes simboli:
H - ciets
S - miksts
1...8 speletajs (moz krasainus?)
B - bumba
R - radiusa palielinasana
T - bumbas atskaites laika palielinasana
**/

int main(int argc, char *argv[]){
///servera saslēgšana
    char serverIP[64];
    if(argc>1){
        strcpy(serverIP, argv[1]);
    }
    else strcpy(serverIP, "127.0.0.1");

    int port = 5000; ///Barbar? approve?

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

    GameConfig config;
    if(game_config_load(&config, "map.cfg")!=0){
        fprintf(stderr, "Failed to load map.cfg\n");
        return 1;
    }

    Player speletaji[MAX_PLAYERS];

    for(int i=0; i<MAX_PLAYERS; i++){
        speletaji[i].id = i;
        speletaji[i].x = config.player_spawn_x[i];
        speletaji[i].y = config.player_spawn_y[i];
        speletaji[i].dzivs = (config.player_spawn_x[i]!=-1); 
    }
    Bomb bumbas[MAX_BOMBS];

    for(int i=0; i<MAX_BOMBS; i++){
        bumbas[i].aktivs = 0;
        bumbas[i].x = -1;
        bumbas[i].y = -1;
        bumbas[i].timer = 0;
    }

    BOOM spradzieni[MAX_BOOM];
    for(int i=0; i<MAX_BOOM; i++){
        spradzieni[i].aktivs=0;
        spradzieni[i].x=-1;
        spradzieni[i].y=-1;
        spradzieni[i].timer=0;
    }

    send_msg(sock, MSG_HELLO, 0, SERVER_ID, NULL, 0);
    int id = -1;

    while(id==-1||config.row==0){
        msg_header_t h;
        uint8_t buff[65536];
        if(recv_msg(sock, &h, buff, sizeof(buff))>0){
            if(h.msg_type==MSG_WELCOME){
                id = h.target_id;
                fprintf(stderr, "Pieslēdzies kā spēlētājs nr. %d\n", id);
            }
            if(h.msg_type==MSG_MAP){
                int pos = 0;
                config.row = buff[pos++];
                config.col = buff[pos++];
                for(int y = 0; y<config.row; y++){
                    for(int x = 0; x<config.col; x++){
                        config.tiles[y][x] = buff[pos++];
                    }
                }
                for(int i = 0; i<MAX_PLAYERS; i++){
                    speletaji[i].x = buff[pos++];
                    speletaji[i].y = buff[pos++];
                    speletaji[i].dzivs = (speletaji[i].x!=255);
                }
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

    ///ekrāns
    WINDOW *win = newwin(config.row+2, config.col*2+1, 1, 0);
    if(win == NULL){
        endwin();
        fprintf(stderr, "Failed to create window\n");
        return 1;
    }

    keypad(win, true);

    ///kartes renderēšana
    map_render(win, &config);
    players_render(win, speletaji, MAX_PLAYERS);
    bombs_render(win, bumbas, MAX_BOMBS); 


    timeout(100);
    int tick = 0;
    ///sagaidīt pogas spiedienu
    int ch;
    while((ch = wgetch(win))!='q'){
        switch(ch){
            case KEY_UP:
            case 'w':{
                ///MSG_MOVE_ATTEMPT ziņa augšup serverim

                uint8_t virz = 'U';
                send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &virz, 1);

                ///"Sobrīd lokāļas darbības"
                /*int ny = speletaji[0].y-1;
                if(ny>=0&&config.tiles[ny][speletaji[0].x]==TILE_FLOOR){
                    speletaji[0].y = ny;
                }*/
                
                break;
            }
            case KEY_DOWN:
            case 's':{
                ///MSG_MOVE_ATTEMPT ziņa lejup serverim

                uint8_t virz = 'D';
                send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &virz, 1);

                 ///"Sobrīd lokāļas darbības"
                /*int ny = speletaji[0].y+1;
                if(ny<config.row&&config.tiles[ny][speletaji[0].x]==TILE_FLOOR){
                    speletaji[0].y = ny;
                }*/
                
                break;
            }
            case KEY_LEFT:
            case 'a':{
                ///MSG_MOVE_ATTEMPT ziņa pa kreisi serverim

                uint8_t virz = 'L';
                send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &virz, 1);

                 ///"Sobrīd lokāļas darbības"
                /*int nx = speletaji[0].x-1;
                if(nx>=0&&config.tiles[speletaji[0].y][nx]==TILE_FLOOR){
                    speletaji[0].x = nx;
                }*/
                
                break;
            }
            case KEY_RIGHT:
            case 'd':{
                ///MSG_MOVE_ATTEMPT ziņa pa labi serverim

                uint8_t virz = 'R';
                send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &virz, 1);

                /*int nx = speletaji[0].x+1;
                if(nx<config.col&&config.tiles[speletaji[0].y][nx]==TILE_FLOOR){
                    speletaji[0].x = nx;
                }*/
                
                break;
            }
            case ' ':{
                ///MSG_BOMB_ATTEMPT ziņa bumbot serverim

                send_msg(sock, MSG_BOMB_ATTEMPT, id, SERVER_ID, NULL, 0);

                /*for(int i=0; i<MAX_BOMBS; i++){
                    if(!bumbas[i].aktivs){
                        bumbas[i].x = speletaji[0].x;
                        bumbas[i].y = speletaji[0].y;
                        bumbas[i].aktivs = 1;
                        bumbas[i].timer = config.fuse_time*10;
                        break;
                    }
                }*/

                break;
            }
        }

        ///apstrādāt servera ziņojumus
        fd_set fds;
        struct timeval tv = {0,0};
        FD_ZERO(&fds);
        FD_SET(sock, &fds);

        if(select(sock+1, &fds, NULL, NULL, &tv)>0){
            msg_header_t sh;
            uint8_t sbuff[65536];

            if(recv_msg(sock, &sh, sbuff, sizeof(sbuff))>0){
                switch(sh.msg_type){
                    case MSG_MAP:{
                        int pos = 0;
                        config.row = sbuff[pos++];
                        config.col = sbuff[pos++];
                        for(int y = 0; y<config.row; y++){
                            for(int x = 0; x<config.col; x++){
                                config.tiles[y][x] = sbuff[pos++];
                            }
                        }
                        for(int i = 0; i<MAX_PLAYERS; i++){
                            speletaji[i].x = sbuff[pos++];
                            speletaji[i].y = sbuff[pos++];
                            speletaji[i].dzivs = (speletaji[i].x!=255);
                        }
                        break;
                    }
                    case MSG_MOVED:{
                        uint8_t pid = sbuff[0];
                        uint8_t nx = sbuff[1];
                        uint8_t ny = sbuff[2];

                        speletaji[pid].x = nx;
                        speletaji[pid].y = ny;
                        break;
                    }
                    case MSG_BOMB:{
                        for(int i=0; i<MAX_BOMBS; i++){
                            if(!bumbas[i].aktivs){
                                bumbas[i].x=sbuff[0];
                                bumbas[i].y=sbuff[1];
                                bumbas[i].aktivs=1;
                                break;
                            }
                        }
                        break;
                    }
                    case MSG_EXPLOSION_START:{
                        config.tiles[sbuff[1]][sbuff[0]]=TILE_BOOM;
                        break;
                    }
                    case MSG_EXPLOSION_END:{
                        config.tiles[sbuff[1]][sbuff[0]]=TILE_FLOOR;
                        break;
                    }
                    case MSG_BLOCK_DESTROYED:{
                        config.tiles[sbuff[1]][sbuff[0]]=TILE_FLOOR;
                        break;
                    }
                    case MSG_DEATH:{
                        uint8_t pid=sbuff[0];
                        speletaji[pid].dzivs=0;
                        break;
                    }
                    case MSG_WINNER:{
                        ///uzvaras ekrāns
                        break;
                    }
                    case MSG_DISCONNECT:{
                        goto cleanup;
                    }
                }
            }
        }

/*
        tick++;
        for(int i=0; i<MAX_BOMBS; i++){
            if(bumbas[i].aktivs){
                bumbas[i].timer--;
                if(bumbas[i].timer<=0){
                    Spragsti(&config, &bumbas[i], spradzieni, MAX_BOOM);
                }
            }
        }

        for(int i=0; i<MAX_BOOM; i++){
            if(spradzieni[i].aktivs){
                spradzieni[i].timer--;
                if(spradzieni[i].timer<=0){
                    config.tiles[spradzieni[i].y][spradzieni[i].x]=TILE_FLOOR;
                    spradzieni[i].aktivs=0;
                }
            }
        }
*/

    map_render(win, &config);
    players_render(win, speletaji, MAX_PLAYERS);
    bombs_render(win, bumbas, MAX_BOMBS);

    }

    ///spēles kods
    cleanup:
        send_msg(sock, MSG_LEAVE, id, SERVER_ID, NULL, 0);
        close(sock);
        delwin(win);
        endwin();
        return 0;

    
}
