#ifndef DC_LEVEL_H
#define DC_LEVEL_H

#include <stdbool.h>
#include <stdint.h>
#include "raylib.h"
#include "assets.h"

typedef struct BgLayer {
    char name[16];
    int map_w;
    int map_h;
    int scroll_x;
    int scroll_y;
    uint16_t *tiles;
} BgLayer;

typedef struct Collision {
    int cell;
    int width;
    int height;
    uint8_t *attrs;
} Collision;

typedef struct StaticSprite {
    Texture2D texture;
    float x;
    float y;
} StaticSprite;

typedef struct Level {
    char name[64];
    int world_w;
    int world_h;

    BgLayer bg[2];
    int bg_count;

    Collision collision;

    float camera_x;
    float camera_y;
    float camera_min_x;
    float camera_max_x;
    float camera_min_y;
    float camera_max_y;

    float spawn_x;
    float spawn_y;

    float play_min_x;
    float play_max_x;
    float play_min_y;
    float play_max_y;

    Texture2D player_texture;
    float player_feet_x;
    float player_feet_y;

    StaticSprite sprites[16];
    int sprite_count;
} Level;

bool Level_Load(Level *level, const Assets *assets, const char *dir);
void Level_Unload(Level *level);

uint8_t Level_AttrAt(const Level *level, float world_x, float world_y);
bool Level_IsSolidAt(const Level *level, float world_x, float world_y);
bool Level_IsWallAt(const Level *level, float world_x, float world_y);
bool Level_FloorAt(const Level *level, float world_x, float feet_y,
                   float *surface_y);
int Level_SurfaceOffset(uint8_t attr, int local_x);

#endif
