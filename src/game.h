#ifndef DC_GAME_H
#define DC_GAME_H

#include <stdbool.h>
#include <stdio.h>

#include "assets.h"
#include "camera.h"
#include "input.h"
#include "level.h"
#include "player.h"
#include "raylib.h"

typedef struct Game {
    Assets assets;
    Level level;
    Player player;
    GameCamera camera;
    Input input;
    InputState input_state;
    RenderTexture2D target;
    FILE *trace;
    unsigned frame;
    bool ready;
} Game;

bool Game_Init(Game *game, const char *asset_dir);
void Game_Update(Game *game);
void Game_Render(Game *game);
void Game_Shutdown(Game *game);

#endif
