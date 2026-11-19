#ifndef SPIRIT_FX_H
#define SPIRIT_FX_H
#include "combat.h"

// Presentation layer for "game feel": a follow camera with trauma shake and zoom punches,
// slow motion, flashes, particles, shockwaves, afterimages and popups. It consumes the
// simulation's events and never writes back into the simulation.

#define FX_PARTICLES 900
#define FX_RINGS 32
#define FX_STARS 24
#define FX_POPUPS 10
#define FX_GHOSTS 40
#define FX_STREAKS 48

typedef enum { PK_SPARK, PK_EMBER, PK_DUST, PK_SHARD, PK_PETAL, PK_SPLASH, PK_BOLT } ParticleKind;

typedef struct {
    Vector2 pos, vel;
    float life, maxLife, size, angle, spin, drag, gravity;
    Color color;
    unsigned char kind;
    bool additive;
} FxParticle;

typedef struct {
    Vector2 pos;
    float life, maxLife, radius, thickness;
    Color color;
} FxRing;

typedef struct {
    Vector2 pos;
    float life, maxLife, size, angle;
    Color color;
} FxStar;

typedef struct {
    Vector2 pos, dir;
    float life, maxLife, length, width;
    Color color;
} FxStreak;

typedef struct {
    char text[28];
    Vector2 pos;
    float life, maxLife, size;
    Color color;
} FxPopup;

typedef struct {
    Fighter snapshot;
    int index;
    float life, maxLife;
    Color color;
} FxGhost;

typedef struct {
    FxParticle particles[FX_PARTICLES];
    FxRing rings[FX_RINGS];
    FxStar stars[FX_STARS];
    FxStreak streaks[FX_STREAKS];
    FxPopup popups[FX_POPUPS];
    FxGhost ghosts[FX_GHOSTS];
    float time, trauma, zoomPunch, flash, slowmo, superTime, fightTime, lightning, thunderDelay;
    Color flashColor;
    Vector2 camCenter, punchFocus, koFocus;
    float camZoom;
    int superOwner;
    float ghostTimer[2], auraTimer[2];
    // Combo counter presentation persists after the sim's combo ends.
    int comboShown[2];
    float comboTimer[2], comboPop[2], comboDamage[2];
    bool thunder; // set for one frame when a storm thunderclap should play
    uint32_t seed;
} Fx;

void FxReset(Fx *fx);
void FxEvents(Fx *fx, const Game *game, const GameEvent *events, int count);
void FxUpdate(Fx *fx, const Game *game, float dt, bool reducedMotion);
float FxTimeScale(const Fx *fx);
Camera2D FxCamera(const Fx *fx, float density, bool reducedMotion);
void FxDrawWorld(const Fx *fx, bool front);
void FxDrawScreen(const Fx *fx, const Game *game, bool reducedMotion);
void FxBurst(Fx *fx, Vector2 pos, Color color, int count, float force, ParticleKind kind);
Color FighterColor(const Game *game, int index);
float FxRandom(Fx *fx);
#endif
