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

    Player speletaji[MAX_PLAYERS];

    for(int i=0; i<=MAX_PLAYERS; i++){
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
    }

    BOOM spradzieni[MAX_BOOM];
    for(int i=0; i<MAX_BOOM; i++){
        spradzieni[i].aktivs=0;
        spradzieni[i].x=-1;
        spradzieni[i].y=-1;
        spradzieni[i].timer=0;
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

                ///"Sobrīd lokāļas darbības"
                int ny = speletaji[1].y-1;
                if(ny>=0&&config.tiles[ny][speletaji[1].x]==TILE_FLOOR){
                    speletaji[1].y = ny;
                }
                
                break;
            }
            case KEY_DOWN:
            case 's':{
                ///MSG_MOVE_ATTEMPT ziņa lejup serverim
                 ///"Sobrīd lokāļas darbības"
                int ny = speletaji[1].y+1;
                if(ny<config.row&&config.tiles[ny][speletaji[1].x]==TILE_FLOOR){
                    speletaji[1].y = ny;
                }
                
                break;
            }
            case KEY_LEFT:
            case 'a':{
                ///MSG_MOVE_ATTEMPT ziņa pa kreisi serverim
                 ///"Sobrīd lokāļas darbības"
                int nx = speletaji[1].x-1;
                if(nx>=0&&config.tiles[speletaji[1].y][nx]==TILE_FLOOR){
                    speletaji[1].x = nx;
                }
                
                break;
            }
            case KEY_RIGHT:
            case 'd':{
                ///MSG_MOVE_ATTEMPT ziņa pa labi serverim
                int nx = speletaji[1].x+1;
                if(nx<config.col&&config.tiles[speletaji[1].y][nx]==TILE_FLOOR){
                    speletaji[1].x = nx;
                }
                
                break;
            }
            case ' ':{
                ///MSG_BOMB_ATTEMPT ziņa bumbot serverim

                for(int i=0; i<MAX_BOMBS; i++){
                    if(!bumbas[i].aktivs){
                        bumbas[i].x = speletaji[1].x;
                        bumbas[i].y = speletaji[1].y;
                        bumbas[i].aktivs = 1;
                        bumbas[i].timer = config.fuse_time*10;
                        break;
                    }
                }


                break;
            }
        }

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

    map_render(win, &config);
    players_render(win, speletaji, MAX_PLAYERS+1);
    bombs_render(win, bumbas, MAX_BOMBS);

    }

    ///spēles kods

    delwin(win);
    endwin();

    return 0;
}
