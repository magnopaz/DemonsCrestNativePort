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
                    "frame,player_x,player_y,vx,vy,state,camera_x,camera_y\n");
        } else {
            fprintf(stderr, "game: cannot open trace file %s\n", trace_path);
        }
    }

    game->ready = true;
    return true;
}

void Game_Update(Game *game) {
    Input_Read(&game->input, &game->input_state);
    Player_Update(&game->player, &game->level, &game->input);
    Player_Animate(&game->player, &game->assets.player_anim);
    Camera_Update(&game->camera, &game->level, &game->player);
    if (game->trace) {
        fprintf(game->trace, "%u,%.3f,%.3f,%.3f,%.3f,%d,%.3f,%.3f\n",
                game->frame, game->player.x, game->player.y,
                game->player.vx, game->player.vy, (int)game->player.state,
                game->camera.x, game->camera.y);
    }
    game->frame++;
}

void Game_Render(Game *game) {
    BeginTextureMode(game->target);
    Render_Game(&game->assets, &game->level, &game->camera, &game->player,
                game->input.debug_collision);
    EndTextureMode();
}

void Game_Shutdown(Game *game) {
    if (game->trace)
        fclose(game->trace);
    if (IsRenderTextureValid(game->target))
        UnloadRenderTexture(game->target);
    Level_Unload(&game->level);
    Assets_Unload(&game->assets);
    memset(game, 0, sizeof(*game));
}
