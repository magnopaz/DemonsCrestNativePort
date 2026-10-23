#ifndef DC_AUDIO_H
#define DC_AUDIO_H

#include <stdbool.h>

typedef enum SfxId {
    SFX_JUMP = 0,
    SFX_LAND,
    SFX_SHOOT,
    SFX_BREAK,
    SFX_PICKUP,
    SFX_HURT,
    SFX_ENEMY_HIT,
    SFX_COUNT
} SfxId;

bool Audio_Init(void);
void Audio_Shutdown(void);
void Audio_Update(void);
void Audio_Play(SfxId id);
void Audio_StartMusic(void);

#endif
