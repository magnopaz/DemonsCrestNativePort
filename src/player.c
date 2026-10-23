#include "player.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PLAYER_HALF_W 6.0f
#define PLAYER_HEIGHT 22.0f

/* Bank $80 player constants, all 8.8 fixed point. */
#define PLAYER_WALK_INITIAL_FX 0x00B3 /* $80:F0BE, 0.69921875 px/frame */
#define PLAYER_WALK_MAX_FX 0x0166     /* $80:F0D0, 1.3984375 px/frame */
#define PLAYER_WALK_ACCEL_FX 0x0003   /* $80:F07A, 3/256 px/frame per frame */
#define PLAYER_GRAVITY_FX 0x0038      /* $80:E070, 0.21875 px/frame^2 */
#define PLAYER_JUMP_FX 0x059C         /* $80:E170, 5.609375 px/frame */
#define PLAYER_JUMP_WATER_FX 0x02CE   /* $80:E166, 2.8046875 px/frame */
#define PLAYER_TERMINAL_FALL_FX 0x0600 /* $80:8D13, 6.0 px/frame */
#define PLAYER_FLY_UP_FX 0x0200       /* debug-only B+Up, 2.0 px/frame */
#define PLAYER_SHOT_FRAMES 17         /* Y spit: idle + 6 + 10 frames */
#define PLAYER_SPECIAL_FRAMES 31      /* A State 0x16: 11 + 4 + 16 frames */

#define FX_ONE 256
#define FX_TO_FLOAT(v) ((float)(v) / (float)FX_ONE)
#define FLOAT_TO_FX(v) ((int32_t)lroundf((v) * (float)FX_ONE))

/* Horizontal/ceiling collision sampled at the upper body only. The feet row
 * is intentionally excluded so a full block the player stands on does not
 * count as a wall; slopes are walkable by definition. */
static bool PlayerCollides(const Level *level, float x, float y) {
    float left = x - PLAYER_HALF_W;
    float right = x + PLAYER_HALF_W;
    float low = y - 3.0f;
    float mid = y - PLAYER_HEIGHT * 0.5f;
    float top = y - PLAYER_HEIGHT + 1.0f;
    return Level_IsWallAt(level, left, low) ||
           Level_IsWallAt(level, right, low) ||
           Level_IsWallAt(level, left, mid) ||
           Level_IsWallAt(level, right, mid) ||
           Level_IsWallAt(level, left, top) ||
           Level_IsWallAt(level, right, top);
}

/* The feet sample is the player's Y line itself. Level_FloorAt applies the
 * $80:962B slope height table for the block under the feet. */
static bool GroundTop(const Level *level, float x, float feet_y, float *top) {
    return Level_FloorAt(level, x, feet_y, top);
}

static void SyncPublic(Player *player) {
    player->x = FX_TO_FLOAT(player->x_fx);
    player->y = FX_TO_FLOAT(player->y_fx);
    player->vy = FX_TO_FLOAT(player->vy_fx);
}

/* $80:8D03 vertical integrator, converted to y-down:
 *   velocity += gravity; clamp at 6 px/frame; position += velocity.
 * Landing uses the $80:962B surface height for the cell under the feet. */
static void IntegrateVertical(Player *player, const Level *level) {
    player->vy_fx += PLAYER_GRAVITY_FX;
    if (player->vy_fx > PLAYER_TERMINAL_FALL_FX)
        player->vy_fx = PLAYER_TERMINAL_FALL_FX;
    if (player->vy_fx < -PLAYER_TERMINAL_FALL_FX)
        player->vy_fx = -PLAYER_TERMINAL_FALL_FX;

    int32_t old_y = player->y_fx;
    player->y_fx += player->vy_fx;
    SyncPublic(player);

    if (player->vy_fx > 0) {
        float surface;
        if (Level_FloorAt(level, player->x, player->y, &surface) &&
            surface <= player->y) {
            player->y_fx = FLOAT_TO_FX(surface);
            player->vy_fx = 0;
            player->on_ground = true;
            player->jump_sub = 0;
            player->landed_event = true;
            SyncPublic(player);
            return;
        }
    } else if (player->vy_fx < 0) {
        /* Ceiling: stop the ascent without moving into the block. */
        if (PlayerCollides(level, player->x, player->y)) {
            player->y_fx = old_y;
            player->vy_fx = 0;
            SyncPublic(player);
            return;
        }
    }
    player->on_ground = false;
}

/* Horizontal movement is a single 8.8 add per frame ($80:8D35/8D46). The
 * 0.5 px sweep only resolves the native AABB against the 16 px block grid. */
static void MoveHorizontal(Player *player, const Level *level, int dir,
                           int32_t speed_fx) {
    float old_x = FX_TO_FLOAT(player->x_fx);
    if (dir == 0) {
        player->vx = 0.0f;
        return;
    }

    float step = FX_TO_FLOAT(speed_fx) * (float)dir;
    float target = old_x + step;
    if (!PlayerCollides(level, target, player->y)) {
        player->x_fx += dir * speed_fx;
    } else {
        float reached = old_x;
        float dist = fabsf(step);
        for (float d = 0.5f; d <= dist; d += 0.5f) {
            float nx = old_x + copysignf(d, step);
            if (PlayerCollides(level, nx, player->y))
                break;
            reached = nx;
        }
        player->x_fx = FLOAT_TO_FX(reached);
    }

    SyncPublic(player);
    player->vx = FX_TO_FLOAT(player->x_fx) - old_x;
}

/* $80:F065 slope walk: first step from rest is 0x00B3, then $8B85 adds
 * 3/256 px/frame until the 0x0166 cap. The flat-ground path ($80:F031)
 * deliberately leaves $1020 alone, so normal walking is immediate 1.4 px/f.
 * Slope detection ($1070) is wired in Step 5. */
static void UpdateWalkSpeed(Player *player, int dir, int prev_dir) {
    if (!player->on_slope)
        return;

    if (dir == 0) {
        player->walk_speed_fx -= PLAYER_WALK_ACCEL_FX;
        if (player->walk_speed_fx < 0)
            player->walk_speed_fx = 0;
        return;
    }

    if (prev_dir == 0) {
        player->walk_speed_fx = PLAYER_WALK_INITIAL_FX;
    } else {
        player->walk_speed_fx += PLAYER_WALK_ACCEL_FX;
        if (player->walk_speed_fx > PLAYER_WALK_MAX_FX)
            player->walk_speed_fx = PLAYER_WALK_MAX_FX;
    }
}

static int ReadDirection(const Input *input) {
    /* $0091 bits 0/1. Pressing both keeps the original rightward bias. */
    if (input->left && input->right)
        return 1;
    if (input->left)
        return -1;
    if (input->right)
        return 1;
    return 0;
}

static void EnterJump(Player *player, const Level *level) {
    (void)level;
    player->on_ground = false;
    player->state = PLAYER_JUMP;
    player->jump_sub = 0;
    player->vy_fx = -PLAYER_JUMP_FX;
    player->walk_speed_fx = PLAYER_WALK_MAX_FX;
    player->land_timer = 0;
    player->jump_started = true;
    SyncPublic(player);
}

static void EnterHover(Player *player) {
    player->state = PLAYER_HOVER;
    player->jump_sub = 0;
    player->vy_fx = 0;
    player->walk_speed_fx = PLAYER_WALK_MAX_FX;
    SyncPublic(player);
}

void Player_Init(Player *player, const Level *level) {
    memset(player, 0, sizeof(*player));
    player->x_fx = FLOAT_TO_FX(level->spawn_x);
    player->y_fx = FLOAT_TO_FX(level->spawn_y);
    const char *start_x = getenv("DC_START_X");
    if (start_x && *start_x)
        player->x_fx = FLOAT_TO_FX(strtof(start_x, NULL));
    const char *start_y = getenv("DC_START_Y");
    if (start_y && *start_y)
        player->y_fx = FLOAT_TO_FX(strtof(start_y, NULL));
    player->walk_speed_fx = PLAYER_WALK_MAX_FX;
    player->facing = 1;
    player->state = PLAYER_IDLE;
    player->anim_clip = -1;
    player->fly_enabled = getenv("DC_FLY_DEBUG") != NULL;
    player->max_hp = 6;
    player->hp = 4; /* leave room for heart pickups */
    SyncPublic(player);

    if (PlayerCollides(level, player->x, player->y)) {
        player->y_fx = (player->y_fx / (level->collision.cell * FX_ONE)) *
                       level->collision.cell * FX_ONE;
        SyncPublic(player);
    }
    float top;
    if (GroundTop(level, player->x, player->y, &top) &&
        top <= player->y + 16.0f) {
        player->y_fx = FLOAT_TO_FX(top);
        player->on_ground = true;
        SyncPublic(player);
    }
}

void Player_Animate(Player *player, const PlayerAnim *anim) {
    if (!anim->ready)
        return;

    AnimClipId clip;
    if (player->special_timer > 0) {
        clip = ANIM_SPELL;
    } else if (player->shot_timer > 0) {
        clip = ANIM_ATTACK;
    } else {
        switch (player->state) {
        case PLAYER_WALK:
            clip = ANIM_WALK;
            break;
        case PLAYER_JUMP:
            /* The recovered state machine separates rise and fall; hover is
             * a distinct State 0x06 before the fall gets here. */
            clip = player->vy_fx < 0 ? ANIM_JUMP : ANIM_FALL;
            break;
        case PLAYER_HOVER:
        case PLAYER_FLY:
            clip = ANIM_HOVER;
            break;
        case PLAYER_LAND:
            clip = ANIM_LAND;
            break;
        default:
            clip = ANIM_IDLE;
            break;
        }
    }

    const AnimClip *c = &anim->clips[clip];
    if (c->step_count <= 0) {
        clip = ANIM_IDLE;
        c = &anim->clips[clip];
    }
    if (!anim->ready || c->step_count <= 0)
        return;

    if (player->anim_clip != (int)clip) {
        player->anim_clip = (int)clip;
        player->anim_step = 0;
        player->anim_timer = 0;
    }
    if (c->step_count <= 1)
        return;

    const AnimStep *step = &anim->steps[c->first_step + player->anim_step];
    player->anim_timer++;
    if (player->anim_timer < step->duration)
        return;

    player->anim_timer = 0;
    player->anim_step++;
    if (player->anim_step >= c->step_count) {
        if (c->loop)
            player->anim_step = 0;
        else
            player->anim_step = c->step_count - 1;
    }
}

void Player_Update(Player *player, const Level *level, const Input *input) {
    player->shot_fired = false;
    player->jump_started = false;
    player->landed_event = false;
    player->hurt_event = false;
    if (player->special_timer > 0)
        player->special_timer--;
    if (player->shot_timer > 0)
        player->shot_timer--;
    if (player->land_timer > 0)
        player->land_timer--;
    if (player->invuln_timer > 0)
        player->invuln_timer--;

    if (input->special_pressed && player->special_timer == 0)
        player->special_timer = PLAYER_SPECIAL_FRAMES;
    if (input->shot_pressed && player->shot_timer == 0) {
        player->shot_timer = PLAYER_SHOT_FRAMES;
        player->shot_fired = true;
    }

    int dir = ReadDirection(input);
    if (player->knockback_timer > 0) {
        player->knockback_timer--;
        dir = player->knockback_dir;
        MoveHorizontal(player, level, dir, 0x0200);
    } else {
        if (dir != 0)
            player->facing = dir;
        UpdateWalkSpeed(player, dir, player->prev_dir);
        MoveHorizontal(player, level, dir, player->walk_speed_fx);
    }
    player->prev_dir = dir;

    bool grounded_at_start = player->on_ground;
    if (grounded_at_start) {
        float top = 0.0f;
        bool supported = GroundTop(level, player->x, player->y, &top);
        /* A surface more than a few pixels below the feet means we walked
         * off a ledge; let State 0x04 start falling instead of snapping. */
        if (supported && top > player->y + 6.0f)
            supported = false;
        if (!supported) {
            /* Walked off the block edge: State 0x04 begins falling. */
            player->on_ground = false;
            player->state = PLAYER_JUMP;
            player->jump_sub = 4;
            player->vy_fx = 0;
            IntegrateVertical(player, level);
        } else {
            player->y_fx = FLOAT_TO_FX(top);
            player->vy_fx = 0;
            SyncPublic(player);

            if (input->jump_pressed) {
                EnterJump(player, level);
            } else if (dir != 0) {
                player->state = PLAYER_WALK;
            } else if (player->land_timer > 0) {
                player->state = PLAYER_LAND;
            } else {
                player->state = PLAYER_IDLE;
            }
        }
    } else {
        if (player->state == PLAYER_HOVER || player->state == PLAYER_FLY) {
            if (input->jump_pressed) {
                /* $80:E2DF: B exits State 0x06 into State 0x04/0x04. */
                player->state = PLAYER_JUMP;
                player->jump_sub = 4;
                player->vy_fx = 0;
            } else if (player->fly_enabled && input->up) {
                player->state = PLAYER_FLY;
                player->vy_fx = -PLAYER_FLY_UP_FX;
                int32_t old_y = player->y_fx;
                player->y_fx += player->vy_fx;
                SyncPublic(player);
                if (PlayerCollides(level, player->x, player->y)) {
                    player->y_fx = old_y;
                    player->vy_fx = 0;
                    SyncPublic(player);
                }
            } else {
                player->state = PLAYER_HOVER;
            }
        } else {
            /* State 0x04: substate 0 and 2 advance first, then integrate.
             * Substate 4 applies the $80:E227 B-release cut before gravity. */
            if (player->jump_sub == 0) {
                player->jump_sub = 2;
                IntegrateVertical(player, level);
            } else if (player->jump_sub == 2) {
                player->jump_sub = 4;
                IntegrateVertical(player, level);
            } else if (player->jump_sub == 4 && input->jump_pressed) {
                EnterHover(player);
            } else {
                player->jump_sub = 4;
                if (player->vy_fx < 0 && !input->jump_held)
                    player->vy_fx = 0;
                IntegrateVertical(player, level);
            }

            if (player->on_ground) {
                player->state = PLAYER_LAND;
                player->land_timer = 8;
            } else if (player->state != PLAYER_HOVER &&
                       player->state != PLAYER_FLY) {
                player->state = PLAYER_JUMP;
            }
        }
    }

    /* Keep Firebrand inside the captured room. A proper screen-transition
     * system will replace these hard bounds later. */
    if (player->x < level->play_min_x) {
        player->x_fx = FLOAT_TO_FX(level->play_min_x);
        player->vx = 0.0f;
    }
    if (player->x > level->play_max_x) {
        player->x_fx = FLOAT_TO_FX(level->play_max_x);
        player->vx = 0.0f;
    }
    if (player->y < level->play_min_y) {
        player->y_fx = FLOAT_TO_FX(level->play_min_y);
        player->vy_fx = 0;
    }
    if (player->y > level->play_max_y) {
        player->y_fx = FLOAT_TO_FX(level->play_max_y);
        player->vy_fx = 0;
        player->on_ground = true;
    }
    SyncPublic(player);

    player->anim_time += 1.0f / 60.0f;
}
