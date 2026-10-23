#include "object.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FX_ONE 256
#define HEART_POPUP_FX 0x0400 /* $85:C1D1 initial upward velocity 4.0 */
#define HEART_GRAVITY_FX 0x0040 /* $85:C1D1 gravity 0.25 */
#define HEART_BOUNCE_FX 0x0400  /* $85:C1D1 bounce decay 4.0 -> 3.0 ... */
#define HEART_BOUNCE_STEP 0x0100

#define URN_HALF 8.0f
#define HEART_HALF 8.0f

static Object *Alloc(ObjectPool *pool) {
    for (int i = 0; i < OBJECT_POOL_SIZE; i++) {
        if (!pool->slots[i].active)
            return &pool->slots[i];
    }
    return NULL;
}

static void SpawnUrn(ObjectPool *pool, const LevelObject *src) {
    Object *o = Alloc(pool);
    if (!o)
        return;
    memset(o, 0, sizeof(*o));
    o->active = true;
    o->type = OBJECT_URN;
    o->species = src->species;
    o->x_fx = (int32_t)lroundf(src->x * FX_ONE);
    o->y_fx = (int32_t)lroundf(src->y * FX_ONE);
    o->hp = 1;
}

static void SpawnPickup(ObjectPool *pool, float x, float bottom_y) {
    Object *o = Alloc(pool);
    if (!o)
        return;
    memset(o, 0, sizeof(*o));
    o->active = true;
    o->type = OBJECT_PICKUP;
    o->x_fx = (int32_t)lroundf(x * FX_ONE);
    o->y_fx = (int32_t)lroundf(bottom_y * FX_ONE);
    o->vy_fx = -HEART_POPUP_FX; /* object convention: positive is up */
    o->bounce_fx = HEART_BOUNCE_FX;
    o->state = 0;
}

static void SpawnEnemy(ObjectPool *pool, const LevelObject *src) {
    Object *o = Alloc(pool);
    if (!o)
        return;
    memset(o, 0, sizeof(*o));
    o->active = true;
    o->type = OBJECT_ENEMY;
    o->species = src->species;
    o->x_fx = (int32_t)lroundf(src->x * FX_ONE);
    o->y_fx = (int32_t)lroundf(src->y * FX_ONE);
    o->hp = 2;
    o->state = 0; /* emerge */
    o->timer = 24;
}

void Objects_Init(ObjectPool *pool, const Level *level) {
    memset(pool, 0, sizeof(*pool));
    for (int i = 0; i < level->object_count; i++) {
        const LevelObject *o = &level->objects[i];
        if (o->type == LEVEL_OBJECT_URN)
            SpawnUrn(pool, o);
        else if (o->type == LEVEL_OBJECT_ENEMY)
            SpawnEnemy(pool, o);
    }
}

static bool PlayerTouches(const Object *o, const Player *player) {
    float ox = (float)o->x_fx / FX_ONE;
    float oy = (float)o->y_fx / FX_ONE - HEART_HALF;
    float px = player->x;
    float py = player->y - 10.0f;
    return fabsf(ox - px) < 16.0f && fabsf(oy - py) < 20.0f;
}

void Objects_Update(ObjectPool *pool, const Level *level, Player *player) {
    for (int i = 0; i < OBJECT_POOL_SIZE; i++) {
        Object *o = &pool->slots[i];
        if (!o->active)
            continue;

        if (o->type == OBJECT_ENEMY) {
            float ox = (float)o->x_fx / FX_ONE;
            float oy = (float)o->y_fx / FX_ONE;
            o->timer++;
            if (o->state == 0) {
                /* First-pass emergence: hold position, then chase. */
                if (o->timer >= 24) {
                    o->state = 1;
                    o->timer = 0;
                }
                continue;
            }

            int32_t dx = player->x_fx - o->x_fx;
            int32_t dy = (player->y_fx - 11 * FX_ONE) - o->y_fx;
            if (abs(dx) < 220 * FX_ONE) {
                if (dx > 8 * FX_ONE)
                    o->x_fx += 0x0099; /* 0.6 px/frame */
                else if (dx < -8 * FX_ONE)
                    o->x_fx -= 0x0099;
                if (dy > 6 * FX_ONE)
                    o->y_fx += 0x0066; /* 0.4 px/frame */
                else if (dy < -6 * FX_ONE)
                    o->y_fx -= 0x0066;
            }

            ox = (float)o->x_fx / FX_ONE;
            oy = (float)o->y_fx / FX_ONE;
            float px = player->x;
            float py = player->y - 10.0f;
            if (player->invuln_timer == 0 &&
                fabsf(ox - px) < 14.0f && fabsf(oy - py) < 18.0f) {
                if (player->hp > 0)
                    player->hp--;
                player->invuln_timer = 60;
                player->knockback_timer = 8;
                player->knockback_dir = (ox < px) ? 1 : -1;
                player->hurt_event = true;
            }
            continue;
        }

        if (o->type != OBJECT_PICKUP)
            continue;

        if (o->state == 0) {
            o->vy_fx += HEART_GRAVITY_FX;
            if (o->vy_fx > 0x0600)
                o->vy_fx = 0x0600;
            o->y_fx += o->vy_fx;

            float surface;
            float x = (float)o->x_fx / FX_ONE;
            float y = (float)o->y_fx / FX_ONE;
            if (Level_FloorAt(level, x, y, &surface) && y >= surface) {
                o->y_fx = (int32_t)lroundf(surface * FX_ONE);
                o->bounce_fx -= HEART_BOUNCE_STEP;
                if (o->bounce_fx > 0) {
                    o->vy_fx = -o->bounce_fx;
                } else {
                    o->bounce_fx = 0;
                    o->vy_fx = 0;
                    o->state = 1; /* at rest */
                }
            }
        }

        if (PlayerTouches(o, player)) {
            if (player->hp < player->max_hp)
                player->hp++;
            o->active = false;
            pool->events |= OBJECT_EVENT_PICKUP;
        }
    }
}

void Objects_CheckProjectiles(ObjectPool *pool, ProjectilePool *projectiles) {
    for (int i = 0; i < PROJECTILE_POOL_SIZE; i++) {
        Projectile *p = &projectiles->slots[i];
        if (!p->active)
            continue;
        float px = (float)p->x_fx / FX_ONE;
        float py = (float)p->y_fx / FX_ONE;

        for (int j = 0; j < OBJECT_POOL_SIZE; j++) {
            Object *o = &pool->slots[j];
            if (!o->active)
                continue;
            float ox = (float)o->x_fx / FX_ONE;
            float oy = (float)o->y_fx / FX_ONE;

            if (o->type == OBJECT_URN) {
                if (fabsf(px - ox) <= URN_HALF + 4.0f &&
                    fabsf(py - oy) <= URN_HALF + 4.0f) {
                    o->active = false;
                    p->active = false;
                    pool->events |= OBJECT_EVENT_BREAK;
                    SpawnPickup(pool, ox, oy + URN_HALF);
                    break;
                }
            } else if (o->type == OBJECT_ENEMY && o->state != 0) {
                if (fabsf(px - ox) <= 12.0f && fabsf(py - oy) <= 12.0f) {
                    p->active = false;
                    o->hp--;
                    pool->events |= OBJECT_EVENT_ENEMY_HIT;
                    if (o->hp <= 0) {
                        SpawnPickup(pool, ox, oy + 8.0f);
                        o->active = false;
                    }
                    break;
                }
            }
        }
    }
}
