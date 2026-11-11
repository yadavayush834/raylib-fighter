#include "audio.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static Sound SynthEffect(SoundEvent type) {
    const int rate = 22050;
    float duration = type == EVENT_BURST ? .32f : type == EVENT_KO ? .65f : .15f;
    unsigned frames = (unsigned)(duration * rate);
    short *samples = malloc(frames * sizeof(*samples));
    if (!samples)
        return (Sound){0};
    uint32_t noise = 19381;
    float phase = 0;
    for (unsigned i = 0; i < frames; ++i) {
        float t = (float)i / rate;
        float progress = t / duration;
        float envelope = powf(1 - progress, 2) * fminf(1, t * 700);
        float frequency = type == EVENT_BLOCK   ? 900
                          : type == EVENT_HEAVY ? 100
                          : type == EVENT_BURST ? 380
                          : type == EVENT_DASH  ? 170
                                                : 230;
        if (type == EVENT_KO)
            frequency = progress < .33f ? 220 : progress < .66f ? 277 : 330;
        else
            frequency *= 1 - progress * .7f;
        phase += 2 * PI * frequency / rate;
        noise = noise * 1664525u + 1013904223u;
        float random = ((float)((noise >> 8) & 0xffffu) / 32767.5f) - 1;
        float tone = sinf(phase) * .6f + random * (type == EVENT_DASH ? .7f : .25f);
        samples[i] = (short)(tone * envelope * 14000);
    }
    Wave wave = {.frameCount = frames, .sampleRate = rate, .sampleSize = 16, .channels = 1, .data = samples};
    Sound result = LoadSoundFromWave(wave);
    free(samples);
    return result;
}
void GameAudioInit(GameAudio *a) {
    memset(a, 0, sizeof(*a));
    InitAudioDevice();
    a->ready = IsAudioDeviceReady();
    if (!a->ready)
        return;
    for (int i = 0; i < EVENT_COUNT; ++i)
        a->effects[i] = SynthEffect((SoundEvent)i);
    SetMasterVolume(.4f);
}
void GameAudioPlay(GameAudio *a, unsigned events) {
    if (!a->ready || a->muted)
        return;
    for (int i = 0; i < EVENT_COUNT; ++i)
        if ((events & (1u << i)) && a->effects[i].frameCount)
            PlaySound(a->effects[i]);
}
void GameAudioMute(GameAudio *a) {
    a->muted = !a->muted;
    if (a->ready)
        SetMasterVolume(a->muted ? 0 : .4f);
}
void GameAudioClose(GameAudio *a) {
    if (!a->ready)
        return;
    for (int i = 0; i < EVENT_COUNT; ++i)
        if (a->effects[i].frameCount)
            UnloadSound(a->effects[i]);
    CloseAudioDevice();
}
