#ifndef DC_PLAYER_H
#define DC_PLAYER_H

#include <stdbool.h>
#include "level.h"
#include "input.h"

typedef enum PlayerState {
    PLAYER_IDLE,
    PLAYER_WALK,
    PLAYER_JUMP,
    PLAYER_HOVER,
    PLAYER_FLY,
    PLAYER_HURT,
    PLAYER_DEAD
} PlayerState;

typedef struct Player {
    float x;          /* feet centre, world pixels */
    float y;
    float vx;
    float vy;
    bool on_ground;
    bool flying;
    bool jump_boosting;
    int facing;       /* -1 left, +1 right */
    PlayerState state;
    float anim_time;
    float hurt_timer;
} Player;

void Player_Init(Player *player, const Level *level);
void Player_Update(Player *player, const Level *level, const Input *input);

#endif
