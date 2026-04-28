#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MAX_PLAYERS 8
#define SERVER_ID 255
#define BROADCAST_ID 254

// Spēles statusi
typedef enum {
    GAME_LOBBY = 0,
    GAME_RUNNING = 1,
    GAME_END = 2 
} game_status_t;

// Ziņu tipi
typedef enum {

    //Savienojuma ziņas
    MSG_HELLO = 0,
    MSG_WELCOME = 1,
    MSG_DISCONNECT = 2,
    MSG_PING = 3,
    MSG_PONG = 4,
    MSG_LEAVE = 5, 
    MSG_ERROR = 6,

    //Spēles stāvoklis
    MSG_MAP = 7,
    MSG_SET_STATUS = 20,  
    MSG_WINNER = 23,

    //Spēlētāju darbības
    MSG_MOVE_ATTEMPT = 30, 
    MSG_BOMB_ATTEMPT = 31, 

    //Servera notikumi
    MSG_MOVED = 40,
    MSG_BOMB = 41, 
    MSG_EXPLOSION_START = 42,
    MSG_EXPLOSION_END = 43,
    MSG_DEATH = 44,
    MSG_BONUS_AVAILABLE = 45,
    MSG_BONUS_RETRIEVED = 46,
    MSG_BLOCK_DESTROYED = 47,

    // Sinhronizācija 
    MSG_SYNC_BOARD = 100, 
    MSG_SYNC_REQUEST = 101  
} msg_type_t;
typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;    // Kam adresēts (255=serveris, 254=broadcast)
    uint16_t payload_len;
} msg_header_t;

#endif
