#ifdef SERVER_BUILD
#define NO_RENDER
#endif

#include "game.h"
#include <stdio.h>
#include <string.h>

int game_config_load(GameConfig *config, const char *filename){
    FILE *fin = fopen(filename, "r");
    if(!fin){
        return -1;
    }

    // Header
    if(fscanf(fin, "%d %d %d %d %d %d",
              &config->row,
              &config->col,
              &config->pl_speed,
              &config->exp_danger,
              &config->exp_distance,
              &config->fuse_time) != 6)
    {
        fclose(fin);
        return -1;
    }

    for(int i = 0; i <= MAX_PLAYERS; i++){
        config->player_spawn_x[i] = -1;
        config->player_spawn_y[i] = -1;
    }

    for(int y = 0; y < config->row; y++){
        for(int x = 0; x < config->col; x++){

            char c;
            if (fscanf(fin, " %c", &c) != 1) {
            fclose(fin);
            return -1;
            }
            
            if (c >= '0' && c <= '7') {
                int id = c - '0';
                config->player_spawn_x[id] = x;
                config->player_spawn_y[id] = y;
                config->tiles[y][x] = TILE_FLOOR;
                continue;
            }

            switch(c){
                case 'H': config->tiles[y][x] = TILE_WALL; break;
                case 'S': config->tiles[y][x] = TILE_BLOCK; break;
                case 'B': config->tiles[y][x] = TILE_BOMB; break;
                case 'A': config->tiles[y][x] = TILE_FASTER; break;
                case 'R': config->tiles[y][x] = TILE_BIGGER; break;
                case 'T': config->tiles[y][x] = TILE_LONGER; break;
                case '*': config->tiles[y][x] = TILE_BOOM; break;
                default:  config->tiles[y][x] = TILE_FLOOR; break;
            }
        }
    }

    fclose(fin);
    return 0;
}

#ifndef NO_RENDER
void map_render(WINDOW *w, const GameConfig *cfg){
    for(int y = 0; y<cfg->row; y++){
        for(int x = 0; x<cfg->col; x++){
            char ch;

            switch(cfg->tiles[y][x]){
                case TILE_WALL:{
                    ch='H';
                    break;
                }
                case TILE_BLOCK:{
                    ch='S';
                    break;
                }
                case TILE_BOMB:{
                    ch='B';
                    break;
                }
                case TILE_FASTER:{
                    ch='A';
                    break;
                }
                case TILE_BIGGER:{
                    ch='R';
                    break;
                }
                case TILE_LONGER:{
                    ch='T';
                    break;
                }
                case TILE_BOOM:{
                    ch='*';
                    break;
                }
                default:{
                    ch='.';
                    break;
                }
            }
            mvwaddch(w, y, x*2, ch);
        }
    }
    wrefresh(w);
}

void players_render(WINDOW *w, const Player *speletaji, int sk){
    for(int i=0; i<sk; i++){
        if(speletaji[i].dzivs){
            mvwaddch(w, speletaji[i].y, speletaji[i].x*2, '0'+speletaji[i].id);
        }
    }
    wrefresh(w);
}

void bombs_render(WINDOW *w, const Bomb *bumbas, int sk){
    for(int i=0; i<sk; i++){
        if(bumbas[i].aktivs){
            mvwaddch(w, bumbas[i].y,bumbas[i].x*2, 'B');
        }
    }
    wrefresh(w);
}
#endif

void Spragsti(GameConfig *cfg, Bomb *bombs, BOOM *spradzieni)
{
    // 1) Samazina bumbu taimerus
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].aktivs)
            continue;

        bombs[i].timer--;

        // Ja vēl nav jāsprāgst — turpinām
        if (bombs[i].timer > 0)
            continue;

        // 2) Bumba sprāgst
        int bx = bombs[i].x;
        int by = bombs[i].y;

        // Notīra bumbu no kartes
        cfg->tiles[by][bx] = TILE_BOOM;

        // 3) Izveido centrālo sprādzienu
        for (int s = 0; s < MAX_BOOM; s++) {
            if (!spradzieni[s].aktivs) {
                spradzieni[s].aktivs = 1;
                spradzieni[s].x = bx;
                spradzieni[s].y = by;
                spradzieni[s].timer = 5;
                break;
            }
        }

        // 4) Sprādziena izplatīšanās
        int virz[4][2] = {
            {0,-1}, {0,1}, {-1,0}, {1,0}
        };

        for (int d = 0; d < 4; d++) {
            for (int r = 1; r <= cfg->exp_distance; r++) {

                int nx = bx + virz[d][0] * r;
                int ny = by + virz[d][1] * r;

                if (nx < 0 || nx >= cfg->col || ny < 0 || ny >= cfg->row)
                    break;

                if (cfg->tiles[ny][nx] == TILE_WALL)
                    break;

                // Ja ir bloks — iznīcina un apstājas
                if (cfg->tiles[ny][nx] == TILE_BLOCK) {
                    cfg->tiles[ny][nx] = TILE_FLOOR;

                    for (int s = 0; s < MAX_BOOM; s++) {
                        if (!spradzieni[s].aktivs) {
                            spradzieni[s].aktivs = 1;
                            spradzieni[s].x = nx;
                            spradzieni[s].y = ny;
                            spradzieni[s].timer = 5;
                            break;
                        }
                    }
                    break;
                }

                // Parasts sprādziena laukums
                for (int s = 0; s < MAX_BOOM; s++) {
                    if (!spradzieni[s].aktivs) {
                        spradzieni[s].aktivs = 1;
                        spradzieni[s].x = nx;
                        spradzieni[s].y = ny;
                        spradzieni[s].timer = 5;
                        break;
                    }
                }

                cfg->tiles[ny][nx] = TILE_BOOM;
            }
        }

        // 5) Bumba vairs nav aktīva
        bombs[i].aktivs = 0;
    }

    // 6) Sprādzienu animācija (pazūd pēc 5 tickiem)
    for (int i = 0; i < MAX_BOOM; i++) {
        if (!spradzieni[i].aktivs)
            continue;

        spradzieni[i].timer--;

        if (spradzieni[i].timer <= 0) {
            cfg->tiles[spradzieni[i].y][spradzieni[i].x] = TILE_FLOOR;
            spradzieni[i].aktivs = 0;
        }
    }
}