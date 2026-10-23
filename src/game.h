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
#include "text.h"

/* Native password: 10 symbols, see Game_PasswordEncode. */
#define DC_PASSWORD_LEN 10

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
    char asset_dir[512];
    unsigned frame;
    int state_timer;
    int state; /* GameState */
    int menu_cursor;
    int subscreen_row;
    int overworld_node;
    float overworld_x;
    float overworld_y;
    float overworld_angle;
    uint8_t stages_cleared;
    char password_code[20];
    char password_entry[DC_PASSWORD_LEN + 1];
    int password_pos;
    int password_status; /* 0 neutral, 1 accepted, -1 invalid */
    bool campaign_mode;
    TextScript script;
    int shop_welcome; /* cached text indices (ROM shop dialogue) */
    int shop_full_urn;
    int shop_full_vellum;
    int shop_message;
    int ending_text;   /* cached ROM narration index */
    int ending_page;
    int dialogue_text;   /* script entry shown by GAME_DIALOGUE */
    int dialogue_page;
    int dialogue_return; /* state after the last page */
    int crest_text[6];   /* cached "YOU HAVE FOUND THE CREST OF ..." */
    int shop_timer;
    bool dialogue_demo; /* DC_DIALOGUE overlay */
    bool script_ready;
    bool has_save;
    bool prev_up;
    bool prev_down;
    bool prev_left;
    bool prev_right;
    bool ready;
} Game;

bool Game_Init(Game *game, const char *asset_dir);
void Game_Update(Game *game);
void Game_Render(Game *game);
void Game_Shutdown(Game *game);
bool Game_TransitionToArea(Game *game, int area_id, int spawn_id);
void Game_EncodePassword(const Game *game, char out[DC_PASSWORD_LEN + 1]);
uint32_t Game_PasswordPayload(const Game *game);
void Game_PasswordEncode(uint32_t payload, char out[DC_PASSWORD_LEN + 1]);
bool Game_PasswordDecode(const char *code, uint32_t *payload);
bool Game_ApplyPassword(Game *game, const char *code);
bool Game_PasswordSelfTest(void);
bool Game_SaveProgress(const Game *game);
bool Game_LoadProgress(Game *game);
bool Game_HasSave(void);

typedef enum GameState {
    GAME_TITLE = 0,
    GAME_PLAYING = 1,
    GAME_PAUSED = 2,
    GAME_CLEAR = 3,
    GAME_SUBSCREEN = 4,
    GAME_OVERWORLD = 5,
    GAME_SHOP = 6,
    GAME_PASSWORD = 7,
    GAME_OPTIONS = 8,
    GAME_OVER = 9,
    GAME_ENDING = 10,
    GAME_DIALOGUE = 11
} GameState;

#endif
