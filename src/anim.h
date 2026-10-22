#ifndef DC_ANIM_H
#define DC_ANIM_H

#include <stdbool.h>

#include "raylib.h"

#define DC_ANIM_MAX_STEPS 64

typedef enum AnimClipId {
    ANIM_IDLE = 0,
    ANIM_WALK,
    ANIM_JUMP,
    ANIM_HOVER,
    ANIM_FALL,
    ANIM_LAND,
    ANIM_ATTACK,
    ANIM_CLIP_COUNT
} AnimClipId;

typedef struct AnimStep {
    Texture2D texture;
    int anchor_x;   /* pixels from the image left edge to the body centre */
    int anchor_y;   /* pixels from the image top edge to the feet */
    int duration;   /* game frames */
} AnimStep;

typedef struct AnimClip {
    int first_step;
    int step_count;
    bool loop;
} AnimClip;

typedef struct PlayerAnim {
    AnimStep steps[DC_ANIM_MAX_STEPS];
    int step_count;
    AnimClip clips[ANIM_CLIP_COUNT];
    bool ready;
} PlayerAnim;

bool Anim_Load(PlayerAnim *anim, const char *dir);
void Anim_Unload(PlayerAnim *anim);
const AnimStep *Anim_Step(const PlayerAnim *anim, int clip, int index);

#endif
