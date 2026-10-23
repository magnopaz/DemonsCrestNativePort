#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render.h"

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
    return true;
}

void Game_Update(Game *game) {
    Input_Read(&game->input, &game->input_state);
    Player_Update(&game->player, &game->level, &game->input);
    /* Existing shots move first; a shot fired this frame stays at its spawn
     * position for one frame, matching the original object loop. */
    Projectiles_Update(&game->projectiles, &game->level, &game->camera);
    if (game->player.shot_fired) {
        Projectiles_Spawn(&game->projectiles, &game->player);
        game->player.shot_fired = false;
    }
    Objects_CheckProjectiles(&game->objects, &game->projectiles);
    Objects_Update(&game->objects, &game->level, &game->player);
    Player_Animate(&game->player, &game->assets.player_anim);
    Camera_Update(&game->camera, &game->level, &game->player);
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

void Game_Render(Game *game) {
    BeginTextureMode(game->target);
    Render_Game(&game->assets, &game->level, &game->camera, &game->player,
                &game->projectiles, &game->objects,
                game->input.debug_collision);
    EndTextureMode();
}

void Game_Shutdown(Game *game) {
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
