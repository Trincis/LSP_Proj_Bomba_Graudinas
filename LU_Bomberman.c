/**
LU_Bomberman.c

Programma ir spēle balstīta uz retro spēli Bomberman

Spēles laukuma konfigurācija - fails "config"

Autori: Barbara Terēze Graudiņa, bg24008
        Katrīna Anna Graudiņa, kg21076

Programma izveidota: ??.04.2026
**/
#include "src/game.h"
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
/**
Kartes simboli:
H - ciets
S - miksts
1...8 speletajs (moz krasainus?)
B - bumba
R - radiusa palielinasana
T - bumbas atskaites laika palielinasana
**/

int main(){
    GameConfig config;
    if(game_config_load(&config, "map.cfg")!=0){
        fprintf(stderr, "Failed to load map.cfg\n");
        return 1;
    }


    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    clear();
    refresh();

    ///ekrāns
    WINDOW *win = newwin(config.row+1, config.col*2+1, 0, 0);

    if(win == NULL){
        endwin();
        fprintf(stderr, "Failed to create window\n");
        return 1;
    }

    ///kartes renderēšana
    map_render(win, &config);

    ///sagaidīt pogas spiedienu
    getch();

    ///spēles kods

    delwin(win);
    endwin();

    return 0;
}
