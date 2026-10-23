#include "render.h"

#include <math.h>
#include <stdlib.h>

#include "raylib.h"

static void DrawLayerBand(const Assets *assets, const BgLayer *layer,
                          int area_id, bool wrap_x, float origin_x,
                          float origin_y, int y_min, int y_max) {
    int tx0 = (int)floorf(origin_x / 8.0f);
    int ty0 = (int)floorf((origin_y + (float)y_min) / 8.0f);
    int tx1 = (int)floorf((origin_x + 256.0f) / 8.0f) + 1;
    int ty1 = (int)floorf((origin_y + (float)y_max) / 8.0f) + 1;

    for (int ty = ty0; ty <= ty1; ty++) {
        if (ty < 0 || ty >= layer->map_h)
            continue;
        float dst_y = (float)ty * 8.0f - origin_y;
        if (dst_y + 8.0f <= (float)y_min || dst_y >= (float)y_max)
            continue;
        for (int tx = tx0; tx <= tx1; tx++) {
            int map_tx = tx;
            if (wrap_x) {
                map_tx %= layer->map_w;
                if (map_tx < 0)
                    map_tx += layer->map_w;
            } else if (tx < 0 || tx >= layer->map_w) {
                continue;
            }
            uint16_t word = layer->tiles[ty * layer->map_w + map_tx];
            int tile = word & 0x3FF;
            int pal = (word >> 10) & 7;
            if (pal >= DC_BG_PALETTES)
                pal = 0;
            float qx = (word & 0x4000) ? 256.0f : 0.0f;
            float qy = (word & 0x8000) ? 256.0f : 0.0f;
            Rectangle src = {
                qx + (float)((tile % 32) * 8),
                qy + (float)((tile / 32) * 8),
                8.0f, 8.0f
            };
            Rectangle dst = {
                (float)tx * 8.0f - origin_x,
                dst_y,
                8.0f, 8.0f
            };
            if (dst.y < (float)y_min) {
                float clip = (float)y_min - dst.y;
                src.y += clip;
                src.height -= clip;
                dst.y = (float)y_min;
                dst.height -= clip;
            }
            if (dst.y + dst.height > (float)y_max) {
                float clip = (dst.y + dst.height) - (float)y_max;
                src.height -= clip;
                dst.height -= clip;
            }
            if (dst.height > 0.0f) {
                DrawTexturePro(Assets_AreaBg(assets, area_id, pal), src, dst,
                               (Vector2){0, 0}, 0.0f, WHITE);
            }
        }
    }
}

static void DrawLayer(const Assets *assets, const BgLayer *layer,
                      const Level *level, const GameCamera *camera) {
    /* Each layer keeps the relative scroll it had in the captured frame
     * (`layer->scroll_x - level->camera_x`); the runtime camera then scrolls
     * all layers together. */
    float offset_x = (float)layer->scroll_x - level->camera_x;
    float offset_y = (float)layer->scroll_y - level->camera_y;
    bool wrap_x = layer->map_w < level->bg[1].map_w ||
                  layer == &level->bg[0];
    float origin_y = camera->y + offset_y;
    if (layer == &level->bg[0] && level->world_h > 256)
        origin_y = fmodf(camera->y * 0.5f, (float)(layer->map_h * 8 - 224));

    if (layer->hdma_band_count > 0 && getenv("DC_NO_HDMA") == NULL) {
        int scan_y = 0;
        float cam_dx = camera->x - level->camera_x;
        for (int b = 0; b < layer->hdma_band_count && scan_y < 224; b++) {
            int next_y = scan_y + layer->hdma_bands[b].lines;
            if (next_y > 224)
                next_y = 224;
            float band_x = (float)layer->scroll_x +
                           cam_dx * layer->hdma_bands[b].factor;
            DrawLayerBand(assets, layer, level->area_id, wrap_x, band_x,
                          origin_y, scan_y, next_y);
            scan_y = next_y;
        }
        return;
    }

    float origin_x = camera->x + offset_x;
    DrawLayerBand(assets, layer, level->area_id, wrap_x, origin_x, origin_y,
                  0, 224);
}

static Color CrestTint(int crest_form, int invuln_timer) {
    if (invuln_timer > 0 && ((invuln_timer / 2) & 1))
        return (Color){255, 160, 160, 180};
    switch (crest_form) {
    case CREST_EARTH:
        return (Color){235, 195, 115, 255};
    case CREST_AIR:
        return (Color){165, 225, 255, 255};
    case CREST_WATER:
        return (Color){135, 240, 220, 255};
    case CREST_TIME:
        return (Color){255, 235, 140, 255};
    case CREST_ULTIMATE:
        return (Color){245, 175, 255, 255};
    default:
        return WHITE;
    }
}

static void DrawPlayer(const Assets *assets, const Level *level,
                       const GameCamera *camera, const Player *player) {
    const AnimStep *step = NULL;
    if (assets->player_anim.ready && player->anim_clip >= 0)
        step = Anim_Step(&assets->player_anim, player->anim_clip,
                         player->anim_step);

    bool left = player->facing < 0;
    Texture2D texture;
    float anchor_x, anchor_y;
    if (step) {
        texture = left ? step->flipped : step->texture;
        anchor_x = (float)step->anchor_x;
        anchor_y = (float)step->anchor_y;
    } else {
        texture = left ? assets->player_flipped : assets->player;
        anchor_x = level->player_feet_x;
        anchor_y = level->player_feet_y;
    }

    float x = player->x - anchor_x - camera->x;
    float y = player->y - anchor_y - camera->y;
    Rectangle src = {0, 0, (float)texture.width, (float)texture.height};
    Rectangle dst = {x, y, (float)texture.width, (float)texture.height};
    Color tint = CrestTint(player->crest_form, player->invuln_timer);
    DrawTexturePro(texture, src, dst, (Vector2){0, 0}, 0.0f, tint);
}

static void DrawProjectiles(const Assets *assets, const GameCamera *camera,
                            const ProjectilePool *projectiles) {
    if (!assets->projectile_anim.ready)
        return;

    for (int i = 0; i < PROJECTILE_POOL_SIZE; i++) {
        const Projectile *p = &projectiles->slots[i];
        if (!p->active || p->age <= 0)
            continue; /* the spawn frame shows no projectile sprite */
        const AnimStep *step = ProjectileAnim_Step(&assets->projectile_anim,
                                                   p->age - 1);
        if (!step)
            continue;

        float x = (float)p->x_fx / 256.0f - (float)step->anchor_x - camera->x;
        float y = (float)p->y_fx / 256.0f - (float)step->anchor_y - camera->y;
        Texture2D texture = p->vx_fx < 0 ? step->flipped : step->texture;
        Rectangle src = {0, 0, (float)texture.width, (float)texture.height};
        Rectangle dst = {x, y, (float)texture.width, (float)texture.height};
        Color tint = WHITE;
        switch (p->element) {
        case 1: tint = (Color){255, 220, 120, 255}; break; /* Buster */
        case 2: tint = (Color){140, 240, 150, 255}; break; /* Tornado */
        case 3: tint = (Color){140, 220, 255, 255}; break; /* Claw */
        case 4: tint = (Color){235, 130, 255, 255}; break; /* Demon Fire */
        default: break;
        }
        DrawTexturePro(texture, src, dst, (Vector2){0, 0}, 0.0f, tint);
    }
}

static void DrawObjects(const Assets *assets, const GameCamera *camera,
                        const ObjectPool *objects) {
    for (int i = 0; i < OBJECT_POOL_SIZE; i++) {
        const Object *o = &objects->slots[i];
        if (!o->active)
            continue;
        float x = (float)o->x_fx / 256.0f - camera->x;
        float y = (float)o->y_fx / 256.0f - camera->y;

        if (o->type == OBJECT_ENEMY_SHOT) {
            const SpeciesSheet *sheet = Assets_SpeciesSheet(assets, o->species);
            if (sheet && sheet->frame_count > 0) {
                const SpeciesFrame *sf = &sheet->frames[0];
                float dx = x - sf->anchor_x;
                float dy = y - sf->anchor_y;
                Rectangle src = {0, 0, (float)sf->texture.width,
                                 (float)sf->texture.height};
                Rectangle dst = {dx, dy, (float)sf->texture.width,
                                 (float)sf->texture.height};
                DrawTexturePro(sf->texture, src, dst, (Vector2){0, 0}, 0.0f,
                               (Color){255, 220, 255, 255});
            } else {
                DrawCircleV((Vector2){x, y}, 5.0f, (Color){240, 160, 220, 255});
                DrawCircleV((Vector2){x, y}, 2.0f, WHITE);
            }
        } else if (o->type == OBJECT_URN) {
            Texture2D tex = assets->item;
            DrawTexture(tex, (int)lroundf(x - tex.width * 0.5f),
                        (int)lroundf(y - tex.height * 0.5f), WHITE);
        } else if (o->type == OBJECT_PICKUP) {
            Texture2D tex = assets->heart;
            DrawTexture(tex, (int)lroundf(x - tex.width * 0.5f),
                        (int)lroundf(y - tex.height), WHITE);
        } else if (o->type == OBJECT_CREST_ITEM) {
            Texture2D tex = assets->heart;
            Color tint = ((o->timer / 6) & 1)
                         ? (Color){255, 240, 120, 255}
                         : (Color){180, 230, 255, 255};
            DrawTexture(tex, (int)lroundf(x - tex.width * 0.5f),
                        (int)lroundf(y - tex.height), tint);
        } else if (o->type == OBJECT_PLATFORM) {
            int px = (int)lroundf(x) - 16;
            int py = (int)lroundf(y);
            DrawRectangle(px, py, 32, 6, (Color){128, 120, 108, 255});
            DrawRectangleLines(px, py, 32, 6, (Color){200, 192, 168, 255});
        } else if ((o->type == OBJECT_ENEMY || o->type == OBJECT_BOSS)) {
            const SpeciesSheet *sheet = Assets_SpeciesSheet(assets, o->species);
            if (sheet && sheet->frame_count > 0) {
                int frame = (o->timer / sheet->duration) % sheet->frame_count;
                const SpeciesFrame *sf = &sheet->frames[frame];
                float dx = x - sf->anchor_x;
                float dy = y - sf->anchor_y;
                float scale = (o->type == OBJECT_BOSS) ? 1.5f : 1.0f;
                if (o->type == OBJECT_BOSS) {
                    /* Boss anchors are measured at 1x; scale around the anchor. */
                    dx = x - sf->anchor_x * scale;
                    dy = y - sf->anchor_y * scale;
                }
                Rectangle src = {0, 0, (float)sf->texture.width,
                                 (float)sf->texture.height};
                Rectangle dst = {dx, dy,
                                 (float)sf->texture.width * scale,
                                 (float)sf->texture.height * scale};
                DrawTexturePro(sf->texture, src, dst, (Vector2){0, 0}, 0.0f,
                               WHITE);
            } else if (assets->enemy_frames > 0) {
                int frame = (o->timer / 8) % assets->enemy_frames;
                Texture2D tex = assets->enemy[frame];
                Color tint = WHITE;
                if (o->species == 0x2C)
                    tint = (Color){210, 230, 255, 255};
                else if (o->species == 0x07)
                    tint = (Color){255, 180, 200, 255};
                float scale = (o->type == OBJECT_BOSS) ? 1.5f : 1.0f;
                Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
                Rectangle dst = {x - (float)tex.width * 0.5f * scale,
                                 y - (float)tex.height * 0.5f * scale,
                                 (float)tex.width * scale,
                                 (float)tex.height * scale};
                DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, tint);
            }
            if (o->type == OBJECT_BOSS && o->max_hp > 0 && o->hp > 0) {
                int bw = 48;
                int filled = (o->hp * bw) / o->max_hp;
                DrawRectangle(196, 26, bw + 2, 6, (Color){20, 10, 20, 200});
                DrawRectangle(197, 27, filled, 4, (Color){230, 60, 80, 255});
            }
        }
    }
}

static void DrawHud(const Assets *assets, const Player *player) {
    DrawRectangle(0, 0, 256, 24, (Color){0, 0, 0, 150});

    if (IsTextureValid(assets->hud_bar)) {
        int bar_x = 24;
        int bar_y = 16;
        int width = assets->hud_bar.width;
        /* The baked bar is a brightness mask (light body + pink highlight),
         * so tint it: dark for missing health, red for the filled part. */
        DrawTexture(assets->hud_bar, bar_x, bar_y,
                    (Color){52, 48, 58, 255});
        if (player->max_hp > 0 && player->hp > 0) {
            float ratio = (float)player->hp / (float)player->max_hp;
            if (ratio > 1.0f)
                ratio = 1.0f;
            int filled = (int)lroundf(ratio * width);
            if (filled > 0) {
                Rectangle src = {0, 0, (float)filled,
                                 (float)assets->hud_bar.height};
                Rectangle dst = {(float)bar_x, (float)bar_y,
                                 (float)filled,
                                 (float)assets->hud_bar.height};
                DrawTexturePro(assets->hud_bar, src, dst, (Vector2){0, 0},
                               0.0f, (Color){235, 88, 120, 255});
            }
        }
    }

    if (IsTextureValid(assets->hud_slots)) {
        DrawTexture(assets->hud_slots, 144, 8, WHITE);
        /* Highlight the active Crest/Talisman slot (3 visible 32x16 slots). */
        int slot_idx = player->crest_form % 3;
        Color slot_col = CrestTint(player->crest_form, 0);
        slot_col.a = 235;
        DrawRectangleLines(144 + slot_idx * 32, 8, 32, 16, slot_col);
    }
}

void Render_Game(const Assets *assets, const Level *level, const GameCamera *camera,
                 const Player *player, const ProjectilePool *projectiles,
                 const ObjectPool *objects, bool show_hud,
                 bool debug_collision) {
    static int layer_mask = -1;
    if (layer_mask < 0) {
        const char *env = getenv("DC_LAYER_MASK");
        layer_mask = env ? atoi(env) : 0x3;
    }
    ClearBackground(level->backdrop);

    for (int i = 0; i < level->bg_count; i++) {
        if (!(layer_mask & (1 << i)))
            continue;
        DrawLayer(assets, &level->bg[i], level, camera);
    }

    for (int i = 0; i < level->sprite_count; i++) {
        const StaticSprite *s = &level->sprites[i];
        DrawTexture(s->texture, (int)(s->x - camera->x),
                    (int)(s->y - camera->y), WHITE);
    }

    DrawPlayer(assets, level, camera, player);
    DrawProjectiles(assets, camera, projectiles);
    DrawObjects(assets, camera, objects);
    if (show_hud)
        DrawHud(assets, player);

    if (debug_collision) {
        const Collision *c = &level->collision;
        int cx0 = (int)floorf(camera->x / (float)c->cell);
        int cy0 = (int)floorf(camera->y / (float)c->cell);
        int cx1 = (int)floorf((camera->x + 256.0f) / (float)c->cell) + 1;
        int cy1 = (int)floorf((camera->y + 224.0f) / (float)c->cell) + 1;
        for (int cy = cy0; cy <= cy1; cy++) {
            for (int cx = cx0; cx <= cx1; cx++) {
                if (cx < 0 || cy < 0 || cx >= c->width || cy >= c->height)
                    continue;
                if (!(c->attrs[cy * c->width + cx] & 0x1F))
                    continue;
                DrawRectangleLines(
                    (int)(cx * c->cell - camera->x),
                    (int)(cy * c->cell - camera->y),
                    c->cell, c->cell, (Color){40, 220, 80, 110});
            }
        }
        DrawRectangleLines((int)(player->x - 6 - camera->x),
                           (int)(player->y - 22 - camera->y),
                           12, 22, (Color){255, 220, 40, 220});
        DrawRectangle(0, 0, 256, 12, (Color){0, 0, 0, 170});
        DrawText(TextFormat("x=%.0f y=%.0f hp=%d/%d  F1 collision",
                            player->x, player->y, player->hp, player->max_hp),
                 3, 1, 10, (Color){220, 220, 220, 255});
    }
}
