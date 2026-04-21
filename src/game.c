#include "game.h"
#include <stdio.h>
#include <string.h>

int game_config_load(GameConfig *config, const char *filename){
    FILE *fin = fopen(filename, "r");
    if(!fin){
        return -1;
    }

    fscanf(fin, "%d %d %d %d %d %d", &config->row, &config->col, &config->pl_speed, &config->exp_danger, &config->exp_distance, &config->fuse_time);

    
}

/*int main(){
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    ///spēles kods

    endwin();

    return 0;
}*/