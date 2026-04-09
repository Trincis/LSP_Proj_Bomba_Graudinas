/**
LU_Bomberman.c

Programma ir spēle balstīta uz retro spēli Bomberman

Spēles laukuma konfigurācija - fails "config"

Autori: Barbara Terēze Graudiņa, bg24008
        Katrīna Anna Graudiņa, kg21076

Programma izveidota: ??.04.2026
**/
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

//funkcijai jaizveido laukums un jasagatavo speles sakums
void start(FILE conf){
///apstrada konfiguracijas faila doto informaciju
///<augst> <plat> <atrums> <boom ilgums> <boom attalums> <boom atskaite>
///laukuma karte

}

int main(){
    FILE conf = fopen("config", "r");

    initscr();

    start(conf);

    return 0;
}
