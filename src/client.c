#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <string.h>

#include "network.h"
#include "protocol.h"
#include "game.h"   

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to server\n");

    send_msg(sock, MSG_HELLO, 0, SERVER_ID, NULL, 0);
    printf("Sent HELLO\n");

    msg_header_t h;
    uint8_t buf[65535];

    // Saņemam WELCOME
    while (1) {
        recv_msg(sock, &h, buf, sizeof(buf));
        if (h.msg_type == MSG_WELCOME)
            break;
    }

    // Saņemam MAP
    while (1) {
        recv_msg(sock, &h, buf, sizeof(buf));
        if (h.msg_type == MSG_SET_STATUS)
            break;
    }

    int rows = buf[0];
    int cols = buf[1];

    printf("Map received: %d x %d\n", rows, cols);

    // Izveido GameConfig struktūru kartes zīmēšanai
    GameConfig cfg;
    cfg.row = rows;
    cfg.col = cols;

    int pos = 2;
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            char c = buf[pos++];

            switch(c){
                case 'H': cfg.tiles[y][x] = TILE_WALL;   break;
                case 'S': cfg.tiles[y][x] = TILE_BLOCK;  break;
                case 'B': cfg.tiles[y][x] = TILE_BOMB;   break;
                case 'A': cfg.tiles[y][x] = TILE_FASTER; break;
                case 'R': cfg.tiles[y][x] = TILE_BIGGER; break;
                case 'T': cfg.tiles[y][x] = TILE_LONGER; break;
                case '*': cfg.tiles[y][x] = TILE_BOOM;   break;
                default:  cfg.tiles[y][x] = TILE_FLOOR;  break;
            }
        }
    }

    // Ncurses
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    WINDOW *w = newwin(rows, cols*2, 1, 1);

    // Zīmē karti ar game.c funkciju
    map_render(w, &cfg);

    // Spēlētāja lokālā kustība
    int px = 1, py = 1;
    mvwaddch(w, py, px*2, '@');
    wrefresh(w);

    while (1) {
        int ch = getch();
        mvwaddch(w, py, px*2, '.');

        if (ch == 'w') py--;
        if (ch == 's') py++;
        if (ch == 'a') px--;
        if (ch == 'd') px++;

        mvwaddch(w, py, px*2, '@');
        wrefresh(w);
    }

    endwin();
    close(sock);
    return 0;
}