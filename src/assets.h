#ifndef DC_ASSETS_H
#define DC_ASSETS_H

#include <stdbool.h>
#include <stdint.h>
#include "anim.h"
#include "raylib.h"

#define DC_BG_PALETTES 8
#define DC_OBJ_PALETTES 8

typedef struct Assets {
    Texture2D bg_pal[DC_BG_PALETTES];
    Texture2D obj_pal[DC_OBJ_PALETTES];
    Texture2D player;
    Texture2D item;
    Texture2D heart;
    Texture2D enemy[2];
    int enemy_frames;
    Texture2D hud_bar;
    Texture2D hud_slots;
    PlayerAnim player_anim;
    ProjectileAnim projectile_anim;
    bool ready;
} Assets;

bool Assets_Load(Assets *assets, const char *dir);
void Assets_Unload(Assets *assets);

#endif
