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
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) return 1;

    // Sūtam Hello
    send_msg(sock, MSG_HELLO, 0, SERVER_ID, NULL, 0);

    initscr(); noecho(); cbreak(); keypad(stdscr, TRUE); curs_set(0); timeout(30);

    GameConfig cfg;
    msg_header_t h;
    uint8_t buf[65535];
    int my_id = -1;
    WINDOW *w = NULL;
    int px[MAX_PLAYERS], py[MAX_PLAYERS];

    while (1) {
        // Apstrādājam visas ienākošās ziņas no servera
        while (recv_msg(sock, &h, buf, sizeof(buf)) > 0) {
            if (h.msg_type == MSG_WELCOME) {
                my_id = h.target_id;
            } 
            else if (h.msg_type == MSG_SET_STATUS) {
                cfg.row = buf[0]; cfg.col = buf[1];
                int pos = 2;
                // Ielādējam karti
                for (int y = 0; y < cfg.row; y++) {
                    for (int x = 0; x < cfg.col; x++) {
                        cfg.tiles[y][x] = (TileType)buf[pos++];
                    }
                }
                // Ielādējam spēlētāju pozīcijas
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    px[i] = (buf[pos] == 255) ? -1 : (int)buf[pos]; pos++;
                    py[i] = (buf[pos] == 255) ? -1 : (int)buf[pos]; pos++;
                }

                if (!w) w = newwin(cfg.row + 2, cfg.col * 2 + 2, 0, 0);
                werase(w);
                // Zīmējam karti
                for (int y = 0; y < cfg.row; y++) {
                    for (int x = 0; x < cfg.col; x++) {
                        mvwaddch(w, y, x * 2, (char)cfg.tiles[y][x]);
                    }
                }
                // Zīmējam spēlētājus
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (px[i] != -1 && py[i] != -1) {
                        mvwaddch(w, py[i], px[i] * 2, (i == my_id) ? '@' : 'P');
                    }
                }
                wrefresh(w);
            }
        }

        int ch = getch();
        if (ch == 'q') break;

        uint8_t dir = 0;
        if (ch == 'w' || ch == KEY_UP)    dir = 1;
        else if (ch == 's' || ch == KEY_DOWN)  dir = 2;
        else if (ch == 'a' || ch == KEY_LEFT)  dir = 3;
        else if (ch == 'd' || ch == KEY_RIGHT) dir = 4;
        
        if (dir > 0 && my_id != -1) {
            send_msg(sock, MSG_MOVE_ATTEMPT, (uint8_t)my_id, SERVER_ID, &dir, 1);
        }
    }

    endwin();
    close(sock);
    return 0;
}