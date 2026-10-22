#ifndef DC_INPUT_H
#define DC_INPUT_H

#include <stdbool.h>

typedef struct Input {
    bool left;
    bool right;
    bool up;
    bool down;
    bool jump_pressed;
    bool jump_held;
    bool attack_pressed;
    bool debug_collision;
} Input;

typedef struct InputState {
    bool prev_jump;
    bool prev_attack;
    bool prev_debug;
} InputState;

void Input_Read(Input *input, InputState *state);

#endif
