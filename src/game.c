#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "render.h"

static void Game_Reset(Game *game) {
    Player_Init(&game->player, &game->level);
    Projectiles_Init(&game->projectiles);
    Objects_Init(&game->objects, &game->level);
    Camera_Init(&game->camera, &game->level);
    game->state_timer = 0;
}

bool Game_Init(Game *game, const char *asset_dir) {
    memset(game, 0, sizeof(*game));
    if (!Assets_Load(&game->assets, asset_dir)) {
        fprintf(stderr, "game: asset load failed (run tools/dc_extract first)\n");
        return false;
    }
    if (!Level_Load(&game->level, &game->assets, asset_dir)) {
        Assets_Unload(&game->assets);
        return false;
    }
    Player_Init(&game->player, &game->level);
    Projectiles_Init(&game->projectiles);
    Objects_Init(&game->objects, &game->level);
    Camera_Init(&game->camera, &game->level);
    game->target = LoadRenderTexture(256, 224);
    if (!IsRenderTextureValid(game->target)) {
        fprintf(stderr, "game: failed to create 256x224 render target\n");
        Level_Unload(&game->level);
        Assets_Unload(&game->assets);
        return false;
    }
    SetTextureFilter(game->target.texture, TEXTURE_FILTER_POINT);

    const char *trace_path = getenv("DC_TRACE_CSV");
    if (trace_path && *trace_path) {
        game->trace = fopen(trace_path, "w");
        if (game->trace) {
            fprintf(game->trace,
                    "frame,player_x,player_y,vx,vy,state,camera_x,camera_y,hp,max_hp\n");
        } else {
            fprintf(stderr, "game: cannot open trace file %s\n", trace_path);
        }
    }

    const char *proj_trace_path = getenv("DC_TRACE_PROJECTILES");
    if (proj_trace_path && *proj_trace_path) {
        game->projectile_trace = fopen(proj_trace_path, "w");
        if (game->projectile_trace) {
            fprintf(game->projectile_trace, "frame,slot,x,y,vx,age\n");
        } else {
            fprintf(stderr, "game: cannot open projectile trace %s\n",
                    proj_trace_path);
        }
    }

    const char *object_trace_path = getenv("DC_TRACE_OBJECTS");
    if (object_trace_path && *object_trace_path) {
        game->object_trace = fopen(object_trace_path, "w");
        if (game->object_trace) {
            fprintf(game->object_trace,
                    "frame,slot,type,x,y,vx,vy,state\n");
        } else {
            fprintf(stderr, "game: cannot open object trace %s\n",
                    object_trace_path);
        }
    }

    game->ready = true;
    const char *demo = getenv("DC_DEMO");
    bool force_title = getenv("DC_TITLE") != NULL;
    bool automated = !force_title &&
                     (getenv("DC_RUN_FRAMES") ||
                      (demo && *demo && strcmp(demo, "0") != 0));
    game->state = automated ? GAME_PLAYING : GAME_TITLE;
    game->state_timer = 0;

    Audio_Init();
    Audio_StartMusic();
    return true;
}

static void Game_UpdateSimulation(Game *game) {
    game->objects.events = 0;
    Player_Update(&game->player, &game->level, &game->input);
    if (game->player.jump_started)
        Audio_Play(SFX_JUMP);
    if (game->player.landed_event)
        Audio_Play(SFX_LAND);

    bool shot = game->player.shot_fired;
    /* Existing shots move first; a shot fired this frame stays at its spawn
     * position for one frame, matching the original object loop. */
    Projectiles_Update(&game->projectiles, &game->level, &game->camera);
    if (shot) {
        Projectiles_Spawn(&game->projectiles, &game->player);
        game->player.shot_fired = false;
        Audio_Play(SFX_SHOOT);
    }
    Objects_CheckProjectiles(&game->objects, &game->projectiles);
    Objects_Update(&game->objects, &game->level, &game->player);
    if (game->player.hurt_event)
        Audio_Play(SFX_HURT);
    if (game->objects.events & OBJECT_EVENT_BREAK)
        Audio_Play(SFX_BREAK);
    if (game->objects.events & OBJECT_EVENT_PICKUP)
        Audio_Play(SFX_PICKUP);
    if (game->objects.events & OBJECT_EVENT_ENEMY_HIT)
        Audio_Play(SFX_ENEMY_HIT);

    Player_Animate(&game->player, &game->assets.player_anim);
    Camera_Update(&game->camera, &game->level, &game->player);

    for (int i = 0; i < game->level.object_count; i++) {
        const LevelObject *o = &game->level.objects[i];
        if (o->type != LEVEL_OBJECT_EXIT)
            continue;
        if (fabsf(game->player.x - o->x) < 16.0f &&
            fabsf(game->player.y - o->y) < 32.0f) {
            game->state = GAME_CLEAR;
            game->state_timer = 0;
            break;
        }
    }

    if (game->trace) {
        fprintf(game->trace, "%u,%.3f,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%d,%d\n",
                game->frame, game->player.x, game->player.y,
                game->player.vx, game->player.vy, (int)game->player.state,
                game->camera.x, game->camera.y, game->player.hp,
                game->player.max_hp);
    }
    if (game->projectile_trace) {
        for (int i = 0; i < PROJECTILE_POOL_SIZE; i++) {
            const Projectile *p = &game->projectiles.slots[i];
            if (!p->active)
                continue;
            fprintf(game->projectile_trace, "%u,%d,%.3f,%.3f,%.3f,%d\n",
                    game->frame, i, (float)p->x_fx / 256.0f,
                    (float)p->y_fx / 256.0f, (float)p->vx_fx / 256.0f,
                    p->age);
        }
    }
    if (game->object_trace) {
        for (int i = 0; i < OBJECT_POOL_SIZE; i++) {
            const Object *o = &game->objects.slots[i];
            if (!o->active)
                continue;
            fprintf(game->object_trace, "%u,%d,%d,%.3f,%.3f,%.3f,%.3f,%d\n",
                    game->frame, i, (int)o->type,
                    (float)o->x_fx / 256.0f, (float)o->y_fx / 256.0f,
                    (float)o->vx_fx / 256.0f, (float)o->vy_fx / 256.0f,
                    o->state);
        }
    }
    game->frame++;
}

void Game_Update(Game *game) {
    Input_Read(&game->input, &game->input_state);
    game->state_timer++;

    switch ((GameState)game->state) {
    case GAME_TITLE:
        if (game->input.start_pressed) {
            Game_Reset(game);
            game->state = GAME_PLAYING;
        }
        break;
    case GAME_PLAYING:
        if (game->input.pause_pressed) {
            game->state = GAME_PAUSED;
            game->state_timer = 0;
        } else {
            Game_UpdateSimulation(game);
        }
        break;
    case GAME_PAUSED:
        if (game->input.pause_pressed || game->input.start_pressed) {
            game->state = GAME_PLAYING;
            game->state_timer = 0;
        }
        break;
    case GAME_CLEAR:
        if (game->input.start_pressed) {
            Game_Reset(game);
            game->state = GAME_PLAYING;
        }
        break;
    }
    Audio_Update();
}

static void DrawCentered(const char *text, int y, int size, Color color) {
    Vector2 measure = MeasureTextEx(GetFontDefault(), text, (float)size, 1.0f);
    DrawTextEx(GetFontDefault(), text,
               (Vector2){(256.0f - measure.x) * 0.5f, (float)y},
               (float)size, 1.0f, color);
}

static void DrawOverlay(const Game *game) {
    if (game->state == GAME_TITLE) {
        DrawRectangle(0, 0, 256, 224, (Color){0, 0, 0, 185});
        DrawCentered("DEMON'S CREST", 58, 24, (Color){238, 232, 214, 255});
        DrawCentered("NATIVE FAN PORT", 92, 10, (Color){196, 168, 92, 255});
        if ((game->state_timer / 30) % 2 == 0)
            DrawCentered("PRESS ENTER OR START", 128, 10,
                         (Color){255, 255, 255, 255});
        DrawCentered("ARROWS MOVE   Z JUMP", 164, 10,
                     (Color){180, 180, 180, 255});
        DrawCentered("X SHOOT   C SPELL", 177, 10,
                     (Color){180, 180, 180, 255});
        DrawCentered("F1 COLLISION   ESC PAUSE", 190, 10,
                     (Color){180, 180, 180, 255});
    } else if (game->state == GAME_PAUSED) {
        DrawRectangle(0, 0, 256, 224, (Color){0, 0, 0, 150});
        DrawCentered("PAUSED", 96, 24, (Color){238, 232, 214, 255});
        DrawCentered("ESC OR BACK TO RESUME", 128, 10,
                     (Color){200, 200, 200, 255});
    } else if (game->state == GAME_CLEAR) {
        DrawRectangle(0, 0, 256, 224, (Color){0, 0, 0, 185});
        DrawCentered("AREA CLEAR", 82, 24, (Color){238, 232, 214, 255});
        DrawCentered("THE GRAVEYARD SLICE ENDS HERE", 116, 10,
                     (Color){196, 168, 92, 255});
        if ((game->state_timer / 30) % 2 == 0)
            DrawCentered("PRESS ENTER TO REPLAY", 148, 10,
                         (Color){255, 255, 255, 255});
    }
}

void Game_Render(Game *game) {
    BeginTextureMode(game->target);
    bool show_hud = game->state == GAME_PLAYING ||
                    game->state == GAME_PAUSED;
    Render_Game(&game->assets, &game->level, &game->camera, &game->player,
                &game->projectiles, &game->objects, show_hud,
                game->input.debug_collision);
    if (game->state != GAME_PLAYING)
        DrawOverlay(game);
    EndTextureMode();
}

void Game_Shutdown(Game *game) {
    Audio_Shutdown();
    if (game->trace)
        fclose(game->trace);
    if (game->projectile_trace)
        fclose(game->projectile_trace);
    if (game->object_trace)
        fclose(game->object_trace);
    if (IsRenderTextureValid(game->target))
        UnloadRenderTexture(game->target);
    Level_Unload(&game->level);
    Assets_Unload(&game->assets);
    memset(game, 0, sizeof(*game));
}
