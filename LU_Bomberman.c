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

#define GAME_LOBBY   0
#define GAME_RUNNING 1
#define GAME_END     2

#define MSG_MAP_SELECT   200

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

    DBG("Connected to server\n");

    send_msg(sock, MSG_HELLO, 0, SERVER_ID, NULL, 0);

    GameConfig config;
    memset(&config, 0, sizeof(config));

    int id = -1;
    int px[MAX_PLAYERS], py[MAX_PLAYERS];
    int patrums[MAX_PLAYERS], pradiuss[MAX_PLAYERS];

    (void)patrums;
    (void)pradiuss;

    int got_welcome = 0;
    int got_map = 0;
    int game_status = GAME_LOBBY;
    int is_host = 0;
    int selected_map = 0;
    int ready_flags[MAX_PLAYERS];
    memset(ready_flags, 0, sizeof(ready_flags));

    for(int i=0;i<MAX_PLAYERS;i++){
        px[i] = py[i] = 255;
        patrums[i] = 1;
        pradiuss[i] = 1;
    }

    //sagaidam welcome, kurā būs mūsu ID un vai esam host
    while(!got_welcome){
        msg_header_t h;
        uint8_t buff[65536];

        int r = recv_msg(sock, &h, buff, sizeof(buff));
        if(r <= 0){
            DBG("Savienojums pārtrūka pirms WELCOME\n");
            close(sock);
            return 1;
        }

        if(h.msg_type == MSG_WELCOME){
            id = h.target_id;
            is_host = (id == 0);
            got_welcome = 1;
        }

        if(h.msg_type == MSG_SET_STATUS){
            game_status = buff[0];
        }

        if(h.msg_type == MSG_MAP_SELECT){
            selected_map = buff[0];
        }

        if(h.msg_type == MSG_MAP){
            int pos = 0;
            config.row          = buff[pos++];
            config.col          = buff[pos++];
            config.pl_speed     = buff[pos++];
            config.exp_distance = buff[pos++];
            config.exp_danger   = buff[pos++];
            config.fuse_time    = buff[pos++];

            for(int y=0; y<config.row; y++)
                for(int x=0; x<config.col; x++)
                    config.tiles[y][x] = (TileType)buff[pos++];

            for(int i=0; i<MAX_PLAYERS; i++){
                px[i] = buff[pos++];
                py[i] = buff[pos++];
            }

            got_map = 1;
        }
    }

    //Ncurses start
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    clear();
    refresh();

lobby_start:
    // reset stāvokli nākamajai spēlei
    game_status = GAME_LOBBY;
    got_map = 0;
    for(int i=0;i<MAX_PLAYERS;i++){
        ready_flags[i] = 0;
        px[i] = py[i] = 255;
    }

    // 3) LOBBY CIKLS
    while(game_status == GAME_LOBBY){

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv = {0, 100000};

        if(select(sock+1, &fds, NULL, NULL, &tv) > 0 && FD_ISSET(sock, &fds)){
            while(1){
                msg_header_t h;
                uint8_t sbuff[65536];

                int r = recv_msg(sock, &h, sbuff, sizeof(sbuff));
                if(r <= 0) break;

                if(h.msg_type == MSG_SET_STATUS){
                    game_status = sbuff[0];
                }

                if(h.msg_type == MSG_SET_READY){
                    int pid = sbuff[0];
                    int val = sbuff[1];
                    if(pid >= 0 && pid < MAX_PLAYERS)
                        ready_flags[pid] = val;
                }

                if(h.msg_type == MSG_MAP_SELECT){
                    selected_map = sbuff[0];
                }

                if(h.msg_type == MSG_MAP){
                    int pos = 0;
                    config.row          = sbuff[pos++];
                    config.col          = sbuff[pos++];
                    config.pl_speed     = sbuff[pos++];
                    config.exp_distance = sbuff[pos++];
                    config.exp_danger   = sbuff[pos++];
                    config.fuse_time    = sbuff[pos++];

                    for(int y=0; y<config.row; y++)
                        for(int x=0; x<config.col; x++)
                            config.tiles[y][x] = (TileType)sbuff[pos++];

                    for(int i=0; i<MAX_PLAYERS; i++){
                        px[i] = sbuff[pos++];
                        py[i] = sbuff[pos++];
                    }

                    got_map = 1;
                }

                int more = 0;
                ioctl(sock, FIONREAD, &more);
                if(more <= 0) break;
            }
        }

        clear();
        mvprintw(0, 0, "Bomberman LOBBY");
        mvprintw(1, 0, "ID=%d HOST=%d", id, is_host);
        mvprintw(2, 0, "Selected map: %d", selected_map);

        int line = 4;
        mvprintw(line++, 0, "Players (READY):");
        for(int i=0; i<MAX_PLAYERS; i++)
            mvprintw(line++, 0, "  %d : %s", i, ready_flags[i] ? "READY" : "NOT READY");

        mvprintw(line+1, 0, "Keys:");
        mvprintw(line+2, 0, "  r - toggle READY");
        if(is_host){
            mvprintw(line+3, 0, "  1/2/3 - select map");
        }
        mvprintw(line+5, 0, "  q - quit");

        refresh();

        int ch = getch();

        if(ch == 'q'){
            endwin();
            close(sock);
            return 0;
        }

        if(ch == 'r'){
            ready_flags[id] = !ready_flags[id];
            uint8_t p[1] = { (uint8_t)ready_flags[id] };
            send_msg(sock, MSG_SET_READY, id, SERVER_ID, p, 1);
        }

        if(is_host && (ch=='1' || ch=='2' || ch=='3')){
            int idx = ch - '1';
            uint8_t p[1] = { (uint8_t)idx };
            send_msg(sock, MSG_MAP_SELECT, id, SERVER_ID, p, 1);
        }
    }

    //gaidam map ja nav tāda
    while(!got_map){
        msg_header_t h;
        uint8_t sbuff[65536];

        int r = recv_msg(sock, &h, sbuff, sizeof(sbuff));
        if(r <= 0){
            endwin();
            close(sock);
            return 1;
        }

        if(h.msg_type == MSG_MAP){
            int pos = 0;
            config.row          = sbuff[pos++];
            config.col          = sbuff[pos++];
            config.pl_speed     = sbuff[pos++];
            config.exp_distance = sbuff[pos++];
            config.exp_danger   = sbuff[pos++];
            config.fuse_time    = sbuff[pos++];

            for(int y=0; y<config.row; y++)
                for(int x=0; x<config.col; x++)
                    config.tiles[y][x] = (TileType)sbuff[pos++];

            for(int i=0; i<MAX_PLAYERS; i++){
                px[i] = sbuff[pos++];
                py[i] = sbuff[pos++];
            }

            got_map = 1;
        }
    }

    //spēles cikls
    WINDOW *win = newwin(config.row+2, config.col*2+1, 1, 0);
    keypad(win, TRUE);
    wtimeout(win, 50);

    werase(win);
    map_render(win, &config);

    for(int i=0; i<MAX_PLAYERS; i++){
        if(px[i] != 255 && py[i] != 255){
            mvwaddch(win, py[i], px[i]*2, (i==id ? '@' : '0'+i));
        }
    }

    wrefresh(win);

    while (1) {

        int ch = wgetch(win);

        // SPECTATE MODE: miris spēlētājs paliek skatīties, bet nevar kustēties
        if(px[id] == 255 && py[id] == 255){
            // tikai skatās
        } else {
            if(ch == 'q') break;

            if(ch == 'w'){ uint8_t v='U'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
            if(ch == 's'){ uint8_t v='D'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
            if(ch == 'a'){ uint8_t v='L'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
            if(ch == 'd'){ uint8_t v='R'; send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
            if(ch == ' '){ uint8_t v=0;  send_msg(sock, MSG_BOMB_ATTEMPT, id, SERVER_ID, &v, 1); }
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);

        struct timeval tv = {0, 200000};

        int sel = select(sock+1, &fds, NULL, NULL, &tv);

        if(sel > 0 && FD_ISSET(sock, &fds)){
            while(1){
                msg_header_t h;
                uint8_t sbuff[65536];

                int r = recv_msg(sock, &h, sbuff, sizeof(sbuff));
                if(r <= 0) break;

                if(h.msg_type == MSG_MAP){
                    int pos = 0;
                    config.row          = sbuff[pos++];
                    config.col          = sbuff[pos++];
                    config.pl_speed     = sbuff[pos++];
                    config.exp_distance = sbuff[pos++];
                    config.exp_danger   = sbuff[pos++];
                    config.fuse_time    = sbuff[pos++];

                    for(int y=0; y<config.row; y++)
                        for(int x=0; x<config.col; x++)
                            config.tiles[y][x] = (TileType)sbuff[pos++];

                    for(int i=0; i<MAX_PLAYERS; i++){
                        px[i] = sbuff[pos++];
                        py[i] = sbuff[pos++];
                    }

                    werase(win);
                    map_render(win, &config);

                    for(int i=0; i<MAX_PLAYERS; i++){
                        if(px[i] != 255 && py[i] != 255){
                            mvwaddch(win, py[i], px[i]*2, (i==id ? '@' : '0'+i));
                        }
                    }

                    wrefresh(win);
                }

                if(h.msg_type == MSG_DEATH){
                    uint8_t pid = sbuff[0];
                    if(pid < MAX_PLAYERS){
                        px[pid] = 255;
                        py[pid] = 255;
                    }
                }

                if (h.msg_type == MSG_WINNER) {

                    clear();

                    if (sbuff[0] == id) {
                        mvprintw(LINES/2, COLS/2 - 8, "YOU WIN!");
                    } else {
                        mvprintw(LINES/2, COLS/2 - 8, "WASTED");
                    }

                    refresh();
                    sleep(5);

                    // atpakaļ uz lobby nākamajai spēlei
                    delwin(win);
                    goto lobby_start;
                }

                int more = 0;
                ioctl(sock, FIONREAD, &more);
                if(more <= 0) break;
            }
        }
    }

/*cleanup:
    send_msg(sock, MSG_LEAVE, id, SERVER_ID, NULL, 0);
    close(sock);
    endwin();
    return 0;
}*/
}