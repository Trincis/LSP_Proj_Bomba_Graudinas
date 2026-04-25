#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>

#include "network.h"
#include "protocol.h"
#include "game.h"

int main() {
    fprintf(stderr, "[CLIENT] Starting client...\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[CLIENT] socket");
        return 1;
    }

    fprintf(stderr, "[CLIENT] Socket created: %d\n", sock);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    fprintf(stderr, "[CLIENT] Connecting to server...\n");
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[CLIENT] connect");
        return 1;
    }

    fprintf(stderr, "[CLIENT] Connected! Sending HELLO...\n");

    if (send_msg(sock, MSG_HELLO, 0, SERVER_ID, NULL, 0) < 0) {
        perror("[CLIENT] send_msg HELLO");
        return 1;
    }

    fprintf(stderr, "[CLIENT] HELLO sent.\n");

    // NCURSES INIT
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    timeout(0);   // getch() nebloķē – mēs paši kontrolējam tempu ar select()

    GameConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    int my_id = -1;
    int px[MAX_PLAYERS], py[MAX_PLAYERS];
    for (int i = 0; i < MAX_PLAYERS; i++) px[i] = py[i] = -1;

    WINDOW *w = NULL;

    while (1) {
        // --- SOCKET READ AR SELECT ---
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // ~50 ms frame

        int sel = select(sock + 1, &rfds, NULL, NULL, &tv);
        if (sel < 0) {
            perror("[CLIENT] select");
            break;
        }

        if (sel > 0 && FD_ISSET(sock, &rfds)) {
            msg_header_t h;
            uint8_t buf[65535];

            int r = recv_msg(sock, &h, buf, sizeof(buf));
            if (r <= 0) {
                fprintf(stderr, "[CLIENT] Server closed or recv error (r=%d, errno=%d)\n", r, errno);
                break;
            }

            fprintf(stderr, "[CLIENT] Received msg: type=%d sender=%d target=%d len=%d\n",
                    h.msg_type, h.sender_id, h.target_id, h.payload_len);

            if (h.msg_type == MSG_WELCOME) {
                fprintf(stderr, "[CLIENT] Got WELCOME. My ID = %d\n", h.target_id);
                my_id = h.target_id;
            }

            if (h.msg_type == MSG_MAP) {
                fprintf(stderr, "[CLIENT] Got MAP update.\n");

                int pos = 0;
                cfg.row = buf[pos++];
                cfg.col = buf[pos++];

                for (int y = 0; y < cfg.row; y++)
                    for (int x = 0; x < cfg.col; x++)
                        cfg.tiles[y][x] = buf[pos++];

                for (int i = 0; i < MAX_PLAYERS; i++) {
                    px[i] = buf[pos++];
                    py[i] = buf[pos++];
                }

                if (!w)
                    w = newwin(cfg.row + 2, cfg.col * 2 + 2, 0, 0);

                werase(w);

                for (int y = 0; y < cfg.row; y++)
                    for (int x = 0; x < cfg.col; x++)
                        mvwaddch(w, y, x * 2, cfg.tiles[y][x]);

                for (int i = 0; i < MAX_PLAYERS; i++)
                    if (px[i] != 255 && py[i] != 255)
                        mvwaddch(w, py[i], px[i] * 2, (i == my_id ? '@' : 'P'));

                wrefresh(w);
            }
        }

        // --- INPUT (VIENMĒR KATRĀ CIKLĀ) ---
        int ch = getch();
        if (ch == 'q') break;

        uint8_t dir = 0;
        if (ch == 'w') dir = 'U';
        if (ch == 's') dir = 'D';
        if (ch == 'a') dir = 'L';
        if (ch == 'd') dir = 'R';

        if (ch == ' ') {
            fprintf(stderr, "[CLIENT] Sending BOMB_ATTEMPT\n");
            uint8_t dummy = 0;
            send_msg(sock, MSG_BOMB_ATTEMPT, my_id, SERVER_ID, &dummy, 1);
        }


        if (dir != 0 && my_id != -1) {
            fprintf(stderr, "[CLIENT] Sending MOVE_ATTEMPT %c\n", dir);
            if (send_msg(sock, MSG_MOVE_ATTEMPT, my_id, SERVER_ID, &dir, 1) < 0) {
                perror("[CLIENT] send_msg MOVE_ATTEMPT");
                break;
            }
        }
    }

    endwin();
    close(sock);
    return 0;
}
