#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "raylib.h"

static const char *FindAssetDir(int argc, char **argv) {
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--assets") == 0)
            return argv[i + 1];
    }
    return "assets/generated";
}

int main(int argc, char **argv) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1024, 896, "Demon's Crest - Fan Game");
    if (!IsWindowReady()) {
        fprintf(stderr, "main: InitWindow failed (check DISPLAY)\n");
        return 1;
    }
    SetWindowMinSize(512, 448);
    SetTargetFPS(60);

    Game game;
    if (!Game_Init(&game, FindAssetDir(argc, argv))) {
        fprintf(stderr, "main: Game_Init failed\n");
        CloseWindow();
        return 1;
    }

    const char *run_frames_env = getenv("DC_RUN_FRAMES");
    int run_limit = run_frames_env ? atoi(run_frames_env) : 0;
    const char *shot = getenv("DC_SCREENSHOT");
    int frames = 0;

    while (run_limit > 0 ? (frames < run_limit) : !WindowShouldClose()) {
        Game_Update(&game);
        Game_Render(&game);
        BeginDrawing();
        ClearBackground((Color){0, 0, 0, 255});

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int scale = sw / 256;
        if (sh / 224 < scale)
            scale = sh / 224;
        if (scale < 1)
            scale = 1;
        int dw = 256 * scale;
        int dh = 224 * scale;
        Rectangle dst = {
            (float)((sw - dw) / 2),
            (float)((sh - dh) / 2),
            (float)dw,
            (float)dh
        };
        Rectangle src = {0, 0, 256, -224};
        DrawTexturePro(game.target.texture, src, dst, (Vector2){0, 0}, 0.0f,
                       WHITE);
        if (IsKeyDown(KEY_F3))
            DrawFPS(8, 8);
        EndDrawing();

        frames++;
        if (shot && *shot && run_limit > 0 && frames >= run_limit) {
            TakeScreenshot(shot);
            break;
        }
    }

    Game_Shutdown(&game);
    CloseWindow();
    return 0;
}
