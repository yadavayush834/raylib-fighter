#ifndef SPIRIT_COMBAT_H
#define SPIRIT_COMBAT_H
#include "raylib.h"
#include <stdint.h>

#define ARENA_W 1280.0f
#define ARENA_H 720.0f
#define GROUND_Y 590.0f
#define WALL_MIN 85.0f
#define WALL_MAX (ARENA_W - 85.0f)
#define MAX_HEALTH 1000.0f
#define MAX_ENERGY 100.0f
#define SPECIAL_COST 35.0f
#define DASH_COST 12.0f
#define SUPER_COST 100.0f
#define MAX_PROJECTILES 8
#define MAX_EVENTS 48
#define FIXED_DT (1.0f / 120.0f)
#define SUPER_FREEZE 0.75f

typedef enum { CHAR_REN, CHAR_KAE, CHAR_COUNT } CharacterId;
typedef enum { STAGE_SANCTUARY, STAGE_STORM_TERRACE, STAGE_COUNT } StageId;

typedef enum {
    ACTION_IDLE,
    ACTION_LIGHT,
    ACTION_HEAVY,
    ACTION_SPECIAL,
    ACTION_DASH,
    ACTION_BACKDASH,
    ACTION_AIR,
    ACTION_DIVE,
    ACTION_SUPER
} Action;
typedef enum { ROUND_INTRO, ROUND_FIGHT, ROUND_END, MATCH_END } Phase;
typedef enum { MODE_SOLO, MODE_LOCAL, MODE_TRAINING, MODE_DEMO } MatchMode;
typedef enum { DUMMY_STAND, DUMMY_GUARD, DUMMY_JUMP, DUMMY_CPU, DUMMY_COUNT } DummyMode;

// The simulation publishes what happened each step. Audio, camera and VFX react to these
// without touching combat state, which keeps the simulation deterministic and testable.
typedef enum {
    EV_HIT,
    EV_BLOCK,
    EV_COUNTER,
    EV_GUARD_BREAK,
    EV_LAUNCH,
    EV_WALL_BOUNCE,
    EV_KNOCKDOWN,
    EV_LAND,
    EV_JUMP,
    EV_DASH,
    EV_WHIFF,
    EV_FIRE,
    EV_CLASH,
    EV_SUPER_FLASH,
    EV_SUPER_HIT,
    EV_KO,
    EV_ROUND_START,
    EV_FIGHT,
    EV_TIME_UP,
    EV_PERFECT,
    EV_COUNT
} EventType;

typedef struct {
    EventType type;
    Vector2 position;
    int owner;      // fighter that caused the event
    float strength; // 1 light, 2 heavy, 3 special/super
} GameEvent;

typedef struct {
    float move;
    bool jump, light, heavy, special, dash, block, super;
} FighterInput;

typedef struct {
    int character;
    Vector2 position, velocity;
    float health, healthTrail, energy;
    float stun, invincible, actionTime, comboTime, bufferTime;
    float hurtDuration, knockdown, blockstun, lag;
    float superHitTimer;
    int hurtDirection;
    int facing, wins, combo, chain, juggle, superHits;
    float comboDamage, lastComboDamage;
    Action action, bufferedAction;
    bool grounded, blocking, connected, launched, wallBounced, counterHit, airUsed;
    // Match statistics for the results screen.
    int maxCombo, specialsLanded, supersLanded, hitsLanded;
    float damageDealt;
} Fighter;

typedef struct {
    Vector2 position;
    float velocity, life;
    int owner;
    bool active;
} Projectile;

typedef struct {
    float think, reaction, block, move, jumpCooldown, dashCooldown;
    float seenJumps, seenShots, patience;
    int pending, stimulus;
    bool chainRolled, chainGo;
    Action chainFor;
    float chainStamp;
} Brain;

typedef struct {
    Fighter fighters[2];
    Projectile projectiles[MAX_PROJECTILES];
    Brain brains[2];
    GameEvent events[MAX_EVENTS];
    int eventCount;
    Phase phase;
    MatchMode mode;
    DummyMode dummy;
    StageId stage;
    float roundTime, phaseTime, totalTime, hitstop, superFreeze;
    int superOwner;
    int round, winner, difficulty;
    uint32_t random;
    bool paused, timeUp, perfect;
} Game;

typedef struct {
    const char *name, *title, *specialName, *superName, *blurb, *style;
    Color accent, deep, glow;
    float walkSpeed, jumpVelocity, dashSpeed;
    int power, speed, reach, technique; // 1..5 display stats
} CharacterInfo;

const CharacterInfo *CharacterGet(int id);
const char *StageName(StageId stage);
const char *StageTagline(StageId stage);

void GameStart(Game *game, MatchMode mode, int difficulty, uint32_t seed);
void GameStartEx(Game *game, MatchMode mode, int difficulty, uint32_t seed, int p1, int p2, StageId stage);
void GameUpdate(Game *game, FighterInput player, FighterInput opponent, float dt);
void GameNextRound(Game *game);
void GameTrainingReset(Game *game);
FighterInput GameAIInput(Game *game, int player, float dt);
Rectangle FighterHurtbox(const Fighter *fighter);
Rectangle FighterAttackbox(const Fighter *fighter);
bool FighterAttackActive(const Fighter *fighter);
bool FighterCanAct(const Fighter *fighter);
float ActionDuration(Action action);
float GameRandom(Game *game);
#endif
