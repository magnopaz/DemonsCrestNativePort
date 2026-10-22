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
    memset(assets, 0, sizeof(*assets));
}
