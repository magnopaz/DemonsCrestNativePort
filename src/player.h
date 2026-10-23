#ifndef DC_PLAYER_H
#define DC_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "anim.h"
#include "level.h"
#include "input.h"

typedef enum PlayerState {
    PLAYER_IDLE,
    PLAYER_WALK,
    PLAYER_JUMP,
    PLAYER_HOVER,
    PLAYER_FLY,
    PLAYER_LAND,
    PLAYER_HURT,
    PLAYER_DEAD
} PlayerState;

typedef struct Player {
    /* Public pixel-space position/velocity used by the camera, renderer and
     * trace output. The authoritative movement state is 8.8 fixed point. */
    float x;          /* feet centre, world pixels */
    float y;
    float vx;
    float vy;

    int32_t x_fx;     /* world X, 8.8 fixed ($1030) */
    int32_t y_fx;     /* world Y, 8.8 fixed ($1033) */
    int32_t vy_fx;    /* vertical velocity, native sign (down = +) */
    int32_t walk_speed_fx; /* $1020, 8.8 fixed */

    bool on_ground;
    bool on_slope;    /* $1070 terrain flag; wired in Step 5 */
    bool fly_enabled; /* debug/crest flight, off for base Gargoyle */
    int facing;       /* -1 left, +1 right */
    int prev_dir;     /* $0072, previous direction input */
    int jump_sub;     /* $1006: 0 takeoff, 2 launch, 4 flight */
    PlayerState state;
    float anim_time;

    int special_timer; /* frames left in the A/Talisman animation */
    int shot_timer;    /* frames left in the Y/Fireball animation */
    int land_timer;    /* frames left in the landing animation */
    bool shot_fired;   /* one-frame event consumed by the projectile pool */

    int hp;            /* $1062 */
    int max_hp;        /* $1E50 */
    int invuln_timer;  /* contact-damage immunity */
    int knockback_timer;
    int knockback_dir;

    int anim_clip;    /* current AnimClipId, -1 before the first frame */
    int anim_step;
    int anim_timer;
} Player;

void Player_Init(Player *player, const Level *level);
void Player_Update(Player *player, const Level *level, const Input *input);
void Player_Animate(Player *player, const PlayerAnim *anim);

#endif
