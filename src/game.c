#include "game.h"
#include <stdio.h>
#include <string.h>

int game_config_load(GameConfig *config, const char *filename){
    FILE *fin = fopen(filename, "r");
    if(!fin){
        return -1;
    }

    if(fscanf(fin, "%d %d %d %d %d %d", &config->row, &config->col, &config->pl_speed, &config->exp_danger, &config->exp_distance, &config->fuse_time)!=6){
        fclose(fin);
        return -1;
    }

    for(int i=0; i<=MAX_PLAYERS; i++){
        config->player_spawn_x[i]=-1;
        config->player_spawn_y[i]=-1;
    }

    for(int y = 0; y<config->row; y++){
        for(int x = 0; x<config->col; x++){
            char token[4];
            if(fscanf(fin, "%3s", token)!=1){
                fclose(fin);
                return -1;
            }

            char c = token[0];

            if(c>='1'&&c<='8'){
                int id = c-'0';
                config->player_spawn_x[id] = x;
                config->player_spawn_y[id]=y;
                config->tiles[y][x]=TILE_FLOOR;
            }
            else{
                switch(c){
                    case 'H':{
                        config->tiles[y][x]=TILE_WALL;
                        break;
                    }
                    case 'S':{
                        config->tiles[y][x]=TILE_BLOCK;
                        break;
                    }
                    case 'B':{
                        config->tiles[y][x]=TILE_BOMB;
                        break;
                    }
                    case 'A':{
                        config->tiles[y][x]=TILE_FASTER;
                        break;
                    }
                    case 'R':{
                        config->tiles[y][x]=TILE_BIGGER;
                        break;
                    }
                    case 'T':{
                        config->tiles[y][x]=TILE_LONGER;
                        break;
                    }
                    default: {
                        config->tiles[y][x]=TILE_FLOOR;
                        break;
                    }
                }
            }
        }
    }

    fclose(fin);
    return 0;    
}

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