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

#define DC_ENDING_VARIANTS 4
#define DC_ENDING_MAX_LINES 12

typedef struct EndingData {
    char title[DC_ENDING_VARIANTS][16];
    char lines[DC_ENDING_VARIANTS][DC_ENDING_MAX_LINES][52];
    int line_count[DC_ENDING_VARIANTS];
    int epilogue[DC_ENDING_VARIANTS]; /* text_strings index or -1 */
    bool loaded;
} EndingData;

typedef struct SpeciesSheet {
    int species;
    int frame_count;
    int duration;
    SpeciesFrame frames[DC_SPECIES_FRAMES];
} SpeciesSheet;

typedef struct Assets {
    Texture2D bg_pal[DC_BG_PALETTES];
    Texture2D bg_area[DC_BG_PALETTES];
    Texture2D bg2_area[DC_BG_PALETTES];
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
    EndingData endings;
    uint8_t room_kind[DC_CAMPAIGN_AREAS]; /* 0 unknown, 1 playable, 2 partial, 3 empty */
    int stage_entries[80]; /* rooms no other room targets (stage select) */
    int stage_count;
    Texture2D hud_bar;
    Texture2D hud_slots;
    Texture2D font;
    Texture2D world_map; /* 1024x1024 Mode 7 map (optional) */
    /* Map names resolved from the level saves (room_names.json), by area id;
     * empty when unknown. */
    char room_names[113][40];
    char room_levels[113][8]; /* BG3 ASCII font mask (font.png + font.json) */
    PlayerAnim player_anim;
    ProjectileAnim projectile_anim;
    bool ready;
} Assets;

bool Assets_Load(Assets *assets, const char *dir);
void Assets_Unload(Assets *assets);
void Assets_LoadAreaBg(Assets *assets, int area);
Texture2D Assets_AreaBg(const Assets *assets, int area, int pal);
Texture2D Assets_AreaBg2(const Assets *assets, int pal);
bool Assets_HasAreaBg2(const Assets *assets);
const SpeciesSheet *Assets_SpeciesSheet(const Assets *assets, int species);
int Assets_RoomKind(const Assets *assets, int area);

#endif
