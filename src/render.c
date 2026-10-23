#include "render.h"

#include <math.h>
#include <stdlib.h>

#include "raylib.h"

static void DrawLayer(const Assets *assets, const BgLayer *layer,
                      const Level *level, const GameCamera *camera) {
    /* BG1 is the reference layer: its scroll is the camera position. BG2 keeps
     * the relative scroll it had in the captured frame. */
    float offset_x = (float)(layer->scroll_x - level->bg[1].scroll_x);
    float offset_y = (float)(layer->scroll_y - level->bg[1].scroll_y);
    /* The full-room BG1 is much wider than the captured BG2 ring; loop the
     * backdrop horizontally instead of running out of tiles. */
    bool wrap_x = layer->map_w < level->bg[1].map_w;

    float origin_x = camera->x + offset_x;
    float origin_y = camera->y + offset_y;

    int tx0 = (int)floorf(origin_x / 8.0f);
    int ty0 = (int)floorf(origin_y / 8.0f);
    int tx1 = (int)floorf((origin_x + 256.0f) / 8.0f) + 1;
    int ty1 = (int)floorf((origin_y + 224.0f) / 8.0f) + 1;

    for (int ty = ty0; ty <= ty1; ty++) {
        if (ty < 0 || ty >= layer->map_h)
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
            /* The extractor bakes four flip quadrants per palette:
             * normal (0,0), H (256,0), V (0,256), HV (256,256). */
            float qx = (word & 0x4000) ? 256.0f : 0.0f;
            float qy = (word & 0x8000) ? 256.0f : 0.0f;
            Rectangle src = {
                qx + (float)((tile % 32) * 8),
                qy + (float)((tile / 32) * 8),
                8.0f, 8.0f
            };
            Rectangle dst = {
                (float)tx * 8.0f - origin_x,
                (float)ty * 8.0f - origin_y,
                8.0f, 8.0f
            };
            DrawTexturePro(assets->bg_pal[pal], src, dst, (Vector2){0, 0}, 0.0f,
                           WHITE);
        }
    }
}

static void DrawPlayer(const Assets *assets, const Level *level,
                       const GameCamera *camera, const Player *player) {
    const AnimStep *step = NULL;
    if (assets->player_anim.ready && player->anim_clip >= 0)
        step = Anim_Step(&assets->player_anim, player->anim_clip,
                         player->anim_step);

    Texture2D texture = step ? step->texture : assets->player;
    float anchor_x = step ? (float)step->anchor_x : level->player_feet_x;
    float anchor_y = step ? (float)step->anchor_y : level->player_feet_y;

    float x = player->x - anchor_x - camera->x;
    float y = player->y - anchor_y - camera->y;
    Rectangle src = {0, 0, (float)texture.width, (float)texture.height};
    if (player->facing < 0) {
        src.x = (float)texture.width;
        src.width = -src.width;
    }
    Rectangle dst = {x, y, (float)texture.width, (float)texture.height};
    DrawTexturePro(texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
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
        Rectangle src = {0, 0, (float)step->texture.width,
                         (float)step->texture.height};
        if (p->vx_fx < 0) {
            src.x = (float)step->texture.width;
            src.width = -src.width;
        }
        Rectangle dst = {x, y, (float)step->texture.width,
                         (float)step->texture.height};
        DrawTexturePro(step->texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
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

        if (o->type == OBJECT_URN) {
            Texture2D tex = assets->item;
            DrawTexture(tex, (int)lroundf(x - tex.width * 0.5f),
                        (int)lroundf(y - tex.height * 0.5f), WHITE);
        } else if (o->type == OBJECT_PICKUP) {
            Texture2D tex = assets->heart;
            DrawTexture(tex, (int)lroundf(x - tex.width * 0.5f),
                        (int)lroundf(y - tex.height), WHITE);
        } else if (o->type == OBJECT_ENEMY && assets->enemy_frames > 0) {
            int frame = (o->timer / 8) % assets->enemy_frames;
            Texture2D tex = assets->enemy[frame];
            DrawTexture(tex, (int)lroundf(x - tex.width * 0.5f),
                        (int)lroundf(y - tex.height * 0.5f), WHITE);
        }
    }
}

static void DrawHud(const Assets *assets, const Player *player) {
    DrawRectangle(0, 0, 256, 24, (Color){0, 0, 0, 150});

    if (IsTextureValid(assets->hud_bar)) {
        int bar_x = 24;
        int bar_y = 16;
        int width = assets->hud_bar.width;
        DrawTexture(assets->hud_bar, bar_x, bar_y,
                    (Color){90, 90, 100, 255});
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
                               0.0f, WHITE);
            }
        }
    }

    if (IsTextureValid(assets->hud_slots)) {
        DrawTexture(assets->hud_slots, 144, 8, WHITE);
        /* Current talisman/spell slot is the first one for now. */
        DrawRectangleLines(144, 8, 32, 16, (Color){240, 200, 80, 220});
    }
}

void Render_Game(const Assets *assets, const Level *level, const GameCamera *camera,
                 const Player *player, const ProjectilePool *projectiles,
                 const ObjectPool *objects, bool debug_collision) {
    static int layer_mask = -1;
    if (layer_mask < 0) {
        const char *env = getenv("DC_LAYER_MASK");
        layer_mask = env ? atoi(env) : 0x3;
    }
    ClearBackground((Color){8, 8, 16, 255});

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
