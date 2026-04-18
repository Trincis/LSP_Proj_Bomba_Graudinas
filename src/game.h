#ifndef GAME_H
#define GAME_H

#include <ncurses.h>

#define MAX_MAP_SIZE 255
#define mAX_PLAYERS 8

typedef enum laucini{
    TILE_FLOOR = '.',
    TILE_WALL = 'H',
    TILE_BLOCK = 'S',
    TILE_BOMB = 'B',
    TILE_FASTER = 'A',
    TILE_BIGGER = 'R',
    TILE_LONGER = 'T',
    TILE_BOOM = '*'
}

typedef struct GameConfig{
    int row;
    int col;
    int pl_speed;
    int exp_danger;
    int exp_distance;
    int fuse_time;

    TileType tiles[MAX_MAP_SIZE][MAX_MAP_SIZE];

    int player_spawn_x[mAX_PLAYERS+1];
    int player_spawn_y[mAX_PLAYERS+1];

}

int config_load(GameConfig *congif, const char *filename);
void map_render(WINDOW *win, const GameConfig *cfg);

#endif