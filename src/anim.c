#include "anim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

static const char *const kClipNames[ANIM_CLIP_COUNT] = {
    "idle", "walk", "jump", "hover", "fall", "land", "attack"
};

static char *ReadText(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *text = malloc((size_t)size + 1);
    if (!text) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)size, f);
    fclose(f);
    text[got] = '\0';
    return text;
}

static int ReadInt(cJSON *obj, const char *key, int *out) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(v))
        return 0;
    *out = v->valueint;
    return 1;
}

void Anim_Unload(PlayerAnim *anim) {
    for (int i = 0; i < anim->step_count; i++) {
        Texture2D tex = anim->steps[i].texture;
        if (!IsTextureValid(tex))
            continue;
        bool seen = false;
        for (int j = 0; j < i; j++) {
            if (anim->steps[j].texture.id == tex.id) {
                seen = true;
                break;
            }
        }
        if (!seen)
            UnloadTexture(tex);
    }
    memset(anim, 0, sizeof(*anim));
}

bool Anim_Load(PlayerAnim *anim, const char *dir) {
    memset(anim, 0, sizeof(*anim));
    char path[1024];
    snprintf(path, sizeof(path), "%s/player_anim.json", dir);
    char *text = ReadText(path);
    if (!text)
        return false;

    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root)
        return false;

    cJSON *clips = cJSON_GetObjectItemCaseSensitive(root, "clips");
    if (!cJSON_IsObject(clips)) {
        cJSON_Delete(root);
        return false;
    }

    bool ok = true;
    for (int clip_id = 0; clip_id < ANIM_CLIP_COUNT && ok; clip_id++) {
        cJSON *clip = cJSON_GetObjectItemCaseSensitive(clips,
                                                       kClipNames[clip_id]);
        if (!clip)
            continue;
        cJSON *steps = cJSON_GetObjectItemCaseSensitive(clip, "steps");
        if (!cJSON_IsArray(steps)) {
            ok = false;
            break;
        }
        AnimClip *dst = &anim->clips[clip_id];
        dst->first_step = anim->step_count;
        dst->loop = cJSON_IsTrue(
            cJSON_GetObjectItemCaseSensitive(clip, "loop"));
        int count = cJSON_GetArraySize(steps);
        if (count <= 0 || anim->step_count + count > DC_ANIM_MAX_STEPS) {
            ok = false;
            break;
        }
        for (int i = 0; i < count && ok; i++) {
            cJSON *item = cJSON_GetArrayItem(steps, i);
            cJSON *tex = cJSON_GetObjectItemCaseSensitive(item, "texture");
            AnimStep *step = &anim->steps[anim->step_count];
            if (!cJSON_IsString(tex) ||
                !ReadInt(item, "anchor_x", &step->anchor_x) ||
                !ReadInt(item, "anchor_y", &step->anchor_y)) {
                ok = false;
                break;
            }
            if (!ReadInt(item, "duration", &step->duration) ||
                step->duration < 1)
                step->duration = 1;

            char tex_path[1024];
            snprintf(tex_path, sizeof(tex_path), "%s/%s", dir,
                     tex->valuestring);
            step->texture = LoadTexture(tex_path);
            if (!IsTextureValid(step->texture)) {
                fprintf(stderr, "anim: failed to load %s\n", tex_path);
                ok = false;
                break;
            }
            SetTextureFilter(step->texture, TEXTURE_FILTER_POINT);
            anim->step_count++;
        }
        dst->step_count = count;
    }

    cJSON_Delete(root);
    if (!ok) {
        Anim_Unload(anim);
        return false;
    }
    anim->ready = anim->step_count > 0;
    return anim->ready;
}

const AnimStep *Anim_Step(const PlayerAnim *anim, int clip, int index) {
    if (!anim->ready || clip < 0 || clip >= ANIM_CLIP_COUNT)
        return NULL;
    const AnimClip *c = &anim->clips[clip];
    if (c->step_count <= 0)
        return NULL;
    if (index < 0)
        index = 0;
    if (index >= c->step_count)
        index = c->step_count - 1;
    return &anim->steps[c->first_step + index];
}
