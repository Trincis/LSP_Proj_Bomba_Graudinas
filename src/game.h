#ifndef GAME_H
#define GAME_H

#ifndef NO_RENDER
#include <ncurses.h>
#endif

#include <stdint.h>

#define MAX_MAP_SIZE 255
#define MAX_PLAYERS 8
#define MAX_BOMBS 16
#define MAX_BOOM 64

typedef enum{
    TILE_FLOOR   = '.',
    TILE_WALL    = 'H',
    TILE_BLOCK   = 'S',
    TILE_BOMB    = 'B',
    TILE_FASTER  = 'A',
    TILE_BIGGER  = 'R',
    TILE_LONGER  = 'T',
    TILE_MOREBOMBS = 'N',
    TILE_BOOM    = '*'
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
    int atrums;
    int radiuss;
} Player;

typedef struct{
    int x;
    int y;
    int aktivs;
    int timer;
    int owner;
    int radiuss;
} Bomb;

typedef struct{
    int x;
    int y;
    int aktivs;
    int timer;
}BOOM;

int game_config_load(GameConfig *config, const char *filename);
void Spragsti(GameConfig *cfg, Bomb *bumba, BOOM *spradzieni);

#ifndef NO_RENDER
void map_render(WINDOW *win, const GameConfig *cfg);
void players_render(WINDOW *w, const Player *speletaji, int sk);
void bombs_render(WINDOW *w, const Bomb *bumbas, int sk);
#endif

static inline uint16_t make_cell_index(uint16_t row, uint16_t col, uint16_t cols) {
    return (uint16_t)(row * cols + col);
}

static inline void split_cell_index(uint16_t index, uint16_t cols, uint16_t *row, uint16_t *col) {
    *row = (uint16_t)(index / cols);
    *col = (uint16_t)(index % cols);
}

#endif
