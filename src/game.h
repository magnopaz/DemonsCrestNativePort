#ifndef DC_GAME_H
#define DC_GAME_H

#include <stdbool.h>
#include <stdio.h>

#include "assets.h"
#include "camera.h"
#include "input.h"
#include "level.h"
#include "player.h"
#include "projectile.h"
#include "object.h"
#include "raylib.h"

typedef struct Game {
    Assets assets;
    Level level;
    Player player;
    ProjectilePool projectiles;
    ObjectPool objects;
    GameCamera camera;
    Input input;
    InputState input_state;
    RenderTexture2D target;
    FILE *trace;
    FILE *projectile_trace;
    FILE *object_trace;
    unsigned frame;
    int state_timer;
    int state; /* GameState */
    bool ready;
} Game;

bool Game_Init(Game *game, const char *asset_dir);
void Game_Update(Game *game);
void Game_Render(Game *game);
void Game_Shutdown(Game *game);

typedef enum GameState {
    GAME_TITLE = 0,
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_CLEAR
} GameState;

#endif
