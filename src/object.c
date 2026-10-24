#include "object.h"
#include <stdio.h>

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

/* Hostile projectile spawned by the species $67 ghoul ($82877B in the
 * original): the child object runs its own species handler, so the native port
 * models it as a straight-flying object with the captured $67 sprite. */
static void SpawnEnemyShot(ObjectPool *pool, float x, float y, int32_t vx,
                           int32_t vy) {
    Object *o = Alloc(pool);
    if (!o)
        return;
    memset(o, 0, sizeof(*o));
    o->active = true;
    o->type = OBJECT_ENEMY_SHOT;
    o->species = 0x67;
    o->x_fx = (int32_t)lroundf(x * FX_ONE);
    o->y_fx = (int32_t)lroundf(y * FX_ONE);
    o->origin_x_fx = o->x_fx;
    o->origin_y_fx = o->y_fx;
    o->vx_fx = vx;
    o->vy_fx = vy;
    o->hp = 1;
}

/* Somulo's fireball: a normal hostile shot, but marked so that hitting the
 * ground spawns a firewall (its `substate` is used as a flag). */
static void SpawnBossFireball(ObjectPool *pool, float x, float y, int32_t vx,
                              int32_t vy) {
    SpawnEnemyShot(pool, x, y, vx, vy);
    for (int i = 0; i < OBJECT_POOL_SIZE; i++) {
        Object *o = &pool->slots[i];
        if (o->active && o->type == OBJECT_ENEMY_SHOT &&
            o->x_fx == (int32_t)lroundf(x * FX_ONE) &&
            o->y_fx == (int32_t)lroundf(y * FX_ONE)) {
            o->substate = 1;
            o->species = 0x34; /* the fireball object in Area 0 */
            break;
        }
    }
}

/* Firewall: three short-lived flames standing on the ground where a fireball
 * landed. They are enemy shots flagged with state 1 so the update path keeps
 * them in place. */
static void SpawnFirewall(ObjectPool *pool, float x, float y) {
    for (int k = -1; k <= 1; k++) {
        Object *o = Alloc(pool);
        if (!o)
            return;
        memset(o, 0, sizeof(*o));
        o->active = true;
        o->type = OBJECT_ENEMY_SHOT;
        o->species = 0x34;
        o->state = 1;
        o->x_fx = (int32_t)lroundf((x + (float)k * 9.0f) * FX_ONE);
        o->y_fx = (int32_t)lroundf(y * FX_ONE);
        o->origin_x_fx = o->x_fx;
        o->origin_y_fx = o->y_fx;
        o->hp = 1;
    }
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
    pool->boss_present = true;
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
    /* Test hook: place the boss where a grounded demo can reach it. */
    const char *bx_env = getenv("DC_BOSS_X");
    const char *by_env = getenv("DC_BOSS_Y");
    if (bx_env && *bx_env)
        bx = (float)atof(bx_env);
    if (by_env && *by_env)
        by = (float)atof(by_env);
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
    if (getenv("DC_BOSS_DEFEATED") != NULL) {
        /* Test hook: the boss starts already defeated so the clear flow can be
         * exercised without fighting. Set the full defeat state, not just an
         * inactive slot. */
        o->active = false;
        o->hp = 0;
        pool->boss_alive = false;
        pool->boss_defeated = true;
        pool->events |= OBJECT_EVENT_BOSS_DEFEATED;
    }
}

/* City NPCs ($96): inert, talkable, drawn from the captured sheet. */
static void SpawnNpc(ObjectPool *pool, const LevelObject *src) {
    Object *o = Alloc(pool);
    if (!o)
        return;
    memset(o, 0, sizeof(*o));
    o->active = true;
    o->type = OBJECT_NPC;
    o->species = (uint8_t)(src->species ? src->species : 0x96);
    o->x_fx = (int32_t)lroundf(src->x * FX_ONE);
    o->y_fx = (int32_t)lroundf(src->y * FX_ONE);
    o->origin_x_fx = o->x_fx;
    o->origin_y_fx = o->y_fx;
    o->hp = 1;
    o->facing = -1;
}

void Objects_Init(ObjectPool *pool, const Level *level) {
    memset(pool, 0, sizeof(*pool));
    if (getenv("DC_DEBUG_OBJ"))
        fprintf(stderr, "objects: init with %d level objects\n",
                level->object_count);
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
        else if (o->type == LEVEL_OBJECT_NPC)
            SpawnNpc(pool, o);
    }
    if (getenv("DC_DEBUG_OBJ")) {
        int n = 0;
        for (int i = 0; i < OBJECT_POOL_SIZE; i++)
            if (pool->slots[i].active) {
                fprintf(stderr, "objects: slot %d type %d species %d at %.0f,%.0f\n",
                        i, (int)pool->slots[i].type, pool->slots[i].species,
                        pool->slots[i].x_fx / 256.0,
                        pool->slots[i].y_fx / 256.0);
                n++;
            }
        fprintf(stderr, "objects: %d active\n", n);
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

        if (o->type == OBJECT_NPC) {
            o->timer++;
            continue; /* talkable but inert */
        }

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
                /* Crest pieces in the original grant the next Fire-Crest
                 * element ($BE:A427..A533: Buster, Tornado, Claw, Demon Fire)
                 * before they become plain stat upgrades. */
                if (player->fire_element < 4) {
                    player->fire_element++;
                    pool->last_fire_piece = player->fire_element;
                    pool->events |= OBJECT_EVENT_FIRE_PIECE;
                }
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
                /* Somulo, the colosseum dragon. The head is the weak point and
                 * the fight is a fireball duel: idle -> windup -> fire (low or
                 * high, aimed at Firebrand) -> recover, with a stagger when the
                 * head is hit. Six head hits kill him. */
                if (o->hit_flash > 0 && o->state != 4) {
                    o->state = 4;   /* stagger */
                    o->phase = 36;
                }
                o->phase--;
                if (o->phase <= 0) {
                    switch (o->state) {
                    case 0: /* idle, breathing */
                        o->state = 1;
                        o->phase = 70;
                        break;
                    case 1: /* windup: jaw opens */
                        o->state = 2;
                        o->phase = 1;
                        break;
                    case 2: { /* fire */
                        float ox = (float)o->x_fx / FX_ONE;
                        float oy = (float)o->y_fx / FX_ONE;
                        float mouth_x = ox + ((dx > 0) ? 26.0f : -26.0f);
                        float mouth_y = oy + 10.0f;
                        bool low = (o->hops++ & 1) == 0;
                        float target_y = low ? (player->y - 4.0f)
                                             : (player->y - 26.0f);
                        float tx = player->x - mouth_x;
                        float ty = target_y - mouth_y;
                        float len = sqrtf(tx * tx + ty * ty);
                        if (len < 0.001f)
                            len = 1.0f;
                        SpawnBossFireball(pool, mouth_x, mouth_y,
                                          (int32_t)(tx / len * 1.6f * FX_ONE),
                                          (int32_t)(ty / len * 1.6f * FX_ONE));
                        o->state = 3;
                        o->phase = 46;
                        break;
                    }
                    case 3: /* recover */
                        o->state = 0;
                        o->phase = 40;
                        break;
                    case 4: /* stagger over: back to idle */
                        o->state = 0;
                        o->phase = 30;
                        break;
                    default:
                        o->state = 0;
                        o->phase = 30;
                        break;
                    }
                }
                float surf;
                float ox = (float)o->x_fx / FX_ONE;
                float oy = (float)o->y_fx / FX_ONE;
                if (Level_FloorAt(level, ox, oy, &surf) && oy >= surf)
                    o->y_fx = (int32_t)lroundf(surf * FX_ONE);
            } else {
                /* Hippogriff / Stage Boss: swoop and hover above Firebrand,
                 * and punish with a three-shot spread every 110 frames. */
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
                if (o->timer % 110 == 40) {
                    float bx = (float)o->x_fx / FX_ONE;
                    float by = (float)o->y_fx / FX_ONE;
                    float tx = player->x - bx;
                    float ty = (player->y - 10.0f) - by;
                    float len = sqrtf(tx * tx + ty * ty);
                    if (len < 0.001f)
                        len = 1.0f;
                    tx /= len;
                    ty /= len;
                    for (int k = -1; k <= 1; k++) {
                        float ang = (float)k * 0.35f;
                        float cx = tx * cosf(ang) - ty * sinf(ang);
                        float cy = tx * sinf(ang) + ty * cosf(ang);
                        SpawnEnemyShot(pool, bx, by, (int32_t)(cx * 1.4f * FX_ONE),
                                       (int32_t)(cy * 1.4f * FX_ONE));
                    }
                }
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
            } else if (o->species == 0x67) {
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
                            /* $22 = 0x0398, or 0x04D0 on odd $38 ($B019). */
                            o->vy_fx = (o->hops & 1) ? 0x04D0 : 0x0398;
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
                        /* Landed ($B034): the original runs 3 hops ($38),
                         * alternating facing each time ($0A bit 6 / $09 swap)
                         * and restarting the hop at $AFBC, then returns to
                         * substate 0 to re-decide. The behaviour states chosen
                         * from $03/$DEA2 are not ported, so after the burst
                         * the placeholder chase takes over. */
                        o->hops++;
                        if (o->hops >= 3) {
                            o->state = 2;
                            o->substate = 0;
                        } else {
                            o->facing = -o->facing; /* $0A ^= $40, $09 swapped */
                            o->substate = 2;
                            o->phase = 3;
                        }
                    }
                } else if (o->state == 2) {
                    /* $B0F1/$B102/$B120: face the player, then fire three
                     * projectiles on a 64-frame cadence ($3C = 0x40, $3D = 3,
                     * spawn at x +-0x13, y - 0x14), then hop again.
                     * Outside firing range the $DEA2 behaviour states would
                     * take over; until those are ported it closes distance with
                     * the placeholder approach so the player can still engage. */
                    int32_t dx = player->x_fx - o->x_fx;
                    o->facing = dx >= 0 ? 1 : -1;
                    if (labs((long)dx) > 200 * FX_ONE) {
                        int32_t dy = (player->y_fx - 11 * FX_ONE) - o->y_fx;
                        if (dx > 8 * FX_ONE)
                            o->x_fx += 0x0099;
                        else if (dx < -8 * FX_ONE)
                            o->x_fx -= 0x0099;
                        if (dy > 6 * FX_ONE)
                            o->y_fx += 0x0066;
                        else if (dy < -6 * FX_ONE)
                            o->y_fx -= 0x0066;
                    } else if (o->phase > 0) {
                        o->phase--;
                    } else if (o->burst < 3) {
                        /* $82877B spawns at x +-0x13, y - 0x14 (pixels). The
                         * child flies toward Firebrand here so the volley is a
                         * real threat even though the child handler itself is
                         * not ported yet. */
                        float ox2 = (float)o->x_fx / FX_ONE +
                                    (float)(o->facing * 19) / 1.0f;
                        float oy2 = (float)o->y_fx / FX_ONE - 20.0f;
                        float dxp = player->x - ox2;
                        float dyp = (player->y - 10.0f) - oy2;
                        float len = sqrtf(dxp * dxp + dyp * dyp);
                        if (len < 0.001f)
                            len = 1.0f;
                        int32_t svx = (int32_t)(dxp / len * 1.5f * FX_ONE);
                        int32_t svy = (int32_t)(dyp / len * 1.5f * FX_ONE);
                        SpawnEnemyShot(pool, ox2, oy2, svx, svy);
                        o->burst++;
                        o->phase = 64; /* $3C = 0x40 */
                    } else {
                        o->state = 3; /* $B0DE: $05 = 4 (leap attack) */
                        o->substate = 0;
                        o->hops = 0;
                        o->burst = 0;
                    }
                } else if (o->state == 3) {
                    /* $B065 state 1: face the player, 32-frame windup, then a
                     * big leap ($20 = 0x0120, $22 = 0x0540, collision $DE7F);
                     * on landing ($B0AF) hold 16 frames and go back to firing
                     * ($B0DD sets $05 = 4).
                     * $B2E8 decides when to commit: only when the player is
                     * above/beside the perch and $47 is set. Here the same
                     * decision is expressed as "player is lower or close, and
                     * outside firing range" so the ghoul leaves its ledge. */
                    if (o->substate == 0) {
                        int32_t dx = player->x_fx - o->x_fx;
                        bool wants = (player->y_fx > o->y_fx + 6 * FX_ONE &&
                                      labs((long)dx) < 200 * FX_ONE) ||
                                     labs((long)dx) > 60 * FX_ONE;
                        if (!wants) {
                            o->state = 2; /* stay and fire */
                            o->substate = 0;
                            o->burst = 0;
                            o->phase = 0;
                        } else {
                            o->facing = dx >= 0 ? 1 : -1;
                            o->substate = 2;
                            o->phase = 0x20; /* $3B = 0x20 */
                        }
                    } else if (o->substate == 2) {
                        if (--o->phase <= 0) {
                            o->vx_fx = o->facing * 0x0120; /* $20 */
                            o->vy_fx = 0x0540; /* $22: 5.25 px/frame, y-down dive */
                            o->substate = 4;
                            o->phase = 120;
                            o->timer = 0;   /* airborne frames */
                            o->hops = 0;    /* reuse as launch Y (px*256) bucket */
                        }
                    } else if (o->substate == 4) {
                        o->timer++;
                        o->x_fx += o->vx_fx;
                        o->y_fx += o->vy_fx;
                        o->vy_fx += 0x0040; /* $26 gravity */
                        float fx = (float)o->x_fx / FX_ONE;
                        float fy = (float)o->y_fx / FX_ONE;
                        float surf;
                        /* Ignore the ledge the dive started from: require a few
                         * airborne frames and a real drop before landing. */
                        bool fell = o->timer > 5;
                        bool landed = fell && o->vy_fx > 0 &&
                                      Level_FloorAt(level, fx, fy + 2.0f,
                                                    &surf) &&
                                      surf <= fy + 6.0f;
                        if (landed || --o->phase <= 0) {
                            if (landed)
                                o->y_fx = (int32_t)lroundf(surf * FX_ONE);
                            o->vx_fx = 0;
                            o->vy_fx = 0;
                            o->substate = 6;
                            o->phase = 0x10; /* $3B = 0x10 */
                        }
                    } else if (--o->phase <= 0) {
                        o->state = 2;
                        o->substate = 0;
                        o->burst = 0;
                        o->phase = 0;
                    }
                } else if (o->state >= 4) {
                    /* Remaining $DEA2 behaviour states: approach fallback. */
                    int32_t dx = player->x_fx - o->x_fx;
                    int32_t dy = (player->y_fx - 11 * FX_ONE) - o->y_fx;
                    o->facing = dx >= 0 ? 1 : -1;
                    if (abs(dx) < 220 * FX_ONE) {
                        if (dx > 8 * FX_ONE)
                            o->x_fx += 0x0099;
                        else if (dx < -8 * FX_ONE)
                            o->x_fx -= 0x0099;
                        if (dy > 6 * FX_ONE)
                            o->y_fx += 0x0066;
                        else if (dy < -6 * FX_ONE)
                            o->y_fx -= 0x0066;
                    }
                }
            } else {
                /* Generic enemy for species whose ROM handler is not ported yet.
                 * Placement decides the style: enemies spawned well above a
                 * floor fly (hover + 2D chase), grounded ones patrol and walk
                 * toward the player. Keeps rooms lively without the ghoul's hop
                 * behaviour leaking into every other species. */
                int32_t dx = player->x_fx - o->x_fx;
                int32_t dy = player->y_fx - o->y_fx;
                bool near = labs((long)dx) < 140 * FX_ONE &&
                            labs((long)dy) < 64 * FX_ONE;
                float spawn_x = (float)o->origin_x_fx / FX_ONE;
                float spawn_y = (float)o->origin_y_fx / FX_ONE;
                float floor_y = 0.0f;
                /* No floor under the spawn also means "flying": those spawns
                 * hang over pits or above the walkable layer. */
                bool airborne = !Level_FloorAt(level, spawn_x, spawn_y,
                                               &floor_y) ||
                                floor_y - spawn_y > 24.0f;
                if (airborne) {
                    if (o->state == 2) {
                        /* Dive attack: locked velocity for a few frames. */
                        o->x_fx += o->vx_fx;
                        o->y_fx += o->vy_fx;
                        if (--o->phase <= 0) {
                            o->state = 0;
                            o->phase = 90;
                            o->vx_fx = 0;
                            o->vy_fx = 0;
                        }
                    } else {
                        /* Flyer: drift toward the player, bob around spawn. */
                        o->facing = dx >= 0 ? 1 : -1;
                        if (labs((long)dx) > 8 * FX_ONE)
                            o->x_fx += o->facing * 0x0070;
                        float target_y = (float)o->origin_y_fx / FX_ONE +
                                         sinf(o->timer * 0.07f) * 14.0f;
                        if (fabsf(player->y - 14.0f - target_y) < 60.0f)
                            target_y = player->y - 14.0f;
                        float cur_y = (float)o->y_fx / FX_ONE;
                        if (target_y > cur_y + 1.0f)
                            o->y_fx += 0x0050;
                        else if (target_y < cur_y - 1.0f)
                            o->y_fx -= 0x0050;
                        if (o->phase > 0)
                            o->phase--;
                        else if (labs((long)dx) < 64 * FX_ONE &&
                                 labs((long)dy) < 180 * FX_ONE) {
                            /* Swoop at the player's current position. */
                            float ddx = (float)dx;
                            float ddy = (float)dy;
                            float len = sqrtf(ddx * ddx + ddy * ddy);
                            if (len < 0.001f)
                                len = 1.0f;
                            o->vx_fx = (int32_t)(ddx / len * 3.0f * FX_ONE);
                            o->vy_fx = (int32_t)(ddy / len * 3.0f * FX_ONE);
                            o->state = 2;
                            o->phase = 22;
                        }
                    }
                } else {
                    if (o->state == 2) {
                        /* Ground lunge: short horizontal burst. */
                        o->x_fx += o->facing * 0x0200;
                        float gx = (float)o->x_fx / FX_ONE;
                        float gy = (float)o->y_fx / FX_ONE;
                        float surf;
                        if (Level_FloorAt(level, gx, gy + 4.0f, &surf))
                            o->y_fx = (int32_t)lroundf(surf * FX_ONE);
                        if (--o->phase <= 0) {
                            o->state = 0;
                            o->phase = 80;
                        }
                    } else {
                        if (near) {
                            o->facing = dx >= 0 ? 1 : -1;
                            if (labs((long)dx) > 10 * FX_ONE)
                                o->x_fx += o->facing * 0x0068;
                        } else {
                            if (o->vx_fx == 0)
                                o->vx_fx = 0x0048;
                            o->x_fx += o->vx_fx;
                            if (labs((long)(o->x_fx - o->origin_x_fx)) >
                                48 * FX_ONE)
                                o->vx_fx = -o->vx_fx;
                        }
                        float gx = (float)o->x_fx / FX_ONE;
                        float gy = (float)o->y_fx / FX_ONE;
                        float surf;
                        if (Level_FloorAt(level, gx, gy + 4.0f, &surf))
                            o->y_fx = (int32_t)lroundf(surf * FX_ONE);
                        if (o->phase > 0)
                            o->phase--;
                        else if (near && labs((long)dx) > 14 * FX_ONE &&
                                 labs((long)dx) < 52 * FX_ONE) {
                            o->state = 2;
                            o->phase = 14;
                        }
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

        if (o->hit_flash > 0)
            o->hit_flash--;

        if (o->type == OBJECT_ENEMY_SHOT) {
            o->timer++;
            float sx = (float)o->x_fx / FX_ONE;
            float sy = (float)o->y_fx / FX_ONE;
            if (o->state == 1) {
                /* Firewall flame: stays put, burns for a while. */
                if (o->timer > 90)
                    o->active = false;
                else if (fabsf(sx - player->x) < 9.0f &&
                         fabsf(sy - (player->y - 6.0f)) < 16.0f)
                    DamagePlayer(player, sx, 1);
                continue;
            }
            o->x_fx += o->vx_fx;
            o->y_fx += o->vy_fx;
            sx = (float)o->x_fx / FX_ONE;
            sy = (float)o->y_fx / FX_ONE;
            bool hit_ground = Level_IsSolidAt(level, sx, sy) ||
                              (Level_FloorAt(level, sx, sy, NULL) && sy >=
                               (float)(level->world_h - 24));
            if (o->timer > 240 || hit_ground ||
                sx < level->play_min_x - 16.0f ||
                sx > level->play_max_x + 16.0f ||
                sy < 0.0f || sy > (float)level->world_h) {
                if (o->substate == 1 && hit_ground)
                    SpawnFirewall(pool, sx, sy - 4.0f);
                o->active = false;
                continue;
            }
            if (fabsf(sx - player->x) < 10.0f &&
                fabsf(sy - (player->y - 10.0f)) < 12.0f) {
                DamagePlayer(player, sx, 1);
                if (o->substate == 1)
                    SpawnFirewall(pool, sx, sy);
                o->active = false;
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
                    fabsf(py - oy) <= URN_HALF + 14.0f) {
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
                    o->hit_flash = 8;
                    pool->events |= OBJECT_EVENT_ENEMY_HIT;
                    if (o->hp <= 0) {
                        SpawnPickup(pool, ox, oy + 8.0f);
                        o->active = false;
                    }
                    break;
                }
            } else if (o->type == OBJECT_ENEMY_SHOT) {
                if (fabsf(px - ox) <= 8.0f && fabsf(py - oy) <= 8.0f) {
                    p->active = false;
                    o->active = false;
                    pool->events |= OBJECT_EVENT_ENEMY_HIT;
                    break;
                }
            } else if (o->type == OBJECT_BOSS) {
                if (fabsf(px - ox) <= 20.0f && fabsf(py - oy) <= 24.0f) {
                    p->active = false;
                    o->hp -= dmg;
                    o->hit_flash = 8;
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
