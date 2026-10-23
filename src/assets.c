#include "assets.h"

#include <stdio.h>
#include <string.h>

static bool LoadOne(Texture2D *out, const char *dir, const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    *out = LoadTexture(path);
    if (!IsTextureValid(*out)) {
        fprintf(stderr, "assets: failed to load %s\n", path);
        return false;
    }
    SetTextureFilter(*out, TEXTURE_FILTER_POINT);
    return true;
}

bool Assets_Load(Assets *assets, const char *dir) {
    memset(assets, 0, sizeof(*assets));
    for (int i = 0; i < DC_BG_PALETTES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "bg_pal%d.png", i);
        if (!LoadOne(&assets->bg_pal[i], dir, name)) {
            Assets_Unload(assets);
            return false;
        }
    }
    for (int i = 0; i < DC_OBJ_PALETTES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "obj_pal%d.png", i);
        if (!LoadOne(&assets->obj_pal[i], dir, name)) {
            Assets_Unload(assets);
            return false;
        }
    }
    if (!LoadOne(&assets->player, dir, "player_idle.png") ||
        !LoadOne(&assets->item, dir, "item_00.png")) {
        Assets_Unload(assets);
        return false;
    }
    if (!LoadOne(&assets->heart, dir, "heart_00.png")) {
        fprintf(stderr, "assets: heart_00.png missing; using item sprite\n");
        assets->heart = assets->item;
    }
    if (LoadOne(&assets->enemy[0], dir, "enemy_00.png"))
        assets->enemy_frames = 1;
    if (LoadOne(&assets->enemy[1], dir, "enemy_01.png"))
        assets->enemy_frames = 2;
    if (!LoadOne(&assets->hud_bar, dir, "hud_bar.png") ||
        !LoadOne(&assets->hud_slots, dir, "hud_slots.png")) {
        fprintf(stderr, "assets: HUD elements missing; HUD disabled\n");
    }
    if (!Anim_Load(&assets->player_anim, dir)) {
        fprintf(stderr,
                "assets: player_anim.json missing; using the static idle sprite\n");
    }
    if (!ProjectileAnim_Load(&assets->projectile_anim, dir)) {
        fprintf(stderr,
                "assets: fireball_anim.json missing; projectiles render invisible\n");
    }
    assets->ready = true;
    return true;
}

void Assets_Unload(Assets *assets) {
    for (int i = 0; i < DC_BG_PALETTES; i++) {
        if (IsTextureValid(assets->bg_pal[i]))
            UnloadTexture(assets->bg_pal[i]);
    }
    for (int i = 0; i < DC_OBJ_PALETTES; i++) {
        if (IsTextureValid(assets->obj_pal[i]))
            UnloadTexture(assets->obj_pal[i]);
    }
    if (IsTextureValid(assets->player))
        UnloadTexture(assets->player);
    if (IsTextureValid(assets->item))
        UnloadTexture(assets->item);
    if (IsTextureValid(assets->heart) &&
        assets->heart.id != assets->item.id)
        UnloadTexture(assets->heart);
    for (int i = 0; i < assets->enemy_frames; i++) {
        if (IsTextureValid(assets->enemy[i]))
            UnloadTexture(assets->enemy[i]);
    }
    if (IsTextureValid(assets->hud_bar))
        UnloadTexture(assets->hud_bar);
    if (IsTextureValid(assets->hud_slots))
        UnloadTexture(assets->hud_slots);
    Anim_Unload(&assets->player_anim);
    ProjectileAnim_Unload(&assets->projectile_anim);
    memset(assets, 0, sizeof(*assets));
}
