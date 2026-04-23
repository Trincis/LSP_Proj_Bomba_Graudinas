#ifndef GAME_H
#define GAME_H

#include <ncurses.h>

#define MAX_MAP_SIZE 255
#define MAX_PLAYERS 8

typedef enum{
    TILE_FLOOR = '.',
    TILE_WALL = 'H',
    TILE_BLOCK = 'S',
    TILE_BOMB = 'B',
    TILE_FASTER = 'A',
    TILE_BIGGER = 'R',
    TILE_LONGER = 'T',
    TILE_BOOM = '*'
}TileType;

typedef struct{
    int row;
    int col;
    int pl_speed;
    int exp_danger;
    int exp_distance;
    int fuse_time;

    TileType tiles[MAX_MAP_SIZE][MAX_MAP_SIZE];

    int player_spawn_x[MAX_PLAYERS+1];
    int player_spawn_y[MAX_PLAYERS+1];

}GameConfig;

typedef struct{
    int x;
    int y;
    int id;
    int dzivs;
} Player;

int game_config_load(GameConfig *config, const char *filename);
void map_render(WINDOW *win, const GameConfig *cfg);
void players_render(WINDOW *w, const Player *speletaji, int sk);

#endif