#ifndef SPIRIT_AUDIO_H
#define SPIRIT_AUDIO_H
#include "combat.h"

typedef enum {
    SFX_WHOOSH_LIGHT,
    SFX_WHOOSH_HEAVY,
    SFX_HIT_LIGHT,
    SFX_HIT_HEAVY,
    SFX_HIT_SUPER,
    SFX_BLOCK,
    SFX_COUNTER,
    SFX_GUARD_BREAK,
    SFX_LAUNCH,
    SFX_WALL,
    SFX_KNOCKDOWN,
    SFX_LAND,
    SFX_JUMP,
    SFX_DASH,
    SFX_FIRE,
    SFX_CLASH,
    SFX_SUPER_FLASH,
    SFX_KO,
    SFX_ROUND,
    SFX_FIGHT,
    SFX_PERFECT,
    SFX_THUNDER,
    SFX_UI_MOVE,
    SFX_UI_SELECT,
    SFX_UI_BACK,
    SFX_COUNT
} SfxId;

typedef enum { MUSIC_NONE, MUSIC_MENU, MUSIC_BATTLE } MusicTrack;

#define SFX_VOICES 3
typedef struct {
    Sound effects[SFX_COUNT][SFX_VOICES];
    int next[SFX_COUNT];
    AudioStream stream;
    bool ready, muted, streaming;
} GameAudio;

void GameAudioInit(GameAudio *audio);
void GameAudioPlay(GameAudio *audio, const GameEvent *events, int count);
void GameAudioSfx(GameAudio *audio, SfxId id);
void GameAudioMusic(GameAudio *audio, MusicTrack track);
void GameAudioDuck(GameAudio *audio, float level); // 1 = full music volume
void GameAudioMute(GameAudio *audio);
void GameAudioClose(GameAudio *audio);
#endif
