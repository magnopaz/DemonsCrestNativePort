#ifndef DC_OBJECT_H
#define DC_OBJECT_H

#include <stdbool.h>
#include <stdint.h>

#include "level.h"
#include "player.h"
#include "projectile.h"

#define OBJECT_POOL_SIZE 26

#define OBJECT_EVENT_PICKUP 0x01
#define OBJECT_EVENT_BREAK 0x02
#define OBJECT_EVENT_ENEMY_HIT 0x04

typedef enum ObjectType {
    OBJECT_NONE = 0,
    OBJECT_URN,
    OBJECT_PICKUP,
    OBJECT_ENEMY
} ObjectType;

typedef struct Object {
    bool active;
    ObjectType type;
    int species;
    int32_t x_fx, y_fx;
    int32_t vx_fx, vy_fx;
    int32_t bounce_fx; /* $39: remaining bounce velocity for pickups */
    int state;
    int hp;
    int timer;
} Object;

typedef struct ObjectPool {
    Object slots[OBJECT_POOL_SIZE];
    int events; /* one-frame sound/effect flags */
} ObjectPool;

void Objects_Init(ObjectPool *pool, const Level *level);
void Objects_Update(ObjectPool *pool, const Level *level, Player *player);
void Objects_CheckProjectiles(ObjectPool *pool, ProjectilePool *projectiles);

#endif
