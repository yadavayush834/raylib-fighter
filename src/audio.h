#ifndef SPIRIT_AUDIO_H
#define SPIRIT_AUDIO_H
#include "combat.h"
typedef struct {
    Sound effects[EVENT_COUNT];
    bool ready, muted;
} GameAudio;
void GameAudioInit(GameAudio *audio);
void GameAudioPlay(GameAudio *audio, unsigned events);
void GameAudioMute(GameAudio *audio);
void GameAudioClose(GameAudio *audio);
#endif
