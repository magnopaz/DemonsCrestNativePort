#include "render.h"

#include <math.h>
#include <stdlib.h>

#include "raylib.h"

static void DrawLayerBand(const Assets *assets, const BgLayer *layer,
                          int area_id, bool wrap_x, bool wrap_y, bool bg2,
                          float origin_x, float origin_y, int y_min,
                          int y_max) {
    int tx0 = (int)floorf(origin_x / 8.0f);
    int ty0 = (int)floorf((origin_y + (float)y_min) / 8.0f);
    int tx1 = (int)floorf((origin_x + 256.0f) / 8.0f) + 1;
    int ty1 = (int)floorf((origin_y + (float)y_max) / 8.0f) + 1;

    for (int ty = ty0; ty <= ty1; ty++) {
        int map_ty = ty;
        if (wrap_y && layer->map_h > 0) {
            map_ty %= layer->map_h;
            if (map_ty < 0)
                map_ty += layer->map_h;
        } else if (ty < 0 || ty >= layer->map_h) {
            continue;
        }
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
            uint16_t word = layer->tiles[map_ty * layer->map_w + map_tx];
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
                Texture2D tex = (bg2 && Assets_HasAreaBg2(assets))
                                    ? Assets_AreaBg2(assets, pal)
                                    : Assets_AreaBg(assets, area_id, pal);
                if (IsTextureValid(tex)) {
                    DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f,
                                   WHITE);
                }
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
    /* A captured backdrop is one screen of a much larger room: tile it in both
     * axes rather than leaving black gaps where the capture ends. */
    bool wrap_y = layer == &level->bg[0] && level->world_h > 256;
    float origin_y = camera->y + offset_y;
    if (wrap_y && layer->map_h * 8 > 224)
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
            DrawLayerBand(assets, layer, level->area_id, wrap_x, wrap_y,
                          layer == &level->bg[0], band_x, origin_y, scan_y,
                          next_y);
            scan_y = next_y;
        }
        return;
    }

    float origin_x = camera->x + offset_x;
    DrawLayerBand(assets, layer, level->area_id, wrap_x, wrap_y,
                  layer == &level->bg[0], origin_x, origin_y, 0, 224);
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
    const PlayerAnim *anim =
        Assets_PlayerAnimForForm(assets, player->crest_form);
    if (anim->ready && player->anim_clip >= 0)
        step = Anim_Step(anim, player->anim_clip, player->anim_step);

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
            /* Somulo's fireballs and its firewall flames reuse the player's
             * fireball art, tinted: $34 is the boss projectile, state 1 marks a
             * standing flame. */
            if (o->species == 0x34 &&
                (o->state == 1 ? assets->boss_flame_count
                               : assets->boss_fireball_count) > 0) {
                Texture2D fire;
                if (o->state == 1) {
                    int idx = (o->timer / 4) % 7;
                    if (idx >= assets->boss_flame_count)
                        idx = assets->boss_flame_count - 1;
                    fire = assets->boss_flame[idx];
                    /* Flames stand on the ground: anchor at their bottom. */
                    Rectangle src = {0, 0, (float)fire.width,
                                     (float)fire.height};
                    Rectangle dst = {x - fire.width * 0.5f,
                                     y - (float)fire.height,
                                     (float)fire.width, (float)fire.height};
                    DrawTexturePro(fire, src, dst, (Vector2){0, 0}, 0.0f,
                                   WHITE);
                } else {
                    int idx = (o->timer / 4) % assets->boss_fireball_count;
                    fire = assets->boss_fireball[idx];
                    Rectangle src = {0, 0, (float)fire.width,
                                     (float)fire.height};
                    Rectangle dst = {x - fire.width * 0.5f,
                                     y - fire.height * 0.5f,
                                     (float)fire.width, (float)fire.height};
                    DrawTexturePro(fire, src, dst, (Vector2){0, 0}, 0.0f,
                                   WHITE);
                }
            } else {
                const SpeciesSheet *sheet =
                    Assets_SpeciesSheet(assets, o->species);
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
                    DrawCircleV((Vector2){x, y}, 5.0f,
                                (Color){240, 160, 220, 255});
                    DrawCircleV((Vector2){x, y}, 2.0f, WHITE);
                }
            }
        } else if (o->type == OBJECT_URN) {
            Texture2D tex = assets->item;
            DrawTexture(tex, (int)lroundf(x - tex.width * 0.5f),
                        (int)lroundf(y - tex.height * 0.5f), WHITE);
        } else if (o->type == OBJECT_PICKUP) {
            Texture2D tex = assets->heart;
            DrawTexture(tex, (int)lroundf(x - tex.width * 0.5f),
                        (int)lroundf(y - tex.height), WHITE);
        } else if (o->type == OBJECT_TALISMAN) {
            Texture2D tex = assets->item;
            Color tint = WHITE;
            switch (o->species) {
            case 0: tint = (Color){255, 230, 120, 255}; break; /* crown */
            case 1: tint = (Color){210, 210, 230, 255}; break; /* skull */
            case 2: tint = (Color){170, 200, 255, 255}; break; /* armor */
            case 3: tint = (Color){255, 200, 160, 255}; break; /* fang */
            default: tint = (Color){190, 255, 190, 255}; break; /* hand */
            }
            DrawTexture(tex, (int)lroundf(x - tex.width * 0.5f),
                        (int)lroundf(y - tex.height), tint);
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
        } else if ((o->type == OBJECT_ENEMY || o->type == OBJECT_BOSS ||
                    o->type == OBJECT_NPC)) {
            const SpeciesSheet *sheet = Assets_SpeciesSheet(assets, o->species);
            if (sheet && sheet->frame_count > 0) {
                int frame = (o->timer / sheet->duration) % sheet->frame_count;
                if (o->species == 0xA5 && sheet->frame_count >= 4) {
                    if (o->hit_flash > 0)
                        frame = 3;
                    else if (o->state == 2 || o->state == 3)
                        frame = 2;
                    else if (o->state == 1)
                        frame = 1;
                    else
                        frame = 0;
                }
                if (o->species == 0x09 && sheet->frame_count >= 6 &&
                    o->state == 6) {
                    frame = 5;   /* statue */
                }
                if (o->species == 0x09 && sheet->frame_count >= 5 &&
                    o->state != 6) {
                    if (o->state == 1)
                        frame = 4;   /* feather volley */
                    else if (o->state == 2 || o->state == 3)
                        frame = 3;   /* swoop */
                    else
                        frame = (o->timer / 8) % 3;  /* flap cycle */
                }
                if (sheet->frame_count >= 6 &&
                    (o->species == 0x13 || o->species == 0x86)) {
                    /* Sheets: first row is idle, second row is the attack /
                     * movement set. */
                    if (o->hit_flash > 0)
                        frame = 5;
                    else if (o->species == 0x13 && o->state == 2)
                        frame = 3;
                    else if (o->species == 0x09 && (o->state == 2 ||
                                                    o->state == 3))
                        frame = 3;
                    else if (o->species == 0x86 && o->state == 1)
                        frame = 3;
                    else
                        frame = 0;
                }
                if (o->species == 0x33 && sheet->frame_count >= 5) {
                    /* Poses: 0 idle, 1 attack high, 2 attack low, 3 hurt,
                     * 4 defeated. */
                    if (o->state == 5)
                        frame = 4;   /* collapsed */
                    else if (o->hit_flash > 0)
                        frame = 3;
                    else if (o->state == 1 || o->state == 2)
                        frame = (o->substate == 1) ? 2 : 1;
                    else
                        frame = 0;
                }
                const SpeciesFrame *sf = &sheet->frames[frame];
                float dx = x - sf->anchor_x;
                float dy = y - sf->anchor_y;
                float scale = 1.0f;
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
                Color tint = o->hit_flash > 0 ? (Color){255, 200, 200, 255}
                                              : WHITE;
                if (o->hit_flash > 4)
                    tint = (Color){255, 255, 255, 255};
                if (o->species == 0x13 && o->hit_flash > 0) {
                    /* Grewon's tell: he turns green while invincible, so the
                     * player can read when he is safe to hit again. */
                    tint = (Color){120, 255, 140, 255};
                } else if (o->type == OBJECT_BOSS && o->species != 0x33 &&
                           o->max_hp > 0 && o->hp < o->max_hp) {
                    /* Bosses redden as they take damage (Somulo is the
                     * exception, per the recorded fights). */
                    float hurt = 1.0f - (float)o->hp / (float)o->max_hp;
                    if (hurt < 0.0f)
                        hurt = 0.0f;
                    if (hurt > 1.0f)
                        hurt = 1.0f;
                    tint.r = 255;
                    tint.g = (unsigned char)(255.0f - 90.0f * hurt);
                    tint.b = (unsigned char)(255.0f - 120.0f * hurt);
                }
                DrawTexturePro(sf->texture, src, dst, (Vector2){0, 0}, 0.0f,
                               tint);
            } else {
                /* No trustworthy sheet for this species yet (the capture was
                 * ambiguous): draw a small silhouette so rooms are not filled
                 * with player clones. */
                float scale = 1.0f;
                int shade = 60 + (o->species * 41) % 70;
                Color body = {(unsigned char)(shade / 2 + 20),
                              (unsigned char)(shade / 3 + 16),
                              (unsigned char)(shade + 10), 255};
                Color eye = (Color){255, 226, 140, 255};
                float r = 5.0f * scale;
                float bh = 8.0f * scale;
                float feet = y;
                float cy = feet - bh - 4.0f * scale;
                /* legs */
                DrawEllipse((int)lroundf(x - r * 0.45f),
                            (int)lroundf(feet - 2.0f * scale),
                            (int)lroundf(1.6f * scale),
                            (int)lroundf(2.4f * scale), body);
                DrawEllipse((int)lroundf(x + r * 0.45f),
                            (int)lroundf(feet - 2.0f * scale),
                            (int)lroundf(1.6f * scale),
                            (int)lroundf(2.4f * scale), body);
                DrawEllipse((int)lroundf(x), (int)lroundf(cy),
                            (int)lroundf(r), (int)lroundf(bh), body);
                float hy = cy - bh - 2.0f * scale;
                DrawCircleV((Vector2){x, hy}, 3.6f * scale, body);
                DrawCircleV((Vector2){x - 1.5f * scale, hy - 0.6f * scale},
                            1.0f * scale, eye);
                DrawCircleV((Vector2){x + 1.5f * scale, hy - 0.6f * scale},
                            1.0f * scale, eye);
                if (o->type == OBJECT_BOSS) {
                    float hr = 3.6f * scale;
                    DrawTriangle((Vector2){x - hr * 0.8f, hy - hr * 0.7f},
                                 (Vector2){x - hr * 1.4f, hy - hr * 2.2f},
                                 (Vector2){x - hr * 0.1f, hy - hr * 1.1f},
                                 body);
                    DrawTriangle((Vector2){x + hr * 0.8f, hy - hr * 0.7f},
                                 (Vector2){x + hr * 1.4f, hy - hr * 2.2f},
                                 (Vector2){x + hr * 0.1f, hy - hr * 1.1f},
                                 body);
                }
            }
        }
    }
}

static void DrawHud(const Assets *assets, const Player *player) {
    /* The original HUD is a grid of dots, not a bar: ten life dots over six
     * potion dots, top-left, plus the crest slots. Measured from captures. */
    const int dot_r = 2;
    const int step = 6;
    const int row1_y = 9;
    const int row2_y = 16;
    const int x0 = 8;
    Color full = {235, 72, 96, 255};
    Color empty = {52, 44, 56, 255};
    Color rim = {18, 14, 22, 255};

    for (int i = 0; i < 10; i++) {
        int cx = x0 + i * step;
        bool on = i < player->max_hp && i < player->hp;
        bool slot = i < player->max_hp;
        DrawCircle(cx, row1_y, dot_r + 1.0f, rim);
        DrawCircle(cx, row1_y, dot_r,
                   on ? full : (slot ? empty : (Color){26, 22, 30, 255}));
    }
    for (int i = 0; i < 6; i++) {
        int cx = x0 + i * step;
        bool on = i < player->potions;
        DrawCircle(cx, row2_y, dot_r + 1.0f, rim);
        DrawCircle(cx, row2_y, dot_r, on ? (Color){240, 190, 90, 255}
                                         : (Color){38, 32, 44, 255});
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
