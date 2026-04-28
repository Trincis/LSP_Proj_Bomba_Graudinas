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
#define MSG_START_GAME   201
#define MSG_SET_READY    202

int main(int argc, char *argv[]){
    fprintf(stderr, "CLIENT STARTED\n");
    char serverIP[64];
    // Ja IP adrese norādīta kā argumenta, izmanto to, citādi izmanto localhost
    if(argc > 1) strcpy(serverIP, argv[1]);
    else strcpy(serverIP, "127.0.0.1");

    int port = 5000;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    // Pārbauda, vai socket izveidošana izdevās
    if(sock < 0){
        DBG("Socket creation failed\n");
        return 1;
    }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    inet_pton(AF_INET, serverIP, &srv.sin_addr);
    // Mēģina izveidot savienojumu ar serveri, un pārbauda, vai tas izdevās
    if(connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0){
        DBG("Connection failed\n");
        return 1;
    }

    DBG("Connected to server\n");

    send_msg(sock, MSG_HELLO, 0, SERVER_ID, NULL, 0);

    GameConfig config;
    memset(&config, 0, sizeof(config));

    int id = -1;
    int px[MAX_PLAYERS], py[MAX_PLAYERS];
    int ready_flags[MAX_PLAYERS];
    memset(ready_flags, 0, sizeof(ready_flags));

    int got_welcome = 0;
    int got_map = 0;
    int game_status = GAME_LOBBY;
    int is_host = 0;
    int selected_map = 0;

    //Sagaida WELCOME ziņojumu no servera, kurā ir spēlētāja ID un citi dati
    while (!got_welcome) {
        msg_header_t h;
        uint8_t buff[65536];

        int r = recv_msg(sock, &h, buff, sizeof(buff));
        // Ja savienojums ir pārtrūcis, iziet no programmas
        if (r <= 0) {
            DBG("Connection lost before WELCOME\n");
            close(sock);
            return 1;
        }

        DBG("Received msg_type=%d\n", h.msg_type);
        // Apstrādā WELCOME ziņojumu, iegūstot spēlētāja ID un hosta statusu
        if (h.msg_type == MSG_WELCOME) {
            id = h.target_id;
            is_host = (id == 0);
            DBG("WELCOME received, id=%d host=%d\n", id, is_host);
            got_welcome = 1;
        }
        // Apstrādā SET_STATUS ziņojumu, atjauninot spēles statusu
        if (h.msg_type == MSG_SET_STATUS) {
            game_status = buff[0];
            DBG("SET_STATUS=%d\n", game_status);
        }
        // Apstrādā MAP ziņojumu, iegūstot kartes datus un spēlētāju pozīcijas
        if (h.msg_type == MSG_MAP_SELECT) {
            selected_map = buff[0];
            DBG("MAP_SELECT=%d\n", selected_map);
        }

        //Serveris sūta karti arī lobby stadijā, lai hostam būtu redzams, kāda karte ir izvēlēta
        if (h.msg_type == MSG_MAP) {
            DBG("GOT MAP (early)\n");
            int pos = 0;
            config.row = buff[pos++];
            config.col = buff[pos++];
            // Pārkopē kartes datus no saņemtā bufera uz config struktūru
            for (int y = 0; y < config.row; y++)
                for (int x = 0; x < config.col; x++)
                    config.tiles[y][x] = buff[pos++];
            // Pārkopē spēlētāju pozīcijas no saņemtā bufera uz px un py masīviem
            for (int i = 0; i < MAX_PLAYERS; i++) {
                px[i] = buff[pos++];
                py[i] = buff[pos++];
            }

            got_map = 1;
        }
    }

    //Ncurses inicializācija
    DBG("Initializing ncurses\n");

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    clear();
    refresh();

    DBG("Entering LOBBY loop\n");

    //Priekšnama cikls, kurā spēlētāji var izvēlēties karti un sagatavoties spēlei
    while (game_status == GAME_LOBBY) {

        // Lasām servera ziņas
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv = {0, 100000};
        // Ja ir jauna ziņa no servera, apstrādājam to
        if (select(sock + 1, &fds, NULL, NULL, &tv) > 0) {
            while (1) {
                msg_header_t h;
                uint8_t sbuff[65536];

                int r = recv_msg(sock, &h, sbuff, sizeof(sbuff));
                if (r <= 0) break;

                DBG("Lobby received msg_type=%d\n", h.msg_type);
                // Apstrādā SET_STATUS ziņojumu, atjauninot spēles statusu
                if (h.msg_type == MSG_SET_STATUS) {
                    game_status = sbuff[0];
                    DBG("SET_STATUS=%d\n", game_status);
                }
                // Apstrādā READY ziņojumu, atjauninot attiecīgā spēlētāja gatavības statusu
                if (h.msg_type == MSG_SET_READY) {
                    int pid = sbuff[0];
                    int val = sbuff[1];
                    ready_flags[pid] = val;
                    DBG("READY pid=%d val=%d\n", pid, val);
                }
                // Apstrādā MAP_SELECT ziņojumu, atjauninot izvēlēto karti
                if (h.msg_type == MSG_MAP_SELECT) {
                    selected_map = sbuff[0];
                    DBG("MAP_SELECT=%d\n", selected_map);
                }
                // Apstrādā MAP ziņojumu, iegūstot kartes datus un spēlētāju pozīcijas
                if (h.msg_type == MSG_MAP) {
                    DBG("GOT MAP in lobby\n");
                    int pos = 0;
                    config.row = sbuff[pos++];
                    config.col = sbuff[pos++];
                    // Pārkopē kartes datus no saņemtā bufera uz config struktūru
                    for (int y = 0; y < config.row; y++)
                        for (int x = 0; x < config.col; x++)
                            config.tiles[y][x] = sbuff[pos++];
                    // Pārkopē spēlētāju pozīcijas no saņemtā bufera uz px un py masīviem
                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        px[i] = sbuff[pos++];
                        py[i] = sbuff[pos++];
                    }

                    got_map = 1;
                }

                int more = 0;
                ioctl(sock, FIONREAD, &more);// Ja nav vairāk datu, iziet no iekšējā cikla un atgriezties uz select gaidīt nākamo ziņu
                if (more <= 0) break;// Ja nav vairāk datu, iziet no iekšējā cikla un atgriezties uz select gaidīt nākamo ziņu
            }
        }

        // Zīmē lobby ekrānu
        clear();
        mvprintw(0, 0, "Bomberman LOBBY (DEBUG)");
        mvprintw(1, 0, "ID=%d HOST=%d", id, is_host);// Parāda spēlētāja ID un vai viņš ir hosts
        mvprintw(2, 0, "Selected map: %d", selected_map);// Parāda izvēlēto karti

        int line = 4;
        mvprintw(line++, 0, "Players:");
        for (int i = 0; i < MAX_PLAYERS; i++)// Parāda katra spēlētāja ID un gatavības statusu
            mvprintw(line++, 0, "  %d : %s", i, ready_flags[i] ? "READY" : "NOT READY");
//mvprintw ir ncurses funkcija, kas izdrukā tekstu norādītajā pozīcijā ekrānā. Šeit tā tiek izmantota, lai parādītu spēlētāju sarakstu un viņu gatavības statusu lobby ekrānā.
        mvprintw(line+1, 0, "Keys:");// Parāda pieejamās taustiņu komandas
        mvprintw(line+2, 0, "  r - toggle READY");
        if (is_host) {
            mvprintw(line+3, 0, "  1/2/3 - select map");
            mvprintw(line+4, 0, "  s - start game");
        }
        mvprintw(line+5, 0, "  q - quit");

        refresh();

        int ch = getch();// getch ir ncurses funkcija, kas gaida un nolasīt lietotāja ievadi no tastatūras. Šeit tā tiek izmantota, lai ļautu spēlētājam ieslēgt/izslēgt gatavības statusu, izvēlēties karti (ja ir hosts), sākt spēli (ja ir hosts) vai iziet no programmas.

        if (ch == 'q') {
            DBG("Quit from lobby\n");
            endwin();
            close(sock);
            return 0;
        }

        if (ch == 'r') {
            ready_flags[id] = !ready_flags[id];
            uint8_t p[1] = { (uint8_t)ready_flags[id] };
            DBG("Sending READY=%d\n", ready_flags[id]);
            send_msg(sock, MSG_SET_READY, id, SERVER_ID, p, 1);
        }

        if (is_host && (ch=='1'||ch=='2'||ch=='3')) {
            int idx = ch - '1';
            uint8_t p[1] = { (uint8_t)idx };
            DBG("Sending MAP_SELECT=%d\n", idx);
            send_msg(sock, MSG_MAP_SELECT, id, SERVER_ID, p, 1);
        }

        if (is_host && ch=='s') {
            DBG("Sending START_GAME\n");
            send_msg(sock, MSG_START_GAME, id, SERVER_ID, NULL, 0);
        }
    }

    DBG("Leaving lobby, waiting for MAP\n");

    //Sagaidam map ziņojumu no servera, kurā ir kartes dati un spēlētāju starta pozīcijas
    while (!got_map) {
        msg_header_t h;
        uint8_t sbuff[65536];

        int r = recv_msg(sock, &h, sbuff, sizeof(sbuff));
        DBG("Waiting-for-MAP msg_type=%d\n", h.msg_type);
        // Ja savienojums ir pārtrūcis, iziet no programmas
        if (h.msg_type == MSG_MAP) {
            int pos = 0;
            config.row = sbuff[pos++];
            config.col = sbuff[pos++];
            // Pārkopē kartes datus no saņemtā bufera uz config struktūru
            for (int y = 0; y < config.row; y++)
                for (int x = 0; x < config.col; x++)
                    config.tiles[y][x] = sbuff[pos++];
            // Pārkopē spēlētāju pozīcijas no saņemtā bufera uz px un py masīviem
            for (int i = 0; i < MAX_PLAYERS; i++) {
                px[i] = sbuff[pos++];
                py[i] = sbuff[pos++];
            }

            DBG("GOT MAP (start)\n");
            got_map = 1;
        }
    }

    DBG("Starting GAME LOOP\n");

    //Spēles cikls, kurā notiek pati spēle, un tiek apstrādātas servera ziņas un lietotāja ievade
    WINDOW *win = newwin(config.row + 2, config.col * 2 + 1, 1, 0);
    keypad(win, TRUE);
    timeout(50);

    while (1) {

        // Lasām servera ziņas spēles laikā
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv = {0, 0};
        // Ja ir jauna ziņa no servera, apstrādājam to
        if (select(sock + 1, &fds, NULL, NULL, &tv) > 0) {
            while (1) {
                msg_header_t h;
                uint8_t sbuff[65536];

                int r = recv_msg(sock, &h, sbuff, sizeof(sbuff));
                if (r <= 0) break;

                DBG("GAME received msg_type=%d\n", h.msg_type);
                // Apstrādā SET_STATUS ziņojumu, atjauninot spēles statusu
                if (h.msg_type == MSG_MAP) {
                    int pos = 0;
                    config.row = sbuff[pos++];
                    config.col = sbuff[pos++];
                    // Pārkopē kartes datus no saņemtā bufera uz config struktūru
                    for (int y = 0; y < config.row; y++)
                        for (int x = 0; x < config.col; x++)
                            config.tiles[y][x] = sbuff[pos++];
                    // Pārkopē spēlētāju pozīcijas no saņemtā bufera uz px un py masīviem
                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        px[i] = sbuff[pos++];
                        py[i] = sbuff[pos++];
                    }

                    DBG("MAP update received in GAME LOOP\n");
                }

                int more = 0;
                ioctl(sock, FIONREAD, &more);
                if (more <= 0) break;
            }
        }

        int ch = wgetch(win);

        if (ch == 'q') {
            DBG("Quit pressed, exiting game loop\n");
            break;
        }

        if (ch == 'w') { uint8_t v='U'; DBG("Sending MOVE U\n"); send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
        if (ch == 's') { uint8_t v='D'; DBG("Sending MOVE D\n"); send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
        if (ch == 'a') { uint8_t v='L'; DBG("Sending MOVE L\n"); send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
        if (ch == 'd') { uint8_t v='R'; DBG("Sending MOVE R\n"); send_msg(sock, MSG_MOVE_ATTEMPT, id, SERVER_ID, &v, 1); }
        if (ch == ' ') { uint8_t v=0;  DBG("Sending BOMB\n");     send_msg(sock, MSG_BOMB_ATTEMPT, id, SERVER_ID, &v, 1); }

        werase(win);

        // Zīmē karti
        for (int y = 0; y < config.row; y++) {
            for (int x = 0; x < config.col; x++) {
                char c = '.';
                switch (config.tiles[y][x]) {
                    case TILE_WALL:   c = 'H'; break;
                    case TILE_BLOCK:  c = 'S'; break;
                    case TILE_BOMB:   c = 'B'; break;
                    case TILE_FASTER: c = 'A'; break;
                    case TILE_BIGGER: c = 'R'; break;
                    case TILE_LONGER: c = 'T'; break;
                    case TILE_BOOM:   c = '*'; break;
                    default:          c = '.'; break;
                }
                mvwaddch(win, y, x * 2, c);
            }
        }

        // Zīmē spēlētājus
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (px[i] != 255 && py[i] != 255) {
                mvwaddch(win, py[i], px[i] * 2, (i == id ? '@' : '0' + i));
            }
        }

        wrefresh(win);// Atjaunina ekrānu ar jaunajiem zīmējumiem
    }

    DBG("Exiting ncurses\n");
    endwin();
    close(sock);
    return 0;
}