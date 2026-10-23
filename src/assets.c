#include "assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

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

static int JsonNum(const cJSON *obj, const char *key, int fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static double JsonDouble(const cJSON *obj, const char *key, double fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

/* `species_anim.json` is produced by tools/dc_extract/extract_species.py from
 * injected-object oracle captures: one entry per species with per-pose texture
 * and anchor. Missing file simply means "fall back to the shared enemy sheet". */
static void LoadSpeciesSheets(Assets *assets, const char *dir) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/species_anim.json", dir);
    if (!FileExists(path))
        return;
    char *text = LoadFileText(path);
    if (!text)
        return;
    cJSON *root = cJSON_Parse(text);
    UnloadFileText(text);
    if (!root)
        return;
    const cJSON *species = cJSON_GetObjectItemCaseSensitive(root, "species");
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, species) {
        if (assets->species_count >= DC_SPECIES_MAX)
            break;
        const cJSON *frames = cJSON_GetObjectItemCaseSensitive(entry, "frames");
        if (!cJSON_IsArray(frames))
            continue;
        SpeciesSheet *sheet = &assets->species[assets->species_count];
        memset(sheet, 0, sizeof(*sheet));
        sheet->species = JsonNum(entry, "species", -1);
        sheet->duration = 6;
        const cJSON *frame = NULL;
        cJSON_ArrayForEach(frame, frames) {
            if (sheet->frame_count >= DC_SPECIES_FRAMES)
                break;
            const cJSON *tex =
                cJSON_GetObjectItemCaseSensitive(frame, "texture");
            if (!cJSON_IsString(tex))
                continue;
            SpeciesFrame *slot = &sheet->frames[sheet->frame_count];
            if (!LoadOne(&slot->texture, dir, tex->valuestring))
                continue;
            slot->anchor_x = (float)JsonDouble(frame, "anchor_x", 0.0);
            slot->anchor_y = (float)JsonDouble(frame, "anchor_y", 0.0);
            sheet->duration = JsonNum(frame, "duration", sheet->duration);
            sheet->frame_count++;
        }
        if (sheet->frame_count > 0)
            assets->species_count++;
        else
            memset(sheet, 0, sizeof(*sheet));
    }
    cJSON_Delete(root);
}

/* campaign.json carries the extractor's per-room classification. Empty rooms
 * (menus/padding grids, currently none) must never be entered as gameplay. */
static void LoadRoomKinds(Assets *assets, const char *dir) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/campaign.json", dir);
    if (!FileExists(path))
        return;
    char *text = LoadFileText(path);
    if (!text)
        return;
    cJSON *root = cJSON_Parse(text);
    UnloadFileText(text);
    if (!root)
        return;
    const cJSON *entries = cJSON_GetObjectItemCaseSensitive(root, "stage_entries");
    if (cJSON_IsArray(entries)) {
        const cJSON *it = NULL;
        cJSON_ArrayForEach(it, entries) {
            if (cJSON_IsNumber(it) && assets->stage_count <
                                          (int)(sizeof(assets->stage_entries) /
                                                sizeof(assets->stage_entries[0])))
                assets->stage_entries[assets->stage_count++] = it->valueint;
        }
    }
    const cJSON *rooms = cJSON_GetObjectItemCaseSensitive(root, "rooms");
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, rooms) {
        if (!cJSON_IsObject(entry))
            continue;
        int area = atoi(entry->string ? entry->string : "-1");
        if (area < 0 || area >= DC_CAMPAIGN_AREAS)
            continue;
        const cJSON *kind = cJSON_GetObjectItemCaseSensitive(entry, "kind");
        if (!cJSON_IsString(kind))
            continue;
        if (strcmp(kind->valuestring, "playable") == 0)
            assets->room_kind[area] = 1;
        else if (strcmp(kind->valuestring, "partial") == 0)
            assets->room_kind[area] = 2;
        else if (strcmp(kind->valuestring, "empty") == 0)
            assets->room_kind[area] = 3;
    }
    cJSON_Delete(root);
}

int Assets_RoomKind(const Assets *assets, int area) {
    if (area < 0 || area >= DC_CAMPAIGN_AREAS)
        return 0;
    return assets->room_kind[area];
}

/* endings.json: the four campaign endings (narration transcribed from the
 * reference ending screenshots; epilogue indexes the ROM text script). */
static void LoadEndings(Assets *assets, const char *dir) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/endings.json", dir);
    if (!FileExists(path))
        return;
    char *text = LoadFileText(path);
    if (!text)
        return;
    cJSON *root = cJSON_Parse(text);
    UnloadFileText(text);
    if (!root)
        return;
    static const char *kIds[DC_ENDING_VARIANTS] = {"bad", "normal", "good",
                                                  "true"};
    const cJSON *endings = cJSON_GetObjectItemCaseSensitive(root, "endings");
    for (int i = 0; i < DC_ENDING_VARIANTS; i++) {
        const cJSON *e = cJSON_GetObjectItemCaseSensitive(endings, kIds[i]);
        if (!cJSON_IsObject(e))
            continue;
        const cJSON *title = cJSON_GetObjectItemCaseSensitive(e, "title");
        if (cJSON_IsString(title))
            snprintf(assets->endings.title[i], sizeof(assets->endings.title[i]),
                     "%s", title->valuestring);
        const cJSON *lines = cJSON_GetObjectItemCaseSensitive(e, "lines");
        int n = 0;
        const cJSON *line = NULL;
        cJSON_ArrayForEach(line, lines) {
            if (!cJSON_IsString(line) || n >= DC_ENDING_MAX_LINES)
                break;
            snprintf(assets->endings.lines[i][n],
                     sizeof(assets->endings.lines[i][n]), "%s",
                     line->valuestring);
            n++;
        }
        assets->endings.line_count[i] = n;
        const cJSON *epi = cJSON_GetObjectItemCaseSensitive(e, "epilogue");
        assets->endings.epilogue[i] = cJSON_IsNumber(epi) ? epi->valueint : -1;
    }
    assets->endings.loaded = true;
    cJSON_Delete(root);
}

const SpeciesSheet *Assets_SpeciesSheet(const Assets *assets, int species) {
    for (int i = 0; i < assets->species_count; i++) {
        if (assets->species[i].species == species)
            return &assets->species[i];
    }
    return NULL;
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
    snprintf(assets->asset_dir, sizeof(assets->asset_dir), "%s", dir);
    assets->bg_area_id = -1;
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
    Image player_image = LoadImageFromTexture(assets->player);
    ImageFlipHorizontal(&player_image);
    assets->player_flipped = LoadTextureFromImage(player_image);
    UnloadImage(player_image);
    if (!IsTextureValid(assets->player_flipped)) {
        Assets_Unload(assets);
        return false;
    }
    SetTextureFilter(assets->player_flipped, TEXTURE_FILTER_POINT);
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
    if (!LoadOne(&assets->font, dir, "font.png")) {
        fprintf(stderr, "assets: font.png missing; dialogue text disabled\n");
        memset(&assets->font, 0, sizeof(assets->font));
    }
    if (!Anim_Load(&assets->player_anim, dir)) {
        fprintf(stderr,
                "assets: player_anim.json missing; using the static idle sprite\n");
    }
    if (!ProjectileAnim_Load(&assets->projectile_anim, dir)) {
        fprintf(stderr,
                "assets: fireball_anim.json missing; projectiles render invisible\n");
    }
    LoadSpeciesSheets(assets, dir);
    LoadRoomKinds(assets, dir);
    LoadEndings(assets, dir);
    if (assets->species_count > 0)
        printf("assets: %d per-species sprite sheet(s)\n", assets->species_count);
    assets->ready = true;
    return true;
}

void Assets_Unload(Assets *assets) {
    for (int i = 0; i < DC_BG_PALETTES; i++) {
        if (IsTextureValid(assets->bg_pal[i]))
            UnloadTexture(assets->bg_pal[i]);
    }
    if (assets->bg_area_ready) {
        for (int p = 0; p < DC_BG_PALETTES; p++) {
            if (IsTextureValid(assets->bg_area[p]))
                UnloadTexture(assets->bg_area[p]);
        }
    }
    for (int i = 0; i < DC_OBJ_PALETTES; i++) {
        if (IsTextureValid(assets->obj_pal[i]))
            UnloadTexture(assets->obj_pal[i]);
    }
    if (IsTextureValid(assets->player))
        UnloadTexture(assets->player);
    if (IsTextureValid(assets->player_flipped))
        UnloadTexture(assets->player_flipped);
    if (IsTextureValid(assets->item))
        UnloadTexture(assets->item);
    if (IsTextureValid(assets->heart) &&
        assets->heart.id != assets->item.id)
        UnloadTexture(assets->heart);
    for (int i = 0; i < assets->enemy_frames; i++) {
        if (IsTextureValid(assets->enemy[i]))
            UnloadTexture(assets->enemy[i]);
    }
    for (int i = 0; i < assets->species_count; i++) {
        for (int f = 0; f < assets->species[i].frame_count; f++) {
            if (IsTextureValid(assets->species[i].frames[f].texture))
                UnloadTexture(assets->species[i].frames[f].texture);
        }
    }
    if (IsTextureValid(assets->hud_bar))
        UnloadTexture(assets->hud_bar);
    if (IsTextureValid(assets->hud_slots))
        UnloadTexture(assets->hud_slots);
    if (IsTextureValid(assets->font))
        UnloadTexture(assets->font);
    Anim_Unload(&assets->player_anim);
    ProjectileAnim_Unload(&assets->projectile_anim);
    memset(assets, 0, sizeof(*assets));
}

bool Assets_HasAreaBg2(const Assets *assets) {
    for (int p = 0; p < DC_BG_PALETTES; p++)
        if (IsTextureValid(assets->bg2_area[p]))
            return true;
    return false;
}

Texture2D Assets_AreaBg2(const Assets *assets, int pal) {
    if (pal < 0 || pal >= DC_BG_PALETTES)
        return (Texture2D){0};
    return assets->bg2_area[pal];
}

Texture2D Assets_AreaBg(const Assets *assets, int area, int pal) {
    (void)area;
    if (pal < 0 || pal >= DC_BG_PALETTES)
        return assets->bg_pal[0];
    if (IsTextureValid(assets->bg_area[pal]))
        return assets->bg_area[pal];
    return assets->bg_pal[pal];
}

void Assets_LoadAreaBg(Assets *assets, int area) {
    if (area == assets->bg_area_id)
        return;
    if (assets->bg_area_ready) {
        for (int p = 0; p < DC_BG_PALETTES; p++) {
            if (IsTextureValid(assets->bg_area[p]))
                UnloadTexture(assets->bg_area[p]);
            memset(&assets->bg_area[p], 0, sizeof(assets->bg_area[p]));
            if (IsTextureValid(assets->bg2_area[p]))
                UnloadTexture(assets->bg2_area[p]);
            memset(&assets->bg2_area[p], 0, sizeof(assets->bg2_area[p]));
        }
        assets->bg_area_ready = false;
    }
    assets->bg_area_id = area;
    bool any = false;
    for (int p = 0; p < DC_BG_PALETTES; p++) {
        char name[48];
        snprintf(name, sizeof(name), "bg_area%d_pal%d.png", area, p);
        char path[1200];
        snprintf(path, sizeof(path), "%s/%s", assets->asset_dir, name);
        if (!FileExists(path))
            continue;
        Texture2D tex = LoadTexture(path);
        if (!IsTextureValid(tex))
            continue;
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
        assets->bg_area[p] = tex;
        any = true;
    }
    /* BG2 atlases are baked from the captured frame's streamed CHR and live at
     * a different base than the ROM BG1 set, so they are loaded separately. */
    for (int p = 0; p < DC_BG_PALETTES; p++) {
        char name[48];
        snprintf(name, sizeof(name), "bg2_area%d_pal%d.png", area, p);
        char path[1200];
        snprintf(path, sizeof(path), "%s/%s", assets->asset_dir, name);
        if (!FileExists(path))
            continue;
        Texture2D tex = LoadTexture(path);
        if (!IsTextureValid(tex))
            continue;
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
        assets->bg2_area[p] = tex;
        any = true;
    }
    assets->bg_area_ready = any;
    if (!any)
        assets->bg_area_id = -1;
}
