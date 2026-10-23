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
    o->flags = src->flags;
    o->x_fx = (int32_t)lroundf(src->x * FX_ONE);
    o->y_fx = (int32_t)lroundf(src->y * FX_ONE);
    o->origin_x_fx = o->x_fx;
    o->origin_y_fx = o->y_fx;
    o->hp = 1;
    o->max_hp = 1;
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
    o->flags = src->flags;
    o->x_fx = (int32_t)lroundf(src->x * FX_ONE);
    o->y_fx = (int32_t)lroundf(src->y * FX_ONE);
    o->origin_x_fx = o->x_fx;
    o->origin_y_fx = o->y_fx;
    o->hp = (src->species == 0x26) ? 3 : 2;
    o->max_hp = o->hp;
    o->state = 0; /* emerge / init */
    o->timer = 24;
    o->vx_fx = (src->flags & 1) ? 0x0080 : -0x0080;
}

static void SpawnPlatform(ObjectPool *pool, const LevelObject *src) {
    Object *o = Alloc(pool);
    if (!o)
        return;
    memset(o, 0, sizeof(*o));
    o->active = true;
    o->type = OBJECT_PLATFORM;
    o->species = src->species;
    o->flags = src->flags;
    o->x_fx = (int32_t)lroundf(src->x * FX_ONE);
    o->y_fx = (int32_t)lroundf(src->y * FX_ONE);
    o->origin_x_fx = o->x_fx;
    o->origin_y_fx = o->y_fx;
    o->vx_fx = 0x0080;
}

static void SpawnCrestItem(ObjectPool *pool, const LevelObject *src) {
    Object *o = Alloc(pool);
    if (!o)
        return;
    memset(o, 0, sizeof(*o));
    o->active = true;
    o->type = OBJECT_CREST_ITEM;
    o->species = src->species;
    o->flags = src->flags;
    o->x_fx = (int32_t)lroundf(src->x * FX_ONE);
    o->y_fx = (int32_t)lroundf(src->y * FX_ONE);
    o->origin_x_fx = o->x_fx;
    o->origin_y_fx = o->y_fx;
}

static void SpawnBoss(ObjectPool *pool, const LevelObject *src) {
    Object *o = Alloc(pool);
    if (!o)
        return;
    memset(o, 0, sizeof(*o));
    o->active = true;
    o->type = OBJECT_BOSS;
    o->species = src->species;
    o->flags = src->flags;
    float bx = src->x;
    float by = src->y;
    if (src->species == 0x33 && bx < 120.0f) {
        bx = 220.0f;
        by = 160.0f;
    } else if (src->species == 0xA5 && by < 64.0f) {
        by = 140.0f;
    }
    o->x_fx = (int32_t)lroundf(bx * FX_ONE);
    o->y_fx = (int32_t)lroundf(by * FX_ONE);
    o->origin_x_fx = o->x_fx;
    o->origin_y_fx = o->y_fx;
    o->hp = (src->species == 0x33) ? 6 : 8;
    const char *hp_env = getenv("DC_BOSS_HP");
    if (hp_env && *hp_env)
        o->hp = atoi(hp_env);
    o->max_hp = o->hp;
    o->state = 0;
    pool->boss_alive = true;
    if (getenv("DC_BOSS_DEFEATED") != NULL)
        o->active = false;
}

void Objects_Init(ObjectPool *pool, const Level *level) {
    memset(pool, 0, sizeof(*pool));
    for (int i = 0; i < level->object_count; i++) {
        const LevelObject *o = &level->objects[i];
        if (o->type == LEVEL_OBJECT_URN)
            SpawnUrn(pool, o);
        else if (o->type == LEVEL_OBJECT_ENEMY)
            SpawnEnemy(pool, o);
        else if (o->type == LEVEL_OBJECT_PLATFORM)
            SpawnPlatform(pool, o);
        else if (o->type == LEVEL_OBJECT_PICKUP_CREST)
            SpawnCrestItem(pool, o);
        else if (o->type == LEVEL_OBJECT_BOSS)
            SpawnBoss(pool, o);
    }
}

static bool PlayerTouches(const Object *o, const Player *player) {
    float ox = (float)o->x_fx / FX_ONE;
    float oy = (float)o->y_fx / FX_ONE - HEART_HALF;
    float px = player->x;
    float py = player->y - 10.0f;
    return fabsf(ox - px) < 16.0f && fabsf(oy - py) < 20.0f;
}

static void DamagePlayer(Player *player, float ox, int amount) {
    if (player->invuln_timer > 0 || player->hp <= 0)
        return;
    /* Time/Ultimate Gargoyle and Armor Talisman halve incoming damage. */
    if ((player->crest_form == CREST_TIME ||
         player->crest_form == CREST_ULTIMATE ||
         player->active_talisman == 2) && amount > 1) {
        amount = (amount + 1) / 2;
    }
    player->hp -= amount;
    if (player->hp < 0)
        player->hp = 0;
    player->invuln_timer = 60;
    player->knockback_timer = 8;
    player->knockback_dir = (ox < player->x) ? 1 : -1;
    player->hurt_event = true;
}

void Objects_Update(ObjectPool *pool, const Level *level, Player *player) {
    pool->boss_alive = false;
    for (int i = 0; i < OBJECT_POOL_SIZE; i++) {
        Object *o = &pool->slots[i];
        if (!o->active)
            continue;

        if (o->type == OBJECT_PLATFORM) {
            o->timer++;
            int32_t old_x = o->x_fx;
            o->x_fx += o->vx_fx;
            if (abs(o->x_fx - o->origin_x_fx) > 48 * FX_ONE)
                o->vx_fx = -o->vx_fx;
            float ox = (float)o->x_fx / FX_ONE;
            float oy = (float)o->y_fx / FX_ONE;
            if (player->vy_fx >= 0 &&
                fabsf(player->x - ox) <= 20.0f &&
                player->y >= oy - 4.0f && player->y <= oy + 6.0f) {
                player->y_fx = o->y_fx;
                player->x_fx += (o->x_fx - old_x);
                player->y = oy;
                player->x = (float)player->x_fx / FX_ONE;
                player->vy_fx = 0;
                player->vy = 0.0f;
                player->on_ground = true;
            }
            continue;
        }

        if (o->type == OBJECT_CREST_ITEM) {
            o->timer++;
            if (PlayerTouches(o, player)) {
                if (player->max_hp < 10)
                    player->max_hp++;
                player->hp = player->max_hp;
                player->gold += 50;
                if (player->potions < 5)
                    player->potions++;
                o->active = false;
                pool->events |= OBJECT_EVENT_PICKUP | OBJECT_EVENT_CREST_PICKUP;
            }
            continue;
        }

        if (o->type == OBJECT_BOSS) {
            pool->boss_alive = true;
            o->timer++;
            int32_t dx = player->x_fx - o->x_fx;
            int32_t dy = (player->y_fx - 16 * FX_ONE) - o->y_fx;
            if (o->species == 0x33) {
                /* Somulo (Area 0 colosseum dragon): lunge and leap cycles. */
                int phase = o->timer % 120;
                if (phase < 60) {
                    o->x_fx += (dx > 0) ? 0x0120 : -0x0120;
                } else if (phase < 90) {
                    o->y_fx -= 0x0100;
                } else {
                    float surf;
                    float ox = (float)o->x_fx / FX_ONE;
                    float oy = (float)o->y_fx / FX_ONE;
                    o->y_fx += 0x0180;
                    if (Level_FloorAt(level, ox, oy, &surf) && oy >= surf)
                        o->y_fx = (int32_t)lroundf(surf * FX_ONE);
                }
            } else {
                /* Hippogriff / Stage Boss: swoop and hover above Firebrand. */
                if (abs(dx) > 12 * FX_ONE)
                    o->x_fx += (dx > 0) ? 0x0100 : -0x0100;
                int32_t target_y = o->origin_y_fx +
                                   (int32_t)(sinf(o->timer * 0.06f) * 28.0f * FX_ONE);
                if ((o->timer % 140) > 90)
                    target_y = player->y_fx - 12 * FX_ONE;
                if (o->y_fx < target_y)
                    o->y_fx += 0x00C0;
                else if (o->y_fx > target_y)
                    o->y_fx -= 0x00C0;
                (void)dy;
            }
            float ox = (float)o->x_fx / FX_ONE;
            float oy = (float)o->y_fx / FX_ONE;
            if (fabsf(ox - player->x) < 20.0f &&
                fabsf(oy - (player->y - 11.0f)) < 22.0f) {
                if (player->dash_timer > 0) {
                    o->hp--;
                    pool->events |= OBJECT_EVENT_ENEMY_HIT;
                    player->dash_timer = 0;
                    if (o->hp <= 0) {
                        o->active = false;
                        pool->boss_alive = false;
                        pool->boss_defeated = true;
                        pool->events |= OBJECT_EVENT_BOSS_DEFEATED;
                        SpawnPickup(pool, ox, oy);
                    }
                } else {
                    DamagePlayer(player, ox, 1);
                }
            }
            continue;
        }

        if (o->type == OBJECT_ENEMY) {
            float ox = (float)o->x_fx / FX_ONE;
            float oy = (float)o->y_fx / FX_ONE;
            o->timer++;
            if (o->species == 0x2C) {
                /* Area 2 Skeleton walker: ground patrol. */
                o->state = 1;
                o->x_fx += o->vx_fx;
                ox = (float)o->x_fx / FX_ONE;
                float surf;
                if (Level_IsWallAt(level, ox + (o->vx_fx > 0 ? 8.0f : -8.0f),
                                   oy - 8.0f) ||
                    abs(o->x_fx - o->origin_x_fx) > 96 * FX_ONE) {
                    o->vx_fx = -o->vx_fx;
                }
                if (Level_FloorAt(level, ox, oy + 4.0f, &surf))
                    o->y_fx = (int32_t)lroundf(surf * FX_ONE);
            } else if (o->species == 0x07) {
                /* Area 2/3 Bat/Watcher: sine wave swoop. */
                o->state = 1;
                int32_t dx = player->x_fx - o->x_fx;
                if (abs(dx) < 200 * FX_ONE) {
                    o->x_fx += (dx > 0) ? 0x0080 : -0x0080;
                    o->y_fx = o->origin_y_fx +
                              (int32_t)(sinf(o->timer * 0.08f) * 18.0f * FX_ONE);
                }
            } else {
                /* Species $67 Graveyard Ghoul: authentic $83:AF74 state 0.
                 *
                 * The original dispatches on substate ($06):
                 *   0  face the player, then wind up for 3 frames ($3B = 3)
                 *   2  launch: $20 = 0x0050 ($0088 / $0070 variants),
                 *      $22 = 0x0398 ($04D0 variant), $39 = 5
                 *   4  airborne: gravity $26 = 0x0040 until ground contact
                 *      ($04 bit 2), then $B2C7 snaps Y (+7) and sets $3B = 6
                 *   6  landed: hold for 6 frames, then restart at 0
                 * The remaining behaviour states (dispatch through $03 and the
                 * random table $DEA2) are not ported yet; this is the hop.
                 */
                if (o->state == 0) {
                    /* First-pass emergence: hold position, then hop-cycle. */
                    if (o->timer >= 24) {
                        o->state = 1;
                        o->timer = 0;
                        o->substate = 0;
                    }
                    continue;
                }

                if (o->state == 1) {
                    /* $B2B1 lifts Y by 9 ($34 -= 9, y-down), sets $06 = 4 and
                     * then $22 = 0x0398 with gravity $26 = 0x0040: a drop from
                     * the perch these ghouls hang at. */
                    if (o->substate == 0) {
                        o->facing = (player->x_fx >= o->x_fx) ? 1 : -1;
                        o->substate = 2;
                        o->phase = 3; /* $3B = 3 windup */
                    } else if (o->substate == 2) {
                        if (--o->phase <= 0) {
                            o->y_fx -= 9 * FX_ONE;
                            o->vx_fx = o->facing * 0x0050; /* $20 */
                            o->vy_fx = 0x0398; /* $22: down 3.6 px/frame */
                            o->substate = 4;
                            o->phase = 90;
                        }
                    } else if (o->substate == 4) {
                        o->x_fx += o->vx_fx;
                        o->y_fx += o->vy_fx;
                        o->vy_fx += 0x0040; /* $26 gravity */
                        float fx = (float)o->x_fx / FX_ONE;
                        float fy = (float)o->y_fx / FX_ONE;
                        float surf;
                        bool landed =
                            Level_FloorAt(level, fx, fy + 2.0f, &surf) &&
                            surf <= fy + 6.0f;
                        if (landed || --o->phase <= 0) {
                            if (landed)
                                o->y_fx = (int32_t)lroundf(surf * FX_ONE);
                            o->vx_fx = 0;
                            o->vy_fx = 0;
                            o->substate = 6;
                            o->phase = 6; /* $3B = 6 */
                        }
                    } else if (--o->phase <= 0) {
                        /* Landed. The original rotates through behaviour
                         * states chosen from $03/$DEA2; those are not ported
                         * yet, so continue with the chase fallback. */
                        o->state = 2;
                        o->substate = 0;
                    }
                } else if (o->state >= 2) {
                    /* Placeholder approach until the $DEA2 behaviour states
                     * are ported: follow the player on both axes. */
                    int32_t dx = player->x_fx - o->x_fx;
                    int32_t dy = (player->y_fx - 11 * FX_ONE) - o->y_fx;
                    o->facing = dx >= 0 ? 1 : -1;
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
                }
            }

            ox = (float)o->x_fx / FX_ONE;
            oy = (float)o->y_fx / FX_ONE;
            float px = player->x;
            float py = player->y - 10.0f;
            if (fabsf(ox - px) < 14.0f && fabsf(oy - py) < 18.0f) {
                if (player->dash_timer > 0) {
                    o->hp = 0;
                    o->active = false;
                    pool->events |= OBJECT_EVENT_ENEMY_HIT;
                    SpawnPickup(pool, ox, oy + 8.0f);
                } else {
                    DamagePlayer(player, ox, 1);
                }
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
            player->gold += 10;
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
        int dmg = p->damage > 0 ? p->damage : 1;

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
                    o->hp -= dmg;
                    pool->events |= OBJECT_EVENT_ENEMY_HIT;
                    if (o->hp <= 0) {
                        SpawnPickup(pool, ox, oy + 8.0f);
                        o->active = false;
                    }
                    break;
                }
            } else if (o->type == OBJECT_BOSS) {
                if (fabsf(px - ox) <= 20.0f && fabsf(py - oy) <= 24.0f) {
                    p->active = false;
                    o->hp -= dmg;
                    pool->events |= OBJECT_EVENT_ENEMY_HIT;
                    if (o->hp <= 0) {
                        o->active = false;
                        pool->boss_alive = false;
                        pool->boss_defeated = true;
                        pool->events |= OBJECT_EVENT_BOSS_DEFEATED;
                        SpawnPickup(pool, ox, oy + 8.0f);
                    }
                    break;
                }
            }
        }
    }
}
