#include "audio.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#define MUSIC_SR 22050
#define MUSIC_STEP_SECONDS 0.25
#define MUSIC_STEPS 64
#define MAX_SFX_FRAMES 22050

static bool ready;
static Sound sfx[SFX_COUNT];
static Music music;
static unsigned char *music_wav;
static bool music_loaded;

static void Put16(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)(v >> 8);
}

static void Put32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static unsigned char *WrapWav(const int16_t *pcm, int frames, int *size_out) {
    int data_bytes = frames * 2;
    int size = 44 + data_bytes;
    unsigned char *wav = malloc((size_t)size);
    if (!wav)
        return NULL;
    memcpy(wav, "RIFF", 4);
    Put32(wav + 4, (uint32_t)(36 + data_bytes));
    memcpy(wav + 8, "WAVEfmt ", 8);
    Put32(wav + 16, 16);
    Put16(wav + 20, 1);       /* PCM */
    Put16(wav + 22, 1);       /* mono */
    Put32(wav + 24, MUSIC_SR);
    Put32(wav + 28, MUSIC_SR * 2);
    Put16(wav + 32, 2);
    Put16(wav + 34, 16);
    memcpy(wav + 36, "data", 4);
    Put32(wav + 40, (uint32_t)data_bytes);
    for (int i = 0; i < frames; i++)
        Put16(wav + 44 + i * 2, (uint16_t)pcm[i]);
    *size_out = size;
    return wav;
}

static float NoteHz(int midi) {
    return 440.0f * powf(2.0f, (float)(midi - 69) / 12.0f);
}

static int16_t Square(float phase, float amp) {
    return (int16_t)((phase < 0.5f ? amp : -amp) * 32767.0f);
}

static void BuildMusic(void) {
    static const int melody[MUSIC_STEPS] = {
        69, 72, 76, 72, 69, 72, 76, 79,
        65, 69, 72, 69, 65, 69, 72, 76,
        67, 71, 74, 71, 67, 71, 74, 79,
        64, 67, 71, 67, 64, 67, 71, 74,
        69, 72, 76, 79, 76, 72, 69, 65,
        65, 69, 72, 76, 72, 69, 65, 62,
        67, 71, 74, 79, 74, 71, 67, 64,
        64, 67, 71, 74, 71, 67, 64, 57
    };
    static const int bass[16] = {
        45, 45, 41, 41, 43, 43, 40, 40,
        45, 45, 41, 41, 43, 43, 40, 40
    };

    int step_frames = (int)(MUSIC_SR * MUSIC_STEP_SECONDS);
    int frames = step_frames * MUSIC_STEPS;
    int16_t *pcm = calloc((size_t)frames, sizeof(int16_t));
    if (!pcm)
        return;

    for (int s = 0; s < MUSIC_STEPS; s++) {
        float mf = NoteHz(melody[s]);
        int b = (s / 4) % 16;
        float bf = NoteHz(bass[b]);
        for (int i = 0; i < step_frames; i++) {
            float t = (float)i / MUSIC_SR;
            float env = 1.0f - (float)i / step_frames;
            int16_t m = Square(fmodf(t * mf, 1.0f), 0.18f * env);
            int16_t bass_note = Square(fmodf(t * bf, 1.0f), 0.22f);
            int mix = (int)m + (int)bass_note;
            if (mix > 32767) mix = 32767;
            if (mix < -32768) mix = -32768;
            pcm[s * step_frames + i] = (int16_t)mix;
        }
    }

    int size = 0;
    music_wav = WrapWav(pcm, frames, &size);
    free(pcm);
    if (!music_wav)
        return;
    music = LoadMusicStreamFromMemory(".wav", music_wav, size);
    if (IsMusicValid(music)) {
        music.looping = true;
        music_loaded = true;
    }
}

static Sound MakeSfx(float seconds, float f0, float f1, float noise,
                     float amp) {
    int frames = (int)(MUSIC_SR * seconds);
    if (frames > MAX_SFX_FRAMES)
        frames = MAX_SFX_FRAMES;
    int16_t *pcm = calloc((size_t)frames, sizeof(int16_t));
    if (!pcm)
        return (Sound){0};
    unsigned seed = 0x12345678u;
    for (int i = 0; i < frames; i++) {
        float u = (float)i / frames;
        float freq = f0 + (f1 - f0) * u;
        float t = (float)i / MUSIC_SR;
        float env = 1.0f - u;
        seed = seed * 1664525u + 1013904223u;
        float n = ((int)(seed >> 8) % 2001 - 1000) / 1000.0f;
        float tone = (fmodf(t * freq, 1.0f) < 0.5f) ? 1.0f : -1.0f;
        float v = tone * (1.0f - noise) + n * noise;
        int sample = (int)(v * env * amp * 32767.0f);
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        pcm[i] = (int16_t)sample;
    }
    Wave wave = {0};
    wave.frameCount = (unsigned int)frames;
    wave.sampleRate = MUSIC_SR;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = pcm;
    Sound sound = LoadSoundFromWave(wave);
    free(pcm);
    return sound;
}

bool Audio_Init(void) {
    ready = false;
    if ((getenv("DC_NO_AUDIO") || getenv("DC_RUN_FRAMES")) &&
        !getenv("DC_AUDIO_FORCE"))
        return false;
    if (getenv("DC_AUDIO_DEBUG"))
        fprintf(stderr, "audio: init start\n");
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        if (getenv("DC_AUDIO_DEBUG"))
            fprintf(stderr, "audio: device not ready; running silent\n");
        return false;
    }

    sfx[SFX_JUMP] = MakeSfx(0.12f, 300.0f, 620.0f, 0.05f, 0.35f);
    sfx[SFX_LAND] = MakeSfx(0.09f, 180.0f, 70.0f, 0.45f, 0.30f);
    sfx[SFX_SHOOT] = MakeSfx(0.11f, 900.0f, 260.0f, 0.25f, 0.30f);
    sfx[SFX_BREAK] = MakeSfx(0.16f, 220.0f, 90.0f, 0.75f, 0.35f);
    sfx[SFX_PICKUP] = MakeSfx(0.22f, 660.0f, 1320.0f, 0.0f, 0.30f);
    sfx[SFX_HURT] = MakeSfx(0.20f, 160.0f, 55.0f, 0.25f, 0.35f);
    sfx[SFX_ENEMY_HIT] = MakeSfx(0.08f, 500.0f, 220.0f, 0.4f, 0.28f);

    BuildMusic();
    ready = true;
    if (getenv("DC_AUDIO_DEBUG"))
        fprintf(stderr, "audio: device ready, music %s\n",
                music_loaded ? "loaded" : "unavailable");
    return true;
}

void Audio_Shutdown(void) {
    if (!ready) {
        if (IsAudioDeviceReady())
            CloseAudioDevice();
        return;
    }
    for (int i = 0; i < SFX_COUNT; i++) {
        if (IsSoundValid(sfx[i]))
            UnloadSound(sfx[i]);
    }
    if (music_loaded)
        UnloadMusicStream(music);
    free(music_wav);
    music_wav = NULL;
    music_loaded = false;
    CloseAudioDevice();
    ready = false;
}

void Audio_Update(void) {
    if (ready && music_loaded)
        UpdateMusicStream(music);
}

void Audio_Play(SfxId id) {
    if (!ready || id < 0 || id >= SFX_COUNT)
        return;
    if (IsSoundValid(sfx[id]))
        PlaySound(sfx[id]);
}

void Audio_StartMusic(void) {
    if (!ready || !music_loaded)
        return;
    if (!IsMusicStreamPlaying(music))
        PlayMusicStream(music);
}
