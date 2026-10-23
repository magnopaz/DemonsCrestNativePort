#ifndef DC_ASSETS_H
#define DC_ASSETS_H

#include <stdbool.h>
#include <stdint.h>
#include "anim.h"
#include "raylib.h"

#define DC_BG_PALETTES 8
#define DC_OBJ_PALETTES 8
#define DC_CAMPAIGN_AREAS 128
#define DC_SPECIES_MAX 16
#define DC_SPECIES_FRAMES 4

typedef struct SpeciesFrame {
    Texture2D texture;
    float anchor_x;
    float anchor_y;
} SpeciesFrame;

typedef struct SpeciesSheet {
    int species;
    int frame_count;
    int duration;
    SpeciesFrame frames[DC_SPECIES_FRAMES];
} SpeciesSheet;

typedef struct Assets {
    Texture2D bg_pal[DC_BG_PALETTES];
    Texture2D bg_area[DC_BG_PALETTES];
    bool bg_area_ready;
    int bg_area_id;
    char asset_dir[1024];
    Texture2D obj_pal[DC_OBJ_PALETTES];
    Texture2D player;
    Texture2D player_flipped;
    Texture2D item;
    Texture2D heart;
    Texture2D enemy[2];
    int enemy_frames;
    SpeciesSheet species[DC_SPECIES_MAX];
    int species_count;
    uint8_t room_kind[DC_CAMPAIGN_AREAS]; /* 0 unknown, 1 playable, 2 partial, 3 empty */
    Texture2D hud_bar;
    Texture2D hud_slots;
    Texture2D font; /* BG3 ASCII font mask (font.png + font.json) */
    PlayerAnim player_anim;
    ProjectileAnim projectile_anim;
    bool ready;
} Assets;

bool Assets_Load(Assets *assets, const char *dir);
void Assets_Unload(Assets *assets);
void Assets_LoadAreaBg(Assets *assets, int area);
Texture2D Assets_AreaBg(const Assets *assets, int area, int pal);
const SpeciesSheet *Assets_SpeciesSheet(const Assets *assets, int species);
int Assets_RoomKind(const Assets *assets, int area);

#endif
