#include "input.h"

#include <stdlib.h>

#include "raylib.h"

static bool PadDown(int button) {
    return IsGamepadAvailable(0) && IsGamepadButtonDown(0, button);
}

void Input_Read(Input *in, InputState *st) {
    static int demo_mode = -1;
    static int demo_frame = 0;
    if (demo_mode < 0) {
        const char *env = getenv("DC_DEMO");
        demo_mode = env ? atoi(env) : 0;
    }

    bool left = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A) ||
                PadDown(GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    bool right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D) ||
                 PadDown(GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    bool up = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W) ||
              PadDown(GAMEPAD_BUTTON_LEFT_FACE_UP);
    bool down = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S) ||
                PadDown(GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    bool jump = IsKeyDown(KEY_Z) || IsKeyDown(KEY_SPACE) ||
                PadDown(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);  /* SNES B */
    bool attack = IsKeyDown(KEY_X) || IsKeyDown(KEY_J) ||
                  PadDown(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);   /* SNES A */
    bool debug = IsKeyDown(KEY_F1);

    /* Deterministic input for screenshots/regressions. */
    if (demo_mode == 1) {
        demo_frame++;
        right = demo_frame > 30 && demo_frame < 210;
        jump = (demo_frame >= 90 && demo_frame < 130) ||
               (demo_frame >= 160 && demo_frame < 200);
        up = demo_frame >= 165 && demo_frame < 200;
    } else if (demo_mode == 2) {
        demo_frame++;
        right = demo_frame > 30 && demo_frame < 90;
        jump = demo_frame >= 90 && demo_frame < 120;
    } else if (demo_mode == 3) {
        /* Mirrors tools/oracle/oracle_physics.script after its 600-frame
         * landing settle: idle 120, right 30, left 30, jump, jump-hold,
         * moving jump. */
        demo_frame++;
        right = (demo_frame >= 120 && demo_frame < 150) ||
                (demo_frame >= 488 && demo_frame < 546);
        left = demo_frame >= 210 && demo_frame < 240;
        jump = (demo_frame >= 300 && demo_frame < 308) ||
               (demo_frame >= 368 && demo_frame < 428) ||
               (demo_frame >= 488 && demo_frame < 496);
        up = demo_frame >= 488 && demo_frame < 496;
    } else if (demo_mode == 4) {
        /* Tap-jump vs hold-jump comparison. */
        demo_frame++;
        jump = (demo_frame >= 30 && demo_frame < 31) ||
               (demo_frame >= 100 && demo_frame < 130);
    }

    in->left = left;
    in->right = right;
    in->up = up;
    in->down = down;
    in->jump_held = jump;
    in->jump_pressed = jump && !st->prev_jump;
    in->attack_pressed = attack && !st->prev_attack;
    in->debug_collision = debug;

    st->prev_jump = jump;
    st->prev_attack = attack;
    st->prev_debug = debug;
}
