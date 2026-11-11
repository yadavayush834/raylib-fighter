#ifndef SPIRIT_COMBAT_H
#define SPIRIT_COMBAT_H
#include "raylib.h"
#include <stdint.h>

#define ARENA_W 1280.0f
#define ARENA_H 720.0f
#define GROUND_Y 590.0f
#define MAX_HEALTH 1000.0f
#define MAX_PARTICLES 200
#define MAX_PROJECTILES 8
#define FIXED_DT (1.0f / 120.0f)

typedef enum { ACTION_IDLE, ACTION_LIGHT, ACTION_HEAVY, ACTION_SPECIAL, ACTION_DASH } Action;
typedef enum { ROUND_INTRO, ROUND_FIGHT, ROUND_END, MATCH_END } Phase;
typedef enum { MODE_SOLO, MODE_LOCAL, MODE_TRAINING, MODE_DEMO } MatchMode;
typedef enum {
    EVENT_LIGHT,
    EVENT_HEAVY,
    EVENT_BLOCK,
    EVENT_BURST,
    EVENT_DASH,
    EVENT_KO,
    EVENT_COUNT
} SoundEvent;

typedef struct {
    float move;
    bool jump, light, heavy, special, dash, block;
} FighterInput;

typedef struct {
    Vector2 position, velocity;
    float health, healthTrail, energy;
    float stun, invincible, actionTime, comboTime, bufferTime;
    float hurtDuration;
    int hurtDirection;
    int facing, wins, combo;
    Action action, bufferedAction;
    bool grounded, blocking, connected;
} Fighter;

typedef struct {
    Vector2 position, velocity;
    float life, maxLife, radius;
    Color color;
} Particle;
typedef struct {
    Vector2 position;
    float velocity, life;
    int owner;
    bool active;
} Projectile;
typedef struct {
    float decision, block, jumpCooldown;
    float move;
} Brain;

typedef struct {
    Fighter fighters[2];
    Projectile projectiles[MAX_PROJECTILES];
    Particle particles[MAX_PARTICLES];
    Brain brains[2];
    Phase phase;
    MatchMode mode;
    float roundTime, phaseTime, totalTime, hitstop, shake;
    int round, winner, difficulty;
    uint32_t random;
    unsigned soundEvents;
    bool paused;
} Game;

void GameStart(Game *game, MatchMode mode, int difficulty, uint32_t seed);
void GameUpdate(Game *game, FighterInput player, FighterInput opponent, float dt);
void GameNextRound(Game *game);
FighterInput GameAIInput(Game *game, int player, float dt);
Rectangle FighterHurtbox(const Fighter *fighter);
Rectangle FighterAttackbox(const Fighter *fighter);
bool FighterAttackActive(const Fighter *fighter);
float ActionDuration(Action action);
void GameParticles(Game *game, Vector2 position, Color color, int count, float force);
#endif
