#include "player.h"

#include <math.h>

#define PLAYER_HALF_W 6.0f
#define PLAYER_HEIGHT 22.0f

static bool SolidPoint(const Level *level, float x, float y) {
    return Level_IsSolidAt(level, x, y);
}

static bool PlayerCollides(const Level *level, float x, float y) {
    float left = x - PLAYER_HALF_W;
    float right = x + PLAYER_HALF_W;
    float bottom = y - 1.0f;
    float mid = y - PLAYER_HEIGHT * 0.5f;
    float top = y - PLAYER_HEIGHT + 1.0f;
    return SolidPoint(level, left, bottom) || SolidPoint(level, right, bottom) ||
           SolidPoint(level, left, mid) || SolidPoint(level, right, mid) ||
           SolidPoint(level, left, top) || SolidPoint(level, right, top);
}

static float Approach(float value, float target, float amount) {
    if (value < target)
        return fminf(value + amount, target);
    return fmaxf(value - amount, target);
}

void Player_Init(Player *player, const Level *level) {
    player->x = level->spawn_x;
    player->y = level->spawn_y;
    player->vx = 0.0f;
    player->vy = 0.0f;
    player->on_ground = false;
    player->flying = false;
    player->jump_boosting = false;
    player->facing = 1;
    player->state = PLAYER_IDLE;
    player->anim_time = 0.0f;
    player->hurt_timer = 0.0f;
    player->attack_timer = 0;
    player->land_timer = 0;
    player->anim_clip = -1;
    player->anim_step = 0;
    player->anim_timer = 0;
}

void Player_Animate(Player *player, const PlayerAnim *anim) {
    if (!anim->ready)
        return;

    AnimClipId clip;
    switch (player->state) {
    case PLAYER_WALK:
        clip = ANIM_WALK;
        break;
    case PLAYER_JUMP:
        /* The same state covers the rise and the descent; the recovered
         * state machine switches to hover before the fall gets here. */
        clip = player->vy < -0.05f ? ANIM_JUMP : ANIM_FALL;
        break;
    case PLAYER_HOVER:
    case PLAYER_FLY:
        clip = ANIM_HOVER;
        break;
    case PLAYER_LAND:
        clip = ANIM_LAND;
        break;
    case PLAYER_ATTACK:
        clip = ANIM_ATTACK;
        break;
    default:
        clip = ANIM_IDLE;
        break;
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
    /* Values recovered from the original object/physics code:
     *   $1020 max walk speed 0x0166 -> 1.398 px/frame
     *   jump state sets $22 = 0x0400 -> 4.0 px/frame
     *   gravity $26 = 0x0040 -> 0.25 px/frame^2
     *   fall clamp 0xFA00 -> 6.0 px/frame
     * (bank 80:8D03 vertical integrator; jump init at 85:C1D4.) */
    const float walk_speed = 1.398f;
    const float accel = 0.28f;
    const float friction = 0.28f;
    const float gravity = 0.25f;
    const float jump_speed = 4.0f;

    if (player->attack_timer > 0)
        player->attack_timer--;
    if (player->land_timer > 0)
        player->land_timer--;

    /* One attack animation per press; the visual is always the full clip. */
    if (input->attack_pressed && player->attack_timer == 0)
        player->attack_timer = 31;

    if (input->left && !input->right) {
        player->vx -= accel;
        player->facing = -1;
    } else if (input->right && !input->left) {
        player->vx += accel;
        player->facing = 1;
    } else {
        player->vx = Approach(player->vx, 0.0f, friction);
    }
    if (player->vx > walk_speed)
        player->vx = walk_speed;
    if (player->vx < -walk_speed)
        player->vx = -walk_speed;

    if (player->on_ground && input->jump_pressed) {
        /* A tap gives a short hop. Holding B adds upward acceleration up to
         * the recovered full-jump speed (4.0 px/frame). */
        player->vy = -2.0f;
        player->on_ground = false;
        player->flying = false;
        player->jump_boosting = true;
    }

    if (!player->on_ground) {
        if (input->jump_held && player->jump_boosting && !player->flying &&
            player->vy < 0.0f) {
            player->vy -= 0.10f;
            if (player->vy < -jump_speed)
                player->vy = -jump_speed;
            player->vy += gravity;
            player->state = PLAYER_JUMP;
        } else if (input->jump_held && player->vy > -2.0f) {
            if (input->up) {
                player->flying = true;
                player->vy -= 0.22f;
                if (player->vy < -2.0f)
                    player->vy = -2.0f;
                player->state = PLAYER_FLY;
            } else if (!player->flying) {
                player->vy += gravity * 0.24f;
                if (player->vy > 1.05f)
                    player->vy = 1.05f;
                player->state = PLAYER_HOVER;
            } else {
                player->vy += gravity;
                if (player->vy > 6.0f)
                    player->vy = 6.0f;
                player->state = PLAYER_FLY;
            }
        } else {
            if (!input->jump_held)
                player->jump_boosting = false;
            player->vy += gravity;
            if (player->vy > 6.0f)
                player->vy = 6.0f;
            if (player->state != PLAYER_FLY)
                player->state = PLAYER_JUMP;
        }
    }

    /* Horizontal move, resolved by small steps. */
    float old_x = player->x;
    player->x += player->vx;
    if (PlayerCollides(level, player->x, player->y)) {
        float dir = player->vx > 0.0f ? 1.0f : -1.0f;
        float dist = fabsf(player->vx);
        player->x = old_x;
        for (float d = 0.5f; d <= dist; d += 0.5f) {
            float nx = old_x + dir * d;
            if (PlayerCollides(level, nx, player->y)) {
                break;
            }
            player->x = nx;
        }
        player->vx = 0.0f;
    }

    /* Vertical move. */
    float old_y = player->y;
    player->y += player->vy;
    if (PlayerCollides(level, player->x, player->y)) {
        if (player->vy > 0.0f) {
            float dist = player->vy;
            player->y = old_y;
            for (float d = 0.5f; d <= dist; d += 0.5f) {
                float ny = old_y + d;
                if (PlayerCollides(level, player->x, ny)) {
                    break;
                }
                player->y = ny;
            }
            player->y = floorf(player->y / 16.0f) * 16.0f;
            player->vy = 0.0f;
            player->on_ground = true;
            player->flying = false;
            player->jump_boosting = false;
            player->land_timer = 8;
        } else if (player->vy < 0.0f) {
            player->y = old_y;
            player->vy = 0.0f;
        }
    } else {
        player->on_ground = false;
    }

    /* Ground probe (the bottom point is one pixel below the feet). */
    if (!player->on_ground && player->vy >= 0.0f) {
        if (PlayerCollides(level, player->x, player->y + 1.0f) ||
            SolidPoint(level, player->x - PLAYER_HALF_W, player->y + 1.0f) ||
            SolidPoint(level, player->x + PLAYER_HALF_W, player->y + 1.0f)) {
            player->on_ground = true;
            player->vy = 0.0f;
            player->flying = false;
            player->jump_boosting = false;
            player->land_timer = 8;
        }
    }

    if (player->on_ground) {
        if (fabsf(player->vx) > 0.08f) {
            player->state = PLAYER_WALK;
        } else if (player->land_timer > 0) {
            player->state = PLAYER_LAND;
        } else {
            player->state = PLAYER_IDLE;
        }
    }
    if (player->attack_timer > 0)
        player->state = PLAYER_ATTACK;

    /* Keep Firebrand inside the captured room. A proper screen-transition
     * system will replace these hard bounds later. */
    if (player->x < level->play_min_x) {
        player->x = level->play_min_x;
        player->vx = 0.0f;
    }
    if (player->x > level->play_max_x) {
        player->x = level->play_max_x;
        player->vx = 0.0f;
    }
    if (player->y < level->play_min_y) {
        player->y = level->play_min_y;
        player->vy = 0.0f;
    }
    if (player->y > level->play_max_y) {
        player->y = level->play_max_y;
        player->vy = 0.0f;
        player->on_ground = true;
    }

    player->anim_time += 1.0f / 60.0f;
}
