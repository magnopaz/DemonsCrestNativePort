#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "cJSON.h"
#include "render.h"

#ifdef _WIN32
#include <direct.h>
#define DC_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define DC_MKDIR(path) mkdir(path, 0755)
#endif

/* Native save format (not the retail password). One JSON file holds campaign
 * progression plus the options that should survive a restart. */
#define DC_SAVE_VERSION 2

static void Game_OpenOverworld(Game *game);
static void Game_CompleteArea(Game *game);

/* Campaign stage nodes on the Mode 7 overworld map. */
#define CAMPAIGN_COLOSSEUM 0
#define CAMPAIGN_GRAVEYARD 1
#define CAMPAIGN_CATACOMBS 2
#define CAMPAIGN_RUINS 3
#define CAMPAIGN_LAKE 4
#define CAMPAIGN_DEMON_KEEP 5

static const char *const kFireNames[] = {
    "FIREBALL", "BUSTER", "TORNADO", "CLAW", "DEMON FIRE"
};
static const char *const kTalismanNames[] = {
    "CROWN", "SKULL", "ARMOR", "FANG", "HAND"
};

static void Game_Reset(Game *game) {
    Player_Init(&game->player, &game->level);
    Projectiles_Init(&game->projectiles);
    Objects_Init(&game->objects, &game->level);
    Camera_Init(&game->camera, &game->level);
    game->objects.boss_alive = false;
    game->objects.boss_defeated = false;
    game->state_timer = 0;
}

bool Game_TransitionToArea(Game *game, int area_id, int spawn_id) {
    if (area_id < 0 || area_id >= DC_CAMPAIGN_AREAS)
        return false;
    /* Never enter a room the extractor classified as empty (menu/padding grid):
     * the player would fall through a world with no collision. */
    if (Assets_RoomKind(&game->assets, area_id) == 3) {
        fprintf(stderr, "game: refusing empty room %d\n", area_id);
        return false;
    }
    /* Preserve campaign progression across the level reload. */
    Player saved = game->player;
    int prev_area = game->level.area_id;
    if (!Level_LoadArea(&game->level, &game->assets, game->asset_dir, area_id)) {
        Level_LoadArea(&game->level, &game->assets, game->asset_dir, prev_area);
        Assets_LoadAreaBg(&game->assets, game->level.area_id);
        return false;
    }
    Assets_LoadAreaBg(&game->assets, game->level.area_id);
    Player_Init(&game->player, &game->level);
    game->player.max_hp = saved.max_hp > 0 ? saved.max_hp : 6;
    game->player.hp = saved.hp > 0 ? saved.hp : game->player.max_hp;
    game->player.crests_mask = saved.crests_mask;
    game->player.crest_form = saved.crest_form;
    game->player.talisman_mask = saved.talisman_mask;
    game->player.active_talisman = saved.active_talisman;
    game->player.potions = saved.potions;
    game->player.scrolls = saved.scrolls;
    game->player.gold = saved.gold;
    if (game->level.spawn_count > 1 && spawn_id > 0 &&
        spawn_id < game->level.spawn_count) {
        game->player.x_fx =
            (int32_t)lroundf(game->level.spawns[spawn_id].x * 256.0f);
        game->player.y_fx =
            (int32_t)lroundf(game->level.spawns[spawn_id].y * 256.0f);
        game->player.x = game->level.spawns[spawn_id].x;
        game->player.y = game->level.spawns[spawn_id].y;
    }
    Projectiles_Init(&game->projectiles);
    Objects_Init(&game->objects, &game->level);
    Camera_Init(&game->camera, &game->level);
    game->objects.boss_alive = false;
    game->objects.boss_defeated = false;
    game->state = GAME_PLAYING;
    game->state_timer = 0;
    return true;
}

/* Native password: 10 letters from a 20-symbol alphabet encode a 32-bit
 * progression payload plus an 8-bit FNV-1a checksum (40 bits). The retail
 * encode/decode algorithm is still to be decoded; this format is stable and
 * self-checking, so a mistyped code is rejected instead of corrupting a run.
 *
 * Payload layout: stages[7:0] max_hp[11:8] crests[17:12] form[20:18]
 *                 potions[23:21] scrolls[26:24] talismans[31:27] */
static const char kPasswordAlphabet[] = "BCDFGHJKLMNPQRSTVWXYZ";
#define DC_PASSWORD_ALPHABET 20

static uint8_t Password_Checksum(uint32_t payload) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < 4; i++) {
        h ^= (payload >> (i * 8)) & 0xFFu;
        h *= 16777619u;
    }
    return (uint8_t)(h & 0xFFu);
}

void Game_PasswordEncode(uint32_t payload, char out[DC_PASSWORD_LEN + 1]) {
    uint64_t value = (uint64_t)payload |
                     ((uint64_t)Password_Checksum(payload) << 32);
    for (int i = DC_PASSWORD_LEN - 1; i >= 0; i--) {
        out[i] = kPasswordAlphabet[value % DC_PASSWORD_ALPHABET];
        value /= DC_PASSWORD_ALPHABET;
    }
    out[DC_PASSWORD_LEN] = '\0';
}

bool Game_PasswordDecode(const char *code, uint32_t *payload) {
    uint64_t value = 0;
    int n = 0;
    for (; code[n] && n < DC_PASSWORD_LEN; n++) {
        const char *p = strchr(kPasswordAlphabet, code[n]);
        if (!p)
            return false;
        value = value * DC_PASSWORD_ALPHABET +
                (uint64_t)(p - kPasswordAlphabet);
    }
    if (n != DC_PASSWORD_LEN)
        return false;
    uint32_t pay = (uint32_t)(value & 0xFFFFFFFFu);
    if ((uint8_t)(value >> 32) != Password_Checksum(pay))
        return false;
    if (payload)
        *payload = pay;
    return true;
}

static int ClampInt(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

uint32_t Game_PasswordPayload(const Game *game) {
    const Player *p = &game->player;
    uint32_t v = 0;
    v |= (uint32_t)(game->stages_cleared & 0xFFu);
    v |= (uint32_t)ClampInt(p->max_hp, 1, 15) << 8;
    v |= (uint32_t)(p->crests_mask & 0x3Fu) << 12;
    v |= (uint32_t)ClampInt(p->crest_form, 0, 7) << 18;
    v |= (uint32_t)ClampInt(p->potions, 0, 7) << 21;
    v |= (uint32_t)ClampInt(p->scrolls, 0, 7) << 24;
    v |= (uint32_t)(p->talisman_mask & 0x1Fu) << 27;
    return v;
}

bool Game_ApplyPassword(Game *game, const char *code) {
    uint32_t payload = 0;
    if (!Game_PasswordDecode(code, &payload))
        return false;
    Player *p = &game->player;
    game->stages_cleared = (uint8_t)(payload & 0xFFu);
    p->max_hp = ClampInt((int)((payload >> 8) & 0xFu), 1, 15);
    p->hp = p->max_hp;
    p->crests_mask = (uint8_t)((payload >> 12) & 0x3Fu);
    p->crest_form = (int)((payload >> 18) & 0x7u);
    if (p->crest_form >= CREST_COUNT)
        p->crest_form = CREST_BASE;
    p->potions = (int)((payload >> 21) & 0x7u);
    p->scrolls = (int)((payload >> 24) & 0x7u);
    p->talisman_mask = (uint8_t)((payload >> 27) & 0x1Fu);
    if (!(p->talisman_mask & (1u << p->active_talisman)))
        p->active_talisman = 0;
    return true;
}

void Game_EncodePassword(const Game *game, char out[DC_PASSWORD_LEN + 1]) {
    Game_PasswordEncode(Game_PasswordPayload(game), out);
}

bool Game_PasswordSelfTest(void) {
    uint32_t state = 0x12345678u;
    for (int i = 0; i < 2000; i++) {
        state = state * 1664525u + 1013904223u;
        uint32_t payload = state;
        char code[DC_PASSWORD_LEN + 1];
        Game_PasswordEncode(payload, code);
        uint32_t decoded = 0;
        if (!Game_PasswordDecode(code, &decoded) || decoded != payload) {
            fprintf(stderr, "password selftest: FAIL at %d (%s)\n", i, code);
            return false;
        }
    }
    fprintf(stderr, "password selftest: OK (2000 round-trips)\n");
    return true;
}

static const char *Game_SavePath(void) {
    const char *env = getenv("DC_SAVE_PATH");
    if (env && *env)
        return env;
    return "saves/dc_save.json";
}

static bool Game_SaveDisabled(void) {
    if (getenv("DC_NO_SAVE"))
        return true;
    /* Deterministic runs must stay side-effect free unless a test opts in. */
    if (getenv("DC_RUN_FRAMES") && !getenv("DC_SAVE_FORCE"))
        return true;
    return false;
}

static void Game_MakeSaveDir(const char *path) {
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    char *bslash = strrchr(dir, '\\');
    if (bslash && (!slash || bslash > slash))
        slash = bslash;
    if (!slash)
        return;
    *slash = '\0';
    if (*dir)
        (void)DC_MKDIR(dir); /* exists already -> EEXIST is fine */
}

bool Game_HasSave(void) {
    if (Game_SaveDisabled())
        return false;
    FILE *f = fopen(Game_SavePath(), "rb");
    if (!f)
        return false;
    fclose(f);
    return true;
}

static int Game_JsonInt(const cJSON *root, const char *key, int fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(item))
        return fallback;
    return item->valueint;
}

bool Game_SaveProgress(const Game *game) {
    if (Game_SaveDisabled())
        return false;
    if (!game->campaign_mode)
        return false;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return false;
    const Player *p = &game->player;
    cJSON_AddNumberToObject(root, "version", DC_SAVE_VERSION);
    cJSON_AddNumberToObject(root, "area", game->level.area_id);
    cJSON_AddNumberToObject(root, "stages_cleared", game->stages_cleared);
    cJSON_AddNumberToObject(root, "max_hp", p->max_hp);
    cJSON_AddNumberToObject(root, "hp", p->hp);
    cJSON_AddNumberToObject(root, "crests", p->crests_mask);
    cJSON_AddNumberToObject(root, "form", p->crest_form);
    cJSON_AddNumberToObject(root, "talismans", p->talisman_mask);
    cJSON_AddNumberToObject(root, "talisman", p->active_talisman);
    cJSON_AddNumberToObject(root, "fire_element", p->fire_element);
    cJSON_AddNumberToObject(root, "potions", p->potions);
    cJSON_AddNumberToObject(root, "scrolls", p->scrolls);
    cJSON_AddNumberToObject(root, "gold", p->gold);
    cJSON_AddNumberToObject(root, "volume", (double)Audio_GetVolume());

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text)
        return false;

    const char *path = Game_SavePath();
    Game_MakeSaveDir(path);
    FILE *f = fopen(path, "wb");
    bool ok = false;
    if (f) {
        ok = fwrite(text, 1, strlen(text), f) == strlen(text);
        fclose(f);
    }
    cJSON_free(text);
    if (!ok)
        fprintf(stderr, "game: cannot write save %s\n", path);
    return ok;
}

bool Game_LoadProgress(Game *game) {
    if (Game_SaveDisabled())
        return false;
    const char *path = Game_SavePath();
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 65536) {
        fclose(f);
        return false;
    }
    char *text = (char *)malloc((size_t)size + 1);
    if (!text) {
        fclose(f);
        return false;
    }
    size_t got = fread(text, 1, (size_t)size, f);
    fclose(f);
    text[got] = '\0';

    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root)
        return false;
    if (Game_JsonInt(root, "version", 0) != DC_SAVE_VERSION) {
        cJSON_Delete(root);
        return false;
    }

    int area = Game_JsonInt(root, "area", game->level.area_id);
    if (area >= 0 && area < DC_CAMPAIGN_AREAS && area != game->level.area_id)
        (void)Game_TransitionToArea(game, area, 0);

    Player *p = &game->player;
    p->max_hp = Game_JsonInt(root, "max_hp", p->max_hp);
    if (p->max_hp < 1)
        p->max_hp = 6;
    if (p->max_hp > 12)
        p->max_hp = 12;
    p->hp = Game_JsonInt(root, "hp", p->max_hp);
    if (p->hp < 1)
        p->hp = 1;
    if (p->hp > p->max_hp)
        p->hp = p->max_hp;
    p->crests_mask = (uint8_t)Game_JsonInt(root, "crests", p->crests_mask);
    int form = Game_JsonInt(root, "form", p->crest_form);
    p->crest_form = (form >= 0 && form < CREST_COUNT) ? form : CREST_BASE;
    p->talisman_mask = (uint8_t)Game_JsonInt(root, "talismans",
                                             p->talisman_mask);
    int talisman = Game_JsonInt(root, "talisman", p->active_talisman);
    p->active_talisman = (talisman >= 0 && talisman < 5) ? talisman : 0;
    p->fire_element = Game_JsonInt(root, "fire_element", p->fire_element) % 5;
    p->potions = Game_JsonInt(root, "potions", p->potions);
    p->scrolls = Game_JsonInt(root, "scrolls", p->scrolls);
    p->gold = Game_JsonInt(root, "gold", p->gold);
    game->stages_cleared = (uint8_t)Game_JsonInt(root, "stages_cleared",
                                                 game->stages_cleared);

    const cJSON *vol = cJSON_GetObjectItemCaseSensitive(root, "volume");
    if (cJSON_IsNumber(vol))
        Audio_SetVolume((float)vol->valuedouble);
    cJSON_Delete(root);
    game->has_save = true;
    return true;
}

bool Game_Init(Game *game, const char *asset_dir) {
    memset(game, 0, sizeof(*game));
    snprintf(game->asset_dir, sizeof(game->asset_dir), "%s", asset_dir);
    if (!Assets_Load(&game->assets, asset_dir)) {
        fprintf(stderr, "game: asset load failed (run tools/dc_extract first)\n");
        return false;
    }
    if (!Level_Load(&game->level, &game->assets, asset_dir)) {
        Assets_Unload(&game->assets);
        return false;
    }
    Assets_LoadAreaBg(&game->assets, game->level.area_id);
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
                    "frame,player_x,player_y,vx,vy,state,camera_x,camera_y,hp,max_hp,area,form,gold,fire\n");
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
    game->has_save = Game_HasSave();
    if (Text_LoadScript(&game->script, asset_dir)) {
        game->script_ready = true;
        game->shop_welcome = Text_Find(&game->script, "BLACK LOTUS");
        game->shop_full_urn = Text_Find(&game->script, "NEED AN URN");
        game->shop_full_vellum = Text_Find(&game->script, "VELLUM IS");
        game->ending_text = Text_Find(&game->script, "PHALANX WOULD");
        game->crest_text[CREST_EARTH] =
            Text_Find(&game->script, "CREST OF \"EARTH\"");
        game->crest_text[CREST_AIR] =
            Text_Find(&game->script, "CREST OF \"AIR\"");
        game->crest_text[CREST_WATER] =
            Text_Find(&game->script, "CREST OF \"WATER\"");
        game->crest_text[CREST_TIME] =
            Text_Find(&game->script, "CREST OF \"TIME\"");
        game->fire_text[1] = Text_Find(&game->script, "\"BUSTER\"");
        game->fire_text[2] = Text_Find(&game->script, "\"TORNADO\"");
        game->fire_text[3] = Text_Find(&game->script, "\"CLAW\"");
        game->fire_text[4] = Text_Find(&game->script, "\"DEMON FIRE\"");
        game->dialogue_demo = getenv("DC_DIALOGUE") != NULL;
    }
    const char *password_env = getenv("DC_PASSWORD");
    if (password_env && *password_env) {
        if (Game_ApplyPassword(game, password_env)) {
            game->campaign_mode = true;
            fprintf(stderr, "game: password applied\n");
        } else {
            fprintf(stderr, "game: invalid DC_PASSWORD\n");
        }
    }
    const char *demo = getenv("DC_DEMO");
    bool force_title = getenv("DC_TITLE") != NULL;
    bool automated = !force_title &&
                     (getenv("DC_RUN_FRAMES") ||
                      (demo && *demo && strcmp(demo, "0") != 0));
    game->state = automated ? GAME_PLAYING : GAME_TITLE;
    if (getenv("DC_CAMPAIGN"))
        game->campaign_mode = true;
    /* Debug state selection for screenshots of campaign menus. */
    const char *state_env = getenv("DC_STATE");
    if (state_env && *state_env) {
        if (strcmp(state_env, "title") == 0)
            game->state = GAME_TITLE;
        else if (strcmp(state_env, "overworld") == 0) {
            game->campaign_mode = true;
            Game_OpenOverworld(game);
        } else if (strcmp(state_env, "subscreen") == 0)
            game->state = GAME_SUBSCREEN;
        else if (strcmp(state_env, "shop") == 0)
            game->state = GAME_SHOP;
        else if (strcmp(state_env, "password") == 0)
            game->state = GAME_PASSWORD;
        else if (strcmp(state_env, "options") == 0)
            game->state = GAME_OPTIONS;
        else if (strcmp(state_env, "clear") == 0) {
            game->campaign_mode = true;
            Game_CompleteArea(game);
        } else if (strcmp(state_env, "gameover") == 0)
            game->state = GAME_OVER;
        else if (strcmp(state_env, "ending") == 0)
            game->state = GAME_ENDING;
    }
    game->state_timer = 0;

    /* Dev hook: jump straight into a story page for deterministic captures
     * (`DC_STORY_TEST=<crest index>`, optional `DC_STORY_PAGE`). */
    const char *fire_test = getenv("DC_FIRE_TEST");
    if (fire_test && *fire_test && game->script_ready) {
        int piece = atoi(fire_test);
        if (piece >= 1 && piece <= 4 && game->fire_text[piece] >= 0) {
            game->dialogue_text = game->fire_text[piece];
            const char *page = getenv("DC_STORY_PAGE");
            game->dialogue_page = page ? atoi(page) : 0;
            game->dialogue_return = GAME_TITLE;
            game->state = GAME_DIALOGUE;
        }
    }
    const char *story_test = getenv("DC_STORY_TEST");
    if (getenv("DC_STORY_DEBUG"))
        fprintf(stderr, "story: ready=%d crest_text=[%d %d %d %d %d %d]\n",
                game->script_ready, game->crest_text[0], game->crest_text[1],
                game->crest_text[2], game->crest_text[3], game->crest_text[4],
                game->crest_text[5]);
    if (story_test && *story_test && game->script_ready) {
        int crest = atoi(story_test);
        if (crest >= 0 && crest < CREST_COUNT && game->crest_text[crest] >= 0) {
            game->dialogue_text = game->crest_text[crest];
            const char *page = getenv("DC_STORY_PAGE");
            game->dialogue_page = page ? atoi(page) : 0;
            game->dialogue_return = GAME_TITLE;
            game->state = GAME_DIALOGUE;
        }
    }

    Audio_Init();
    /* Dev hook: resume the saved run without going through the title menu. */
    if (getenv("DC_RESUME") && game->has_save) {
        if (Game_LoadProgress(game)) {
            game->campaign_mode = true;
            game->state = GAME_PLAYING;
            game->state_timer = 0;
        }
    }
    Audio_StartMusic();
    return true;
}

static bool Game_NarrativeEnabled(void);

static void Game_UpdateSimulation(Game *game) {
    game->objects.events = 0;
    game->objects.last_fire_piece = 0;
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
    if ((game->objects.events & OBJECT_EVENT_FIRE_PIECE) &&
        Game_NarrativeEnabled()) {
        int piece = game->objects.last_fire_piece;
        if (piece >= 1 && piece <= 4 && game->fire_text[piece] >= 0) {
            game->dialogue_text = game->fire_text[piece];
            game->dialogue_page = 0;
            game->dialogue_return = GAME_PLAYING;
            game->state = GAME_DIALOGUE;
            game->state_timer = 0;
        }
    }
    if (game->player.hurt_event)
        Audio_Play(SFX_HURT);
    if (game->objects.events & OBJECT_EVENT_BREAK)
        Audio_Play(SFX_BREAK);
    if (game->objects.events & OBJECT_EVENT_PICKUP)
        Audio_Play(SFX_PICKUP);
    if (game->objects.events & OBJECT_EVENT_ENEMY_HIT)
        Audio_Play(SFX_ENEMY_HIT);
    if (game->objects.events & OBJECT_EVENT_BOSS_DEFEATED)
        Audio_Play(SFX_BREAK);
    if (game->objects.events & OBJECT_EVENT_CREST_PICKUP)
        Audio_Play(SFX_PICKUP);

    Player_Animate(&game->player, &game->assets.player_anim);
    Camera_Update(&game->camera, &game->level, &game->player);

    for (int i = 0; i < game->level.object_count; i++) {
        const LevelObject *o = &game->level.objects[i];
        if (o->type != LEVEL_OBJECT_EXIT)
            continue;
        if (fabsf(game->player.x - o->x) >= 16.0f ||
            fabsf(game->player.y - o->y) >= 32.0f)
            continue;
        /* Boss-gated exits stay closed until the area boss is defeated. */
        if (game->objects.boss_alive)
            break;
        if (!game->campaign_mode) {
            game->state = GAME_CLEAR;
            game->state_timer = 0;
            break;
        }
        int next = game->level.gate_next_area;
        if (next >= 0) {
            Game_TransitionToArea(game, next, 0);
        } else {
            game->campaign_mode = true;
            Game_OpenOverworld(game);
        }
        return;
    }

    if (game->trace) {
        fprintf(game->trace,
                "%u,%.3f,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%d,%d,%d,%d,%d,%d\n",
                game->frame, game->player.x, game->player.y,
                game->player.vx, game->player.vy, (int)game->player.state,
                game->camera.x, game->camera.y, game->player.hp,
                game->player.max_hp, game->level.area_id,
                game->player.crest_form, game->player.gold,
                game->player.fire_element);
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

static bool PlayerInsideTransition(const Player *player,
                                   const LevelTransition *tr) {
    /* The original box check runs before collision resolution, so a door box
     * embedded in the room's edge wall is reachable while walking into it. */
    return fabsf(player->x - tr->x) <= tr->half_w + 20.0f &&
           fabsf(player->y - tr->y) <= tr->half_h + 8.0f;
}

static void Game_CheckTransitions(Game *game) {
    if (!game->campaign_mode)
        return;
    Player *player = &game->player;
    if (!game->objects.boss_alive) {
        for (int i = 0; i < game->level.transition_count; i++) {
            const LevelTransition *tr = &game->level.transitions[i];
            if (tr->next_area < 0 || tr->next_area >= 113 ||
                tr->next_area == game->level.area_id)
                continue;
            if (PlayerInsideTransition(player, tr)) {
                Game_TransitionToArea(game, tr->next_area, tr->spawn_id);
                return;
            }
        }
    }
    /* Boss gate $81:97DA: advancing requires the area boss to be defeated. */
    if (game->level.gate_next_area >= 0 &&
        game->level.gate_next_area < 113 && !game->objects.boss_alive) {
        float gate_x = (float)game->level.world_w - 8.0f;
        if (player->x >= gate_x) {
            Game_TransitionToArea(game, game->level.gate_next_area, 0);
            return;
        }
    }
}

static void Game_HandleDeath(Game *game) {
    if (game->player.hp > 0 || game->state != GAME_PLAYING)
        return;
    game->state = GAME_OVER;
    game->state_timer = 0;
}

static void Game_OpenOverworld(Game *game) {
    (void)Game_SaveProgress(game);
    game->state = GAME_OVERWORLD;
    game->state_timer = 0;
    game->overworld_x = 96.0f;
    game->overworld_y = 176.0f;
    game->overworld_angle = 0.0f;
    game->overworld_node = CAMPAIGN_GRAVEYARD;
}

/* Deterministic test runs fast-forward story beats; players see them. */
static bool Game_NarrativeEnabled(void) {
    return getenv("DC_RUN_FRAMES") == NULL && getenv("DC_NO_STORY") == NULL;
}

static void Game_CompleteArea(Game *game) {
    int area = game->level.area_id;
    if (area >= 0 && area < 8)
        game->stages_cleared |= (uint8_t)(1u << area);
    /* Story progression: each cleared area grants the next Gargoyle Crest. */
    int granted = -1;
    switch (area) {
    case 0: granted = CREST_EARTH; break;
    case 1: granted = CREST_AIR; break;
    case 2: granted = CREST_WATER; break;
    case 3: granted = CREST_TIME; break;
    case 4: granted = CREST_ULTIMATE; break;
    default: break;
    }
    if (granted >= 0) {
        static const uint8_t kBits[CREST_COUNT] = {0x01, 0x02, 0x04, 0x08,
                                                  0x10, 0x20};
        game->player.crests_mask |= kBits[granted];
    }
    Game_EncodePassword(game, game->password_code);
    game->state = GAME_CLEAR;
    game->state_timer = 0;
    (void)Game_SaveProgress(game);
    /* Show the ROM's "YOU HAVE FOUND THE CREST OF ..." pages before the clear
     * banner when a crest was just granted and story beats are enabled. */
    if (granted >= 0 && granted < CREST_COUNT &&
        game->crest_text[granted] >= 0 && Game_NarrativeEnabled()) {
        game->dialogue_text = game->crest_text[granted];
        game->dialogue_page = 0;
        game->dialogue_return = GAME_CLEAR;
        game->state = GAME_DIALOGUE;
        game->state_timer = 0;
    }
}

static void Game_UpdateOverworld(Game *game) {
    const float speed = 1.6f;
    float dx = 0.0f, dy = 0.0f;
    if (game->input.left)
        dx -= speed;
    if (game->input.right)
        dx += speed;
    if (game->input.up)
        dy -= speed;
    if (game->input.down)
        dy += speed;
    game->overworld_x += dx;
    game->overworld_y += dy;
    if (game->overworld_x < 24.0f)
        game->overworld_x = 24.0f;
    if (game->overworld_x > 232.0f)
        game->overworld_x = 232.0f;
    if (game->overworld_y < 152.0f)
        game->overworld_y = 152.0f;
    if (game->overworld_y > 208.0f)
        game->overworld_y = 208.0f;
    game->overworld_angle += (game->input.right ? 2.5f : 0.0f) -
                             (game->input.left ? 2.5f : 0.0f);

    /* Landing-point selection by nearest horizontal node. */
    static const struct { int node; float x; float y; } kNodes[] = {
        {CAMPAIGN_COLOSSEUM, 40.0f, 168.0f},
        {CAMPAIGN_GRAVEYARD, 96.0f, 176.0f},
        {CAMPAIGN_CATACOMBS, 144.0f, 184.0f},
        {CAMPAIGN_RUINS, 192.0f, 176.0f},
        {CAMPAIGN_LAKE, 64.0f, 200.0f},
        {CAMPAIGN_DEMON_KEEP, 224.0f, 160.0f},
    };
    int nearest = 1;
    float best = 1e9f;
    for (int i = 0; i < 6; i++) {
        float ddx = kNodes[i].x - game->overworld_x;
        float ddy = kNodes[i].y - game->overworld_y;
        float d2 = ddx * ddx + ddy * ddy;
        if (d2 < best) {
            best = d2;
            nearest = kNodes[i].node;
        }
    }
    game->overworld_node = nearest;

    if (game->input.start_pressed) {
        if (nearest == CAMPAIGN_DEMON_KEEP) {
            /* Final keep requires all earlier stages cleared. */
            if ((game->stages_cleared & 0x0F) == 0x0F) {
                game->state = GAME_ENDING;
                game->state_timer = 0;
                return;
            }
            return;
        }
        Game_TransitionToArea(game, nearest, 0);
        game->campaign_mode = true;
    } else if (game->input.special_pressed) {
        game->state = GAME_SHOP;
        game->state_timer = 0;
    }
}

static void Game_UpdateSubscreen(Game *game, bool left_pressed,
                                 bool right_pressed, bool up_pressed,
                                 bool down_pressed) {
    Player *p = &game->player;
    if (left_pressed)
        Player_CycleForm(p, -1);
    if (right_pressed)
        Player_CycleForm(p, 1);
    if (up_pressed) {
        if (game->subscreen_row > 0)
            game->subscreen_row--;
    }
    if (down_pressed) {
        if (game->subscreen_row < 5)
            game->subscreen_row++;
    }
    if (game->input.special_pressed)
        p->fire_element = (p->fire_element + 1) % 5;
    if (game->input.shot_pressed) {
        p->active_talisman = (p->active_talisman + 1) % 5;
        if (!(p->talisman_mask & (1u << p->active_talisman)))
            p->active_talisman = 0;
    }
    if (game->input.jump_pressed && p->potions > 0) {
        p->potions--;
        if (p->hp < p->max_hp)
            p->hp++;
        Audio_Play(SFX_PICKUP);
    }
    if (game->input.form_next_pressed && p->scrolls > 0) {
        p->scrolls--;
        if (p->hp > 0)
            p->hp--;
        Audio_Play(SFX_SHOOT);
    }
    if (game->input.subscreen_pressed || game->input.start_pressed ||
        game->input.pause_pressed) {
        game->state = GAME_PLAYING;
        game->state_timer = 0;
    }
}

static void Game_UpdateTitle(Game *game, bool up_pressed, bool down_pressed) {
    const int kItemCount = game->has_save ? 5 : 4;
    if (up_pressed && game->menu_cursor > 0)
        game->menu_cursor--;
    if (down_pressed && game->menu_cursor < kItemCount - 1)
        game->menu_cursor++;
    if (!game->input.start_pressed)
        return;
    int item = game->menu_cursor;
    if (game->has_save) {
        if (item == 0) {
            if (Game_LoadProgress(game)) {
                game->campaign_mode = true;
                game->state = GAME_PLAYING;
                game->state_timer = 0;
            }
            return;
        }
        item--;
    }
    switch (item) {
    case 0:
        game->campaign_mode = true;
        game->stages_cleared = 0;
        game->menu_cursor = 0;
        /* Reset run progression: Player_Init applies the 6 HP / 4 HP,
         * Base-Crest, 2 Urns and 1 Spell starting kit. */
        Player_Init(&game->player, &game->level);
        if (!Game_TransitionToArea(game, CAMPAIGN_COLOSSEUM, 0)) {
            Game_Reset(game);
            game->state = GAME_PLAYING;
        }
        break;
    case 1:
        game->campaign_mode = true;
        Game_OpenOverworld(game);
        break;
    case 2:
        Game_EncodePassword(game, game->password_entry);
        game->password_pos = 0;
        game->password_status = 0;
        game->state = GAME_PASSWORD;
        game->state_timer = 0;
        break;
    case 3:
        game->state = GAME_OPTIONS;
        game->state_timer = 0;
        break;
    default:
        break;
    }
}

static void Game_UpdateShop(Game *game, bool left_pressed,
                            bool right_pressed, bool up_pressed) {
    if (game->shop_timer > 0)
        game->shop_timer--;
    if (left_pressed) {
        if (game->player.potions >= 5) {
            /* "YOUR URN IS ALREADY FULL OF" ($85:87AC) */
            game->shop_message = game->shop_full_urn;
            game->shop_timer = 120;
        } else if (game->player.gold >= 30) {
            game->player.gold -= 30;
            game->player.potions++;
            Audio_Play(SFX_PICKUP);
        }
    }
    if (right_pressed) {
        if (game->player.scrolls >= 5) {
            /* "YOUR VELLUM IS ALREADY FULL OF" ($85:875B) */
            game->shop_message = game->shop_full_vellum;
            game->shop_timer = 120;
        } else if (game->player.gold >= 50) {
            game->player.gold -= 50;
            game->player.scrolls++;
            Audio_Play(SFX_PICKUP);
        }
    }
    if (up_pressed && game->player.gold >= 80 && game->player.max_hp < 10) {
        game->player.gold -= 80;
        game->player.max_hp++;
        game->player.hp = game->player.max_hp;
        Audio_Play(SFX_PICKUP);
    }
    if (game->input.start_pressed || game->input.pause_pressed) {
        game->campaign_mode = true;
        Game_OpenOverworld(game);
    }
}

static void Game_UpdatePassword(Game *game, bool up_pressed,
                               bool down_pressed, bool left_pressed,
                               bool right_pressed) {
    if (game->password_entry[0] == '\0') {
        Game_EncodePassword(game, game->password_entry);
        game->password_pos = 0;
    }
    if (left_pressed && game->password_pos > 0)
        game->password_pos--;
    if (right_pressed && game->password_pos < DC_PASSWORD_LEN - 1)
        game->password_pos++;
    if (up_pressed || down_pressed) {
        int dir = down_pressed ? 1 : -1;
        char *c = &game->password_entry[game->password_pos];
        const char *p = strchr(kPasswordAlphabet, *c);
        int idx = p ? (int)(p - kPasswordAlphabet) : 0;
        idx = (idx + dir + DC_PASSWORD_ALPHABET) % DC_PASSWORD_ALPHABET;
        *c = kPasswordAlphabet[idx];
        game->password_status = 0;
    }
    if (game->input.start_pressed) {
        if (Game_ApplyPassword(game, game->password_entry)) {
            game->password_status = 1;
            game->campaign_mode = true;
            game->has_save = true;
            Game_OpenOverworld(game);
        } else {
            game->password_status = -1;
        }
    } else if (game->input.pause_pressed) {
        game->state = GAME_TITLE;
        game->state_timer = 0;
    }
}

static void Game_UpdateOptions(Game *game) {
    if (game->input.left)
        Audio_SetVolume(Audio_GetVolume() - 0.1f);
    if (game->input.right)
        Audio_SetVolume(Audio_GetVolume() + 0.1f);
    if (game->input.start_pressed || game->input.pause_pressed) {
        game->state = GAME_TITLE;
        game->state_timer = 0;
    }
}

void Game_Update(Game *game) {
    Input_Read(&game->input, &game->input_state);
    game->state_timer++;

    /* Edge-detect direction presses for menu navigation. */
    bool up_pressed = game->input.up && !game->prev_up;
    bool down_pressed = game->input.down && !game->prev_down;
    bool left_pressed = game->input.left && !game->prev_left;
    bool right_pressed = game->input.right && !game->prev_right;
    game->prev_up = game->input.up;
    game->prev_down = game->input.down;
    game->prev_left = game->input.left;
    game->prev_right = game->input.right;

    switch ((GameState)game->state) {
    case GAME_TITLE:
        Game_UpdateTitle(game, up_pressed, down_pressed);
        break;
    case GAME_PLAYING: {
        bool has_exit = false;
        for (int i = 0; i < game->level.object_count; i++) {
            if (game->level.objects[i].type == LEVEL_OBJECT_EXIT) {
                has_exit = true;
                break;
            }
        }
        if (game->campaign_mode && !has_exit &&
            game->level.gate_next_area < 0 && game->input.start_pressed) {
            Game_OpenOverworld(game);
            break;
        }
        if (game->input.subscreen_pressed) {
            game->state = GAME_SUBSCREEN;
            game->state_timer = 0;
        } else if (game->input.pause_pressed) {
            game->state = GAME_PAUSED;
            game->menu_cursor = 0;
            game->state_timer = 0;
        } else {
            int area_before = game->level.area_id;
            Game_UpdateSimulation(game);
            Game_HandleDeath(game);
            if (game->state != GAME_PLAYING)
                break;
            if (game->objects.boss_defeated &&
                game->level.area_id == area_before) {
                game->objects.boss_defeated = false;
                game->player.max_hp = game->player.max_hp < 10
                                         ? game->player.max_hp + 1
                                         : game->player.max_hp;
                game->player.hp = game->player.max_hp;
                game->campaign_mode = true;
                Game_CompleteArea(game);
            } else {
                Game_CheckTransitions(game);
            }
        }
        break;
    }
    case GAME_PAUSED:
        if (up_pressed && game->menu_cursor > 0)
            game->menu_cursor--;
        if (down_pressed && game->menu_cursor < 1)
            game->menu_cursor++;
        if (game->input.pause_pressed) {
            game->state = GAME_PLAYING;
            game->state_timer = 0;
        } else if (game->input.start_pressed) {
            if (game->menu_cursor == 0) {
                game->state = GAME_PLAYING;
            } else {
                game->menu_cursor = 0;
                game->campaign_mode = true;
                Game_OpenOverworld(game);
            }
            game->state_timer = 0;
        }
        break;
    case GAME_SUBSCREEN:
        Game_UpdateSubscreen(game, left_pressed, right_pressed, up_pressed,
                             down_pressed);
        break;
    case GAME_OVERWORLD:
        Game_UpdateOverworld(game);
        break;
    case GAME_CLEAR:
        if (game->input.start_pressed) {
            if (game->campaign_mode) {
                Game_OpenOverworld(game);
            } else {
                Game_Reset(game);
                game->state = GAME_PLAYING;
            }
        }
        break;
    case GAME_SHOP:
        Game_UpdateShop(game, left_pressed, right_pressed, up_pressed);
        break;
    case GAME_PASSWORD:
        Game_UpdatePassword(game, up_pressed, down_pressed, left_pressed,
                            right_pressed);
        break;
    case GAME_OPTIONS:
        Game_UpdateOptions(game);
        break;
    case GAME_OVER:
        if (game->input.start_pressed) {
            game->player.hp = game->player.max_hp;
            game->campaign_mode = true;
            Game_OpenOverworld(game);
        }
        break;
    case GAME_DIALOGUE: {
        int pages = game->script.page_count[game->dialogue_text];
        if (game->input.start_pressed) {
            if (game->dialogue_page + 1 < pages) {
                game->dialogue_page++;
            } else {
                game->state = game->dialogue_return;
                game->state_timer = 0;
            }
        }
        break;
    }
    case GAME_ENDING: {
        int pages = (game->ending_text >= 0)
                        ? game->script.page_count[game->ending_text] : 0;
        if (game->input.start_pressed) {
            if (game->ending_page + 1 < pages) {
                game->ending_page++;
            } else {
                game->ending_page = 0;
                game->state = GAME_TITLE;
            }
        }
        break;
    }
    }
    Audio_Update();
}

static void DrawCentered(const char *text, int y, int size, Color color) {
    Vector2 measure = MeasureTextEx(GetFontDefault(), text, (float)size, 1.0f);
    DrawTextEx(GetFontDefault(), text,
               (Vector2){(256.0f - measure.x) * 0.5f, (float)y},
               (float)size, 1.0f, color);
}

static void DrawOverworld(const Game *game) {
    /* Mode 7 style pseudo-3D overworld: horizontal strips shrink toward the
     * horizon, with land/water/path bands and stage nodes. */
    ClearBackground((Color){6, 8, 14, 255});
    for (int y = 0; y < 224; y++) {
        float t = (float)y / 223.0f;
        float depth = 1.0f / (0.08f + t * 0.92f);
        float w = 256.0f * depth;
        float x = 128.0f - w * 0.5f;
        Color col;
        if (y < 70) {
            col = (Color){14 + (int)(t * 20), 10 + (int)(t * 16),
                          32 + (int)(t * 30), 255};
        } else {
            float band = sinf(t * 38.0f + game->overworld_angle * 0.05f);
            int g = 60 + (int)(band * 18.0f);
            col = (Color){18, g, 34, 255};
            if (fabsf(t - 0.62f) < 0.02f || fabsf(t - 0.78f) < 0.025f)
                col = (Color){86, 74, 44, 255};
        }
        DrawRectangle((int)x, y, (int)w + 1, 1, col);
    }

    /* World map nodes */
    static const struct { int node; float x; float y; const char *label; } kNodes[] = {
        {0, 40.0f, 168.0f, "COLOSSEUM"},
        {1, 96.0f, 176.0f, "GRAVEYARD"},
        {2, 144.0f, 184.0f, "CATACOMBS"},
        {3, 192.0f, 176.0f, "RUINS"},
        {4, 64.0f, 200.0f, "LAKE"},
        {5, 224.0f, 160.0f, "DEMON KEEP"},
    };
    for (int i = 0; i < 6; i++) {
        bool active = game->overworld_node == kNodes[i].node;
        Color col = active ? (Color){255, 220, 90, 255}
                           : (Color){150, 140, 120, 255};
        if (kNodes[i].node == 5 &&
            (game->stages_cleared & 0x0F) != 0x0F)
            col = (Color){120, 70, 90, 255};
        DrawCircle((int)kNodes[i].x, (int)kNodes[i].y, active ? 5 : 3, col);
        if (active)
            DrawText(kNodes[i].label,
                     (int)kNodes[i].x - MeasureText(kNodes[i].label, 10) / 2,
                     (int)kNodes[i].y + 8, 10, (Color){255, 240, 200, 255});
    }

    /* Firebrand marker */
    DrawCircle((int)game->overworld_x, (int)game->overworld_y, 4,
               (Color){240, 90, 60, 255});
    DrawTriangle((Vector2){game->overworld_x - 4, game->overworld_y - 8},
                 (Vector2){game->overworld_x + 4, game->overworld_y - 8},
                 (Vector2){game->overworld_x, game->overworld_y - 2},
                 (Color){255, 200, 120, 255});

    DrawRectangle(0, 0, 256, 18, (Color){0, 0, 0, 170});
    DrawText("OVERWORLD", 6, 5, 10, (Color){235, 225, 195, 255});
    DrawText(TextFormat("GOLD %d", game->player.gold), 170, 5, 10,
             (Color){235, 210, 120, 255});
    DrawCentered("ARROWS FLY   ENTER LAND   C SHOP", 208, 10,
                 (Color){200, 200, 200, 255});
}

static void DrawSubscreen(const Game *game) {
    const Player *p = &game->player;
    DrawRectangle(0, 0, 256, 224, (Color){8, 8, 18, 235});
    DrawText("CREST / ITEM SUBSCREEN", 12, 8, 10,
             (Color){230, 210, 150, 255});
    DrawText(TextFormat("FORM: %s", Player_FormName(p->crest_form)), 12, 28,
             10, (Color){200, 230, 255, 255});
    DrawText(TextFormat("FIRE: %s", kFireNames[p->fire_element]), 12, 44, 10,
             (Color){255, 180, 120, 255});
    DrawText(TextFormat("TALISMAN: %s", kTalismanNames[p->active_talisman]),
             12, 60, 10, (Color){200, 255, 200, 255});
    DrawText(TextFormat("HP %d/%d", p->hp, p->max_hp), 12, 80, 10,
             (Color){240, 120, 140, 255});
    DrawText(TextFormat("URNS %d/5   SPELLS %d/5   GOLD %d", p->potions,
                        p->scrolls, p->gold), 12, 96, 10,
             (Color){240, 220, 140, 255});
    DrawText("LEFT/RIGHT FORM  A FIRE  Y TALISMAN", 12, 128, 10,
             (Color){190, 190, 190, 255});
    DrawText("Z USE URN  Q CAST SPELL  TAB CLOSE", 12, 144, 10,
             (Color){190, 190, 190, 255});
    DrawText("OWNED CRESTS:", 12, 172, 10, (Color){220, 220, 220, 255});
    for (int i = 0; i < CREST_COUNT; i++) {
        Color c = (p->crests_mask & (1u << i)) ? (Color){230, 220, 160, 255}
                                               : (Color){70, 70, 80, 255};
        DrawRectangle(14 + i * 24, 188, 18, 10, c);
        if (i == p->crest_form)
            DrawRectangleLines(13 + i * 24, 187, 20, 12,
                               (Color){255, 255, 255, 255});
    }
}

static void DrawOverlay(const Game *game) {
    static const char *const kMenu[] = {
        "NEW GAME", "OVERWORLD MAP", "PASSWORD", "OPTIONS"
    };
    static const char *const kMenuWithSave[] = {
        "CONTINUE", "NEW GAME", "OVERWORLD MAP", "PASSWORD", "OPTIONS"
    };
    if (game->state == GAME_TITLE) {
        DrawRectangle(0, 0, 256, 224, (Color){0, 0, 0, 185});
        DrawCentered("DEMON'S CREST", 34, 24, (Color){238, 232, 214, 255});
        DrawCentered("NATIVE FAN PORT", 62, 10, (Color){196, 168, 92, 255});
        const char *const *menu = game->has_save ? kMenuWithSave : kMenu;
        const int count = game->has_save ? 5 : 4;
        for (int i = 0; i < count; i++) {
            Color col = game->menu_cursor == i ? (Color){255, 230, 120, 255}
                                               : (Color){190, 190, 190, 255};
            DrawCentered(menu[i], 92 + i * 16, 10, col);
        }
        DrawCentered("ARROWS SELECT   ENTER CONFIRM", 186, 10,
                     (Color){150, 150, 150, 255});
        DrawCentered("F1 COLLISION   ESC PAUSE", 200, 10,
                     (Color){150, 150, 150, 255});
    } else if (game->state == GAME_PAUSED) {
        DrawRectangle(0, 0, 256, 224, (Color){0, 0, 0, 150});
        DrawCentered("PAUSED", 84, 24, (Color){238, 232, 214, 255});
        Color c0 = game->menu_cursor == 0 ? (Color){255, 230, 120, 255}
                                          : (Color){200, 200, 200, 255};
        Color c1 = game->menu_cursor == 1 ? (Color){255, 230, 120, 255}
                                          : (Color){200, 200, 200, 255};
        DrawCentered("RESUME", 124, 10, c0);
        DrawCentered("RETURN TO OVERWORLD", 140, 10, c1);
    } else if (game->state == GAME_CLEAR) {
        DrawRectangle(0, 0, 256, 224, (Color){0, 0, 0, 185});
        DrawCentered("AREA CLEAR", 62, 24, (Color){238, 232, 214, 255});
        DrawCentered(TextFormat("AREA %d CONQUERED", game->level.area_id), 96,
                     10, (Color){196, 168, 92, 255});
        DrawCentered(TextFormat("PASSWORD %s", game->password_code), 114, 10,
                     (Color){160, 220, 255, 255});
        DrawCentered(TextFormat("MAX HP %d   GOLD %d", game->player.max_hp,
                                game->player.gold), 132, 10,
                     (Color){240, 220, 140, 255});
        if ((game->state_timer / 30) % 2 == 0)
            DrawCentered("PRESS ENTER FOR OVERWORLD", 160, 10,
                         (Color){255, 255, 255, 255});
    } else if (game->state == GAME_SUBSCREEN) {
        DrawSubscreen(game);
    } else if (game->state == GAME_SHOP) {
        DrawRectangle(0, 0, 256, 224, (Color){10, 8, 14, 240});
        int shop_gold_y = 52;
        if (game->shop_welcome >= 0) {
            int h = Text_DrawPage(&game->assets, &game->script,
                                  game->shop_welcome, 0, 16, 16, 220,
                                  (Color){240, 236, 220, 255});
            shop_gold_y = 16 + h + 10;
        } else {
            DrawCentered("POTION & SPELL SHOP", 24, 14,
                         (Color){238, 220, 160, 255});
        }
        DrawCentered(TextFormat("GOLD %d", game->player.gold), shop_gold_y, 10,
                     (Color){255, 224, 120, 255});
        DrawCentered("LEFT  - BUY URN (30G)", 88, 10,
                     (Color){200, 220, 255, 255});
        DrawCentered("RIGHT - BUY SPELL (50G)", 104, 10,
                     (Color){200, 220, 255, 255});
        DrawCentered("UP    - LIFE STONE (80G)", 120, 10,
                     (Color){200, 220, 255, 255});
        DrawCentered(TextFormat("URNS %d/5  SPELLS %d/5  MAX HP %d",
                                game->player.potions, game->player.scrolls,
                                game->player.max_hp), 152, 10,
                     (Color){240, 220, 140, 255});
        if (game->shop_timer > 0 && game->shop_message >= 0) {
            Text_DrawPage(&game->assets, &game->script, game->shop_message, 0,
                          16, 150, 220, (Color){255, 210, 160, 255});
        }
        DrawCentered("ENTER / ESC RETURN", 208, 10,
                     (Color){170, 170, 170, 255});
    } else if (game->state == GAME_PASSWORD) {
        DrawRectangle(0, 0, 256, 224, (Color){6, 10, 16, 240});
        DrawCentered("PASSWORD", 40, 20, (Color){230, 220, 170, 255});
        int cw = 16;
        int start = 128 - (DC_PASSWORD_LEN * cw) / 2;
        for (int i = 0; i < DC_PASSWORD_LEN; i++) {
            char ch = game->password_entry[i];
            if (!ch)
                ch = '-';
            bool active = (i == game->password_pos);
            Color col = active ? (Color){255, 230, 120, 255}
                               : (Color){150, 230, 255, 255};
            DrawText(TextFormat("%c", ch), start + i * cw + 5, 86, 20, col);
            if (active)
                DrawRectangleLines(start + i * cw, 84, cw, 26, col);
        }
        if (game->password_status < 0)
            DrawCentered("INVALID CODE", 124, 12, (Color){255, 120, 120, 255});
        else if (game->password_status > 0)
            DrawCentered("PROGRESS RESTORED", 124, 12,
                         (Color){150, 255, 170, 255});
        DrawCentered("UP/DOWN LETTER   LEFT/RIGHT SELECT", 160, 10,
                     (Color){190, 190, 190, 255});
        DrawCentered("ENTER APPLY   ESC BACK", 178, 10,
                     (Color){190, 190, 190, 255});
    } else if (game->state == GAME_OPTIONS) {
        DrawRectangle(0, 0, 256, 224, (Color){8, 8, 14, 240});
        DrawCentered("OPTIONS", 40, 20, (Color){230, 220, 170, 255});
        DrawCentered(TextFormat("VOLUME %d%%", (int)(Audio_GetVolume() * 100)),
                     96, 12, (Color){200, 230, 255, 255});
        DrawCentered("LEFT/RIGHT ADJUST", 120, 10,
                     (Color){190, 190, 190, 255});
        DrawCentered("ENTER / ESC BACK", 168, 10,
                     (Color){190, 190, 190, 255});
    } else if (game->state == GAME_OVER) {
        DrawRectangle(0, 0, 256, 224, (Color){16, 0, 4, 230});
        DrawCentered("GAME OVER", 76, 24, (Color){240, 90, 90, 255});
        DrawCentered("THE DEMON'S CREST IS LOST", 108, 10,
                     (Color){200, 160, 160, 255});
        if ((game->state_timer / 30) % 2 == 0)
            DrawCentered("PRESS ENTER TO CONTINUE", 148, 10,
                         (Color){255, 255, 255, 255});
    } else if (game->state == GAME_DIALOGUE) {
        DrawRectangle(0, 0, 256, 224, (Color){4, 6, 12, 235});
        if (game->dialogue_text >= 0)
            Text_DrawPage(&game->assets, &game->script, game->dialogue_text,
                          game->dialogue_page, 16, 64, 224,
                          (Color){240, 236, 220, 255});
        if ((game->state_timer / 30) % 2 == 0)
            DrawCentered("PRESS ENTER", 196, 10,
                         (Color){200, 200, 200, 255});
    } else if (game->state == GAME_ENDING) {
        DrawRectangle(0, 0, 256, 224, (Color){4, 6, 12, 240});
        DrawCentered("THE DEMON'S CREST", 24, 18,
                     (Color){240, 220, 150, 255});
        if (game->ending_text >= 0 && game->script_ready) {
            /* Real ending narration ($81:8DF2 pages). */
            Text_DrawPage(&game->assets, &game->script, game->ending_text,
                          game->ending_page, 16, 56, 224,
                          (Color){240, 236, 220, 255});
        } else {
            DrawCentered("THE PHALANX IS BROKEN", 84, 10,
                         (Color){200, 220, 255, 255});
        }
        DrawCentered(TextFormat("MAX HP %d   GOLD %d", game->player.max_hp,
                                game->player.gold), 184, 10,
                     (Color){240, 220, 140, 255});
        if ((game->state_timer / 30) % 2 == 0)
            DrawCentered("PRESS ENTER", 206, 10, (Color){200, 200, 200, 255});
    }
}

void Game_Render(Game *game) {
    BeginTextureMode(game->target);
    bool show_hud = game->state == GAME_PLAYING ||
                    game->state == GAME_PAUSED ||
                    game->state == GAME_SUBSCREEN;
    if (game->state == GAME_OVERWORLD) {
        DrawOverworld(game);
    } else {
        Render_Game(&game->assets, &game->level, &game->camera,
                    &game->player, &game->projectiles, &game->objects,
                    show_hud, game->input.debug_collision);
    }
    if (game->state != GAME_PLAYING)
        DrawOverlay(game);
    if (game->dialogue_demo && game->script_ready && game->script.count > 0) {
        /* Dev hook (`DC_DIALOGUE=1`): show the extracted ROM script with the
         * baked BG3 font, advancing a page every two seconds. The real
         * dialogue state will drive this from story events instead. */
        int pages = game->script.page_count[0];
        int page = pages > 0 ? (game->state_timer / 120) % pages : 0;
        int height = 0;
        if (getenv("DC_FONT_TEST")) {
            Text_DrawString(&game->assets, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 8, 120,
                            (Color){240, 236, 220, 255});
            Text_DrawString(&game->assets, "abcdefghijklmnopqrstuvwxyz", 8, 130,
                            (Color){240, 236, 220, 255});
            Text_DrawString(&game->assets, "0123456789 .,:;!?-'", 8, 140,
                            (Color){240, 236, 220, 255});
            height = 24;
        } else {
            height = Text_DrawPage(&game->assets, &game->script, 0, page, 16,
                                   150, 216, (Color){240, 236, 220, 255});
        }
        if (height > 0)
            Text_DrawString(&game->assets, "ENTER NEXT", 16,
                            150 + height + 6, (Color){170, 170, 185, 255});
    }
    EndTextureMode();
}

void Game_Shutdown(Game *game) {
    if (game->ready && game->campaign_mode)
        (void)Game_SaveProgress(game);
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
