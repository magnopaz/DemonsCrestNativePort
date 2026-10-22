#include "render.h"

#include <math.h>
#include <stdlib.h>

#include "raylib.h"

static void DrawLayer(const Assets *assets, const BgLayer *layer,
                      const Level *level, const GameCamera *camera) {
    /* BG1 is the reference layer: its scroll is the camera position. BG2 keeps
     * the relative scroll it had in the captured frame. */
    float base_scroll = (float)level->bg[1].scroll_x;
    float offset_x = (float)(layer->scroll_x - level->bg[1].scroll_x);
    float offset_y = (float)(layer->scroll_y - level->bg[1].scroll_y);
    (void)base_scroll;

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
            if (tx < 0 || tx >= layer->map_w)
                continue;
            uint16_t word = layer->tiles[ty * layer->map_w + tx];
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

void Render_Game(const Assets *assets, const Level *level, const GameCamera *camera,
                 const Player *player, bool debug_collision) {
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
        DrawText(TextFormat("x=%.0f y=%.0f  F1 collision", player->x, player->y),
                 3, 1, 10, (Color){220, 220, 220, 255});
    }
}
