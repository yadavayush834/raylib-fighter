#include "combat.h"
#include <math.h>
#include <string.h>

// ---------------------------------------------------------------------------------------------
// Roster and stages
// ---------------------------------------------------------------------------------------------
static const CharacterInfo CHARACTERS[CHAR_COUNT] = {
    {
        .name = "REN",
        .title = "THE STORMBOUND",
        .specialName = "GALE SHOT",
        .superName = "STORMBREAKER LANCE",
        .blurb = "A wandering duelist who bottles thunder in his fists. Controls space, then ends "
                 "it from across the arena.",
        .style = "ZONER",
        .accent = {117, 229, 255, 255},
        .deep = {30, 84, 132, 255},
        .glow = {190, 246, 255, 255},
        .walkSpeed = 310,
        .jumpVelocity = 860,
        .dashSpeed = 1050,
        .power = 4,
        .speed = 3,
        .reach = 5,
        .technique = 3,
    },
    {
        .name = "KAE",
        .title = "EMBERHEART",
        .specialName = "CRIMSON ASCENT",
        .superName = "EMBERFALL DANCE",
        .blurb = "A shrine guardian with a temper hotter than the lanterns she tends. Gets in, "
                 "stays in, and burns through any guard.",
        .style = "RUSHDOWN",
        .accent = {255, 153, 172, 255},
        .deep = {140, 36, 66, 255},
        .glow = {255, 214, 180, 255},
        .walkSpeed = 345,
        .jumpVelocity = 880,
        .dashSpeed = 1130,
        .power = 3,
        .speed = 5,
        .reach = 2,
        .technique = 4,
    },
};

const CharacterInfo *CharacterGet(int id) {
    if (id < 0 || id >= CHAR_COUNT)
        id = 0;
    return &CHARACTERS[id];
}
const char *StageName(StageId stage) {
    return stage == STAGE_STORM_TERRACE ? "STORM TERRACE" : "MOONLIT SANCTUARY";
}
const char *StageTagline(StageId stage) {
    return stage == STAGE_STORM_TERRACE ? "RAIN-SLICK STONE BENEATH A BREAKING SKY"
                                        : "CHERRY PETALS UNDER A PATIENT MOON";
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------
static float ClampValue(float v, float lo, float hi) {
    return fminf(hi, fmaxf(lo, v));
}
float GameRandom(Game *game) {
    uint32_t x = game->random;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    game->random = x;
    return (float)(x & 0xffffu) / 65535.0f;
}
static float Approach(float current, float target, float amount) {
    if (current < target)
        return fminf(current + amount, target);
    return fmaxf(current - amount, target);
}
static void Emit(Game *g, EventType type, Vector2 position, int owner, float strength) {
    if (g->eventCount >= MAX_EVENTS)
        return;
    g->events[g->eventCount++] = (GameEvent){type, position, owner, strength};
}
static Vector2 Chest(const Fighter *f) {
    return (Vector2){f->position.x, f->position.y - 120};
}

// ---------------------------------------------------------------------------------------------
// Frame data
// ---------------------------------------------------------------------------------------------
float ActionDuration(Action action) {
    switch (action) {
    case ACTION_LIGHT:
        return 0.34f;
    case ACTION_HEAVY:
        return 0.59f;
    case ACTION_SPECIAL:
        return 0.65f;
    case ACTION_DASH:
        return 0.20f;
    case ACTION_BACKDASH:
        return 0.27f;
    case ACTION_AIR:
        return 0.40f;
    case ACTION_DIVE:
        return 1.2f;
    case ACTION_SUPER:
        return 1.1f;
    default:
        return 0;
    }
}
static float MoveDuration(const Fighter *f) {
    if (f->action == ACTION_SPECIAL && f->character == CHAR_KAE)
        return 1.2f; // ends on landing
    if (f->action == ACTION_SUPER && f->character == CHAR_KAE)
        return f->connected ? 3.0f : 0.85f;
    return ActionDuration(f->action);
}
bool FighterCanAct(const Fighter *f) {
    return f->stun <= 0 && !f->launched && f->knockdown <= 0 && f->blockstun <= 0 && f->lag <= 0 &&
           f->health > 0;
}
bool FighterAttackActive(const Fighter *f) {
    if (f->stun > 0 || f->launched || f->health <= 0)
        return false;
    float t = f->actionTime;
    switch (f->action) {
    case ACTION_LIGHT:
        return t >= 0.08f && t < 0.15f;
    case ACTION_HEAVY:
        return t >= 0.21f && t < 0.32f;
    case ACTION_AIR:
        return t >= 0.07f && t < 0.30f;
    case ACTION_DIVE:
        return t >= 0.04f;
    case ACTION_SPECIAL:
        return f->character == CHAR_KAE && t >= 0.05f && t < 0.32f;
    case ACTION_SUPER:
        return f->character == CHAR_KAE && !f->connected && t >= 0.06f && t < 0.45f;
    default:
        return false;
    }
}
Rectangle FighterHurtbox(const Fighter *f) {
    if (f->knockdown > 0)
        return (Rectangle){f->position.x - 70, f->position.y - 45, 140, 45};
    if (f->launched)
        return (Rectangle){f->position.x - 45, f->position.y - 150, 90, 130};
    return (Rectangle){f->position.x - 27, f->position.y - 190, 54, 190};
}
static Rectangle Forward(const Fighter *f, float near, float reach, float top, float height) {
    float x = f->facing > 0 ? f->position.x + near : f->position.x - near - reach;
    return (Rectangle){x, f->position.y + top, reach, height};
}
Rectangle FighterAttackbox(const Fighter *f) {
    switch (f->action) {
    case ACTION_HEAVY:
        return Forward(f, 15, 100, -192, 84);
    case ACTION_AIR:
        return Forward(f, 5, 88, -150, 76);
    case ACTION_DIVE:
        return Forward(f, -5, 82, -95, 100);
    case ACTION_SPECIAL:
        return Forward(f, -15, 85, -235, 215);
    case ACTION_SUPER:
        if (f->character == CHAR_REN) {
            float start = f->facing > 0 ? f->position.x + 50 : -200;
            float end = f->facing > 0 ? ARENA_W + 200 : f->position.x - 50;
            return (Rectangle){start, f->position.y - 205, end - start, 112};
        }
        return Forward(f, -10, 100, -185, 170);
    default:
        return Forward(f, 15, 90, -174, 60);
    }
}

// ---------------------------------------------------------------------------------------------
// Round flow
// ---------------------------------------------------------------------------------------------
static void ResetFighters(Game *g) {
    for (int i = 0; i < 2; ++i) {
        Fighter keep = g->fighters[i];
        g->fighters[i] = (Fighter){.character = keep.character,
                                   .position = {i == 0 ? 365 : 915, GROUND_Y},
                                   .health = MAX_HEALTH,
                                   .healthTrail = MAX_HEALTH,
                                   .energy = 40,
                                   .facing = i == 0 ? 1 : -1,
                                   .grounded = true,
                                   .wins = keep.wins,
                                   .maxCombo = keep.maxCombo,
                                   .specialsLanded = keep.specialsLanded,
                                   .supersLanded = keep.supersLanded,
                                   .hitsLanded = keep.hitsLanded,
                                   .damageDealt = keep.damageDealt};
    }
    memset(g->projectiles, 0, sizeof(g->projectiles));
    memset(g->brains, 0, sizeof(g->brains));
}
static void ResetRound(Game *g) {
    ResetFighters(g);
    g->roundTime = 75;
    g->phaseTime = 0;
    g->phase = ROUND_INTRO;
    g->winner = -1;
    g->hitstop = 0;
    g->superFreeze = 0;
    g->timeUp = g->perfect = false;
}
void GameStartEx(Game *g, MatchMode mode, int difficulty, uint32_t seed, int p1, int p2, StageId stage) {
    memset(g, 0, sizeof(*g));
    g->mode = mode;
    g->difficulty = (int)ClampValue((float)difficulty, 0, 2);
    g->random = seed ? seed : 1;
    g->round = 1;
    g->stage = stage;
    g->fighters[0].character = (int)ClampValue((float)p1, 0, CHAR_COUNT - 1);
    g->fighters[1].character = (int)ClampValue((float)p2, 0, CHAR_COUNT - 1);
    ResetRound(g);
}
void GameStart(Game *g, MatchMode mode, int difficulty, uint32_t seed) {
    GameStartEx(g, mode, difficulty, seed, CHAR_REN, CHAR_KAE, STAGE_SANCTUARY);
}
void GameNextRound(Game *g) {
    g->round++;
    ResetRound(g);
}
void GameTrainingReset(Game *g) {
    ResetFighters(g);
    g->hitstop = g->superFreeze = 0;
    g->phase = ROUND_FIGHT;
}

// ---------------------------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------------------------
static void BeginAction(Game *g, int index, Action action, bool cancel) {
    Fighter *f = &g->fighters[index];
    const CharacterInfo *c = CharacterGet(f->character);
    f->chain = cancel ? f->chain + 1 : 1;
    f->action = action;
    f->actionTime = 0;
    f->connected = false;
    f->blocking = false;
    f->superHits = 0;
    f->superHitTimer = 0;
    switch (action) {
    case ACTION_DASH:
        f->energy -= DASH_COST;
        f->velocity.x = f->facing * c->dashSpeed;
        f->invincible = 0.14f;
        Emit(g, EV_DASH, f->position, index, 1);
        break;
    case ACTION_BACKDASH:
        f->energy -= DASH_COST * 0.5f;
        f->velocity.x = -f->facing * c->dashSpeed * 0.78f;
        f->invincible = 0.2f;
        Emit(g, EV_DASH, f->position, index, 1);
        break;
    case ACTION_SPECIAL:
        f->energy -= SPECIAL_COST;
        if (f->character == CHAR_KAE)
            f->invincible = 0.15f;
        Emit(g, EV_WHIFF, Chest(f), index, 3);
        break;
    case ACTION_SUPER:
        f->energy -= SUPER_COST;
        f->velocity = (Vector2){0, 0};
        f->invincible = 0.4f;
        g->superFreeze = SUPER_FREEZE;
        g->superOwner = index;
        Emit(g, EV_SUPER_FLASH, Chest(f), index, 4);
        break;
    case ACTION_DIVE:
        f->airUsed = true;
        f->velocity = (Vector2){f->facing * 560, 760};
        Emit(g, EV_WHIFF, Chest(f), index, 2);
        break;
    case ACTION_AIR:
        f->airUsed = true;
        Emit(g, EV_WHIFF, Chest(f), index, 1);
        break;
    case ACTION_HEAVY:
        Emit(g, EV_WHIFF, Chest(f), index, 2);
        break;
    case ACTION_LIGHT:
        Emit(g, EV_WHIFF, Chest(f), index, 1);
        break;
    default:
        break;
    }
}
static bool CanCancel(const Fighter *f, Action next) {
    if (!f->connected)
        return false;
    switch (f->action) {
    case ACTION_LIGHT:
        if (f->actionTime < 0.10f)
            return false;
        return next == ACTION_LIGHT ? f->chain < 3
                                    : next == ACTION_HEAVY || next == ACTION_SPECIAL || next == ACTION_SUPER;
    case ACTION_HEAVY:
        return f->actionTime >= 0.24f && (next == ACTION_SPECIAL || next == ACTION_SUPER);
    case ACTION_SPECIAL:
        return f->character == CHAR_KAE && next == ACTION_SUPER;
    default:
        return false;
    }
}
static bool Affordable(const Fighter *f, Action action) {
    if (action == ACTION_SPECIAL)
        return f->energy >= SPECIAL_COST;
    if (action == ACTION_SUPER)
        return f->energy >= SUPER_COST;
    return true;
}
static void SpawnProjectile(Game *g, int owner) {
    Fighter *f = &g->fighters[owner];
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        if (g->projectiles[i].active)
            continue;
        g->projectiles[i] = (Projectile){.position = {f->position.x + f->facing * 95, f->position.y - 165},
                                         .velocity = f->facing * 730,
                                         .life = 1.7f,
                                         .owner = owner,
                                         .active = true};
        Emit(g, EV_FIRE, g->projectiles[i].position, owner, 3);
        return;
    }
}

// ---------------------------------------------------------------------------------------------
// Damage
// ---------------------------------------------------------------------------------------------
typedef struct {
    float damage, stun, push, lift, hitstop;
    float strength;
    bool launch, special, super;
} HitSpec;

static bool Hittable(const Fighter *t) {
    return t->invincible <= 0 && t->health > 0 && t->knockdown <= 0;
}

// Returns true when the attack touched the target, whether it was guarded or not.
static bool Hit(Game *g, int attacker, HitSpec spec, Vector2 impact) {
    Fighter *t = &g->fighters[1 - attacker];
    Fighter *s = &g->fighters[attacker];
    if (!Hittable(t))
        return false;
    int dir = spec.push >= 0 ? 1 : -1;
    bool atWall = t->position.x <= WALL_MIN + 4 || t->position.x >= WALL_MAX - 4;
    bool front = (t->position.x < impact.x) == (t->facing > 0);
    bool guarded = t->blocking && front && t->grounded;
    bool guardBreak = false;
    if (guarded && (t->energy >= 7 || spec.super)) {
        float chip = spec.damage * (spec.super ? 0.18f : spec.special ? 0.12f : 0.07f);
        t->health = fmaxf(1, t->health - chip);
        t->energy = fmaxf(0, t->energy - (spec.super ? 2 : 7));
        t->blockstun = fmaxf(t->blockstun, spec.stun * 0.55f);
        t->velocity.x = dir * fabsf(spec.push) * 0.6f;
        if (atWall)
            s->velocity.x -= dir * fabsf(spec.push) * 0.55f;
        s->energy = fminf(MAX_ENERGY, s->energy + 3);
        g->hitstop = fmaxf(g->hitstop, 0.04f + spec.strength * 0.01f);
        Emit(g, EV_BLOCK, impact, attacker, spec.strength);
        return true;
    }
    if (guarded) {
        guardBreak = true;
        t->energy = 0;
        Emit(g, EV_GUARD_BREAK, impact, attacker, 3);
    }
    bool counter = !guardBreak && t->stun <= 0 && t->action != ACTION_IDLE && t->action != ACTION_DASH &&
                   t->action != ACTION_BACKDASH;
    float scale = s->combo < 2       ? 1
                  : spec.super ? fmaxf(0.7f, 1 - 0.04f * (float)(s->combo - 1))
                               : fmaxf(0.3f, 1 - 0.11f * (float)(s->combo - 1));
    float damage = spec.damage * scale * (counter ? 1.2f : 1) * (guardBreak ? 0.5f : 1);
    t->health = fmaxf(0, t->health - damage);
    t->hurtDirection = dir;
    t->action = ACTION_IDLE;
    t->blocking = false;
    t->lag = t->blockstun = t->bufferTime = 0;
    t->counterHit = counter;
    bool launch = spec.launch || !t->grounded || (counter && spec.strength >= 2) || t->health <= 0;
    if (guardBreak) {
        t->stun = 0.8f;
        t->hurtDuration = t->stun;
        t->velocity.x = dir * 160;
    } else if (launch) {
        float decay = powf(0.8f, (float)t->juggle);
        float lift = spec.lift > 0 ? spec.lift : 560;
        if (t->health <= 0)
            lift = fmaxf(lift, 720);
        if (!t->launched)
            t->wallBounced = false;
        t->launched = true;
        t->grounded = false;
        t->velocity.x = dir * fmaxf(fabsf(spec.push) * (spec.launch ? 1 : 0.55f), t->health <= 0 ? 420 : 160);
        t->velocity.y = t->juggle >= 5 ? fmaxf(t->velocity.y, 120) : -lift * decay;
        t->juggle++;
        t->stun = fmaxf(t->stun, 0.25f);
        t->hurtDuration = 0.35f;
        Emit(g, EV_LAUNCH, impact, attacker, spec.strength);
    } else {
        t->stun = spec.stun;
        t->hurtDuration = t->stun;
        t->velocity.x = spec.push;
        if (atWall)
            s->velocity.x -= dir * fabsf(spec.push) * 0.6f;
    }
    g->hitstop = fmaxf(g->hitstop, spec.hitstop + (counter ? 0.05f : 0) + (guardBreak ? 0.12f : 0));
    s->combo++;
    s->comboTime = 1.2f;
    s->comboDamage += damage;
    s->damageDealt += damage;
    s->hitsLanded++;
    if (s->combo > s->maxCombo)
        s->maxCombo = s->combo;
    if (!spec.super)
        s->energy = fminf(MAX_ENERGY, s->energy + 3 + spec.strength * 3);
    t->energy = fminf(MAX_ENERGY, t->energy + 4);
    Emit(g, spec.super ? EV_SUPER_HIT : EV_HIT, impact, attacker, spec.strength);
    if (counter)
        Emit(g, EV_COUNTER, impact, attacker, spec.strength);
    return true;
}

static HitSpec MeleeSpec(const Fighter *f) {
    float dir = (float)f->facing;
    switch (f->action) {
    case ACTION_HEAVY:
        if (f->chain >= 2)
            return (HitSpec){118, 0.4f, dir * 540, 640, 0.11f, 2, .launch = true};
        return (HitSpec){118, 0.38f, dir * 450, 0, 0.1f, 2, false, false, false};
    case ACTION_AIR:
        return (HitSpec){70, 0.36f, dir * 200, 380, 0.065f, 1, false, false, false};
    case ACTION_DIVE:
        return (HitSpec){92, 0.4f, dir * 260, 420, 0.085f, 2, false, false, false};
    case ACTION_SPECIAL:
        return (HitSpec){110, 0.4f, dir * 170, 920, 0.13f, 3, .launch = true, .special = true};
    case ACTION_SUPER:
        return (HitSpec){36, 0.6f, dir * 40, 0, 0.03f, 3, .super = true};
    default:
        return (HitSpec){64, 0.28f, dir * 270, 0, 0.06f, 1, false, false, false};
    }
}

static void UpdateSuper(Game *g, int index, float dt) {
    Fighter *f = &g->fighters[index];
    Fighter *t = &g->fighters[1 - index];
    float dir = (float)f->facing;
    if (f->character == CHAR_REN) {
        // Stormbreaker Lance: a screen-long beam that ticks eight times, then launches.
        if (f->actionTime < 0.12f || f->actionTime > 0.82f || f->superHits >= 9)
            return;
        f->superHitTimer -= dt;
        if (f->superHitTimer > 0)
            return;
        f->superHitTimer = 0.075f;
        if (!CheckCollisionRecs(FighterAttackbox(f), FighterHurtbox(t)) || !Hittable(t))
            return;
        f->superHits++;
        Vector2 impact = {t->position.x - dir * 20, f->position.y - 150};
        bool last = f->superHits >= 9;
        HitSpec spec = last ? (HitSpec){120, 0.5f, dir * 520, 780, 0.2f, 4, .launch = true, .super = true}
                            : (HitSpec){40, 0.4f, dir * 70, 0, 0.025f, 3, .super = true};
        if (Hit(g, index, spec, impact) && !f->connected) {
            f->connected = true;
            f->supersLanded++;
        }
        return;
    }
    // Emberfall Dance: an invincible rush that locks the target into a flurry.
    if (!f->connected) {
        if (f->actionTime >= 0.06f && f->actionTime < 0.45f)
            f->velocity.x = dir * 1150;
        else
            f->velocity.x = Approach(f->velocity.x, 0, dt * 4000);
        if (FighterAttackActive(f) && Hittable(t) && CheckCollisionRecs(FighterAttackbox(f), FighterHurtbox(t))) {
            bool guarded = t->blocking && t->grounded && ((t->position.x < f->position.x) != (t->facing > 0));
            Vector2 impact = {t->position.x - dir * 18, f->position.y - 120};
            HitSpec spec = MeleeSpec(f);
            if (guarded) {
                Hit(g, index, spec, impact);
                f->actionTime = 0.45f; // blocked: stop dead and eat the recovery
                f->velocity.x = 0;
            } else if (Hit(g, index, spec, impact)) {
                f->connected = true;
                f->superHits = 1;
                f->superHitTimer = 0.08f;
                f->velocity.x = 0;
                f->supersLanded++;
            }
        }
        return;
    }
    f->velocity.x = 0;
    t->position.x = ClampValue(f->position.x + dir * 72, WALL_MIN, WALL_MAX);
    if (fabsf(t->position.x - f->position.x) < 60)
        f->position.x = ClampValue(t->position.x - dir * 72, WALL_MIN, WALL_MAX);
    f->superHitTimer -= dt;
    if (f->superHitTimer > 0)
        return;
    f->superHits++;
    f->superHitTimer = 0.075f;
    t->invincible = 0;
    t->knockdown = 0;
    Vector2 impact = {t->position.x - dir * 18 + (GameRandom(g) - 0.5f) * 40,
                      f->position.y - 150 + (GameRandom(g) - 0.5f) * 70};
    if (f->superHits >= 9) {
        Hit(g, index, (HitSpec){125, 0.5f, dir * 320, 880, 0.22f, 4, .launch = true, .super = true}, impact);
        f->action = ACTION_IDLE;
        f->lag = 0.3f;
        f->velocity.y = -380;
        f->grounded = false;
        f->airUsed = true;
    } else {
        Hit(g, index, MeleeSpec(f), impact);
        t->stun = 0.6f;
        t->launched = false;
        t->grounded = true;
        t->position.y = GROUND_Y;
        t->velocity = (Vector2){0, 0};
    }
}

static void BufferInput(Fighter *f, FighterInput input) {
    if (input.super || input.special || input.heavy || input.light) {
        f->bufferedAction = input.super     ? ACTION_SUPER
                            : input.special ? ACTION_SPECIAL
                            : input.heavy   ? ACTION_HEAVY
                                            : ACTION_LIGHT;
        f->bufferTime = 0.14f;
    }
}

// ---------------------------------------------------------------------------------------------
// Per-fighter step
// ---------------------------------------------------------------------------------------------
static void UpdateFighter(Game *g, int index, FighterInput input, float dt) {
    Fighter *f = &g->fighters[index];
    Fighter *opponent = &g->fighters[1 - index];
    const CharacterInfo *c = CharacterGet(f->character);
    if (!f->launched)
        f->stun = fmaxf(0, f->stun - dt);
    f->invincible = fmaxf(0, f->invincible - dt);
    f->comboTime = fmaxf(0, f->comboTime - dt);
    f->bufferTime = fmaxf(0, f->bufferTime - dt);
    f->blockstun = fmaxf(0, f->blockstun - dt);
    f->lag = fmaxf(0, f->lag - dt);
    if (f->knockdown > 0 && f->health > 0) {
        f->knockdown -= dt;
        if (f->knockdown <= 0) {
            f->knockdown = 0;
            f->invincible = fmaxf(f->invincible, 0.15f);
        }
    }
    BufferInput(f, input);
    f->energy = fminf(MAX_ENERGY, f->energy + dt * 4.5f);
    bool hurting = f->stun > 0 || f->launched || f->knockdown > 0;
    f->healthTrail = Approach(f->healthTrail, f->health, dt * (hurting ? 25 : 320));
    input.move = ClampValue(input.move, -1, 1);
    bool free = FighterCanAct(f);
    if (free && f->action == ACTION_IDLE) {
        if (f->grounded)
            f->facing = f->position.x < opponent->position.x ? 1 : -1;
        f->blocking = input.block && f->grounded;
        Action want = f->bufferTime > 0 ? f->bufferedAction : ACTION_IDLE;
        if (want == ACTION_SUPER && f->grounded && Affordable(f, ACTION_SUPER)) {
            BeginAction(g, index, ACTION_SUPER, false);
            f->bufferTime = 0;
        } else if (input.dash && f->grounded && f->energy >= DASH_COST) {
            BeginAction(g, index, input.move * f->facing < -0.3f ? ACTION_BACKDASH : ACTION_DASH, false);
        } else if (want != ACTION_IDLE && want != ACTION_SUPER) {
            if (f->grounded && Affordable(f, want)) {
                BeginAction(g, index, want, false);
                f->bufferTime = 0;
            } else if (!f->grounded && !f->airUsed && want != ACTION_SPECIAL) {
                Action air = want == ACTION_HEAVY ? ACTION_DIVE : ACTION_AIR;
                if (air == ACTION_AIR || f->position.y < GROUND_Y - 70) {
                    BeginAction(g, index, air, false);
                    f->bufferTime = 0;
                }
            }
        }
        if (input.jump && f->grounded && f->action == ACTION_IDLE && !f->blocking) {
            f->velocity.y = -c->jumpVelocity;
            f->velocity.x = input.move * c->walkSpeed * 1.05f;
            f->grounded = false;
            f->airUsed = false;
            Emit(g, EV_JUMP, f->position, index, 1);
        }
    } else if (f->stun <= 0 && !f->launched && f->action != ACTION_IDLE && f->bufferTime > 0 &&
               CanCancel(f, f->bufferedAction) && Affordable(f, f->bufferedAction)) {
        BeginAction(g, index, f->bufferedAction, true);
        f->bufferTime = 0;
    }
    if (!(free && f->action == ACTION_IDLE))
        f->blocking = f->blockstun > 0;

    // Horizontal movement.
    if (free && f->action == ACTION_IDLE) {
        if (f->grounded) {
            float speed = f->blocking ? 0 : c->walkSpeed;
            f->velocity.x = Approach(f->velocity.x, input.move * speed, dt * 2500);
        } else
            f->velocity.x = Approach(f->velocity.x, input.move * c->walkSpeed, dt * 700);
    } else if (f->action == ACTION_DASH || f->action == ACTION_BACKDASH) {
        if (f->actionTime > ActionDuration(f->action) * 0.7f)
            f->velocity.x = Approach(f->velocity.x, 0, dt * 4200);
    } else if (f->action == ACTION_DIVE || f->action == ACTION_SUPER) {
        // velocity is driven by the move itself
    } else if (f->action == ACTION_SPECIAL && f->character == CHAR_KAE) {
        if (f->grounded && f->actionTime > 0.1f)
            f->velocity.x = Approach(f->velocity.x, 0, dt * 2600);
    } else if (f->grounded)
        f->velocity.x = Approach(f->velocity.x, 0, dt * 1300);
    else if (f->launched)
        f->velocity.x = Approach(f->velocity.x, 0, dt * 60);

    // Action timeline.
    if (f->action != ACTION_IDLE) {
        float previous = f->actionTime;
        f->actionTime += dt;
        if (f->action == ACTION_SPECIAL && f->character == CHAR_REN && previous < 0.24f && f->actionTime >= 0.24f) {
            SpawnProjectile(g, index);
        }
        if (f->action == ACTION_SPECIAL && f->character == CHAR_KAE && previous < 0.05f && f->actionTime >= 0.05f) {
            f->velocity = (Vector2){f->facing * 230.0f, -900};
            f->grounded = false;
            f->airUsed = true;
            Emit(g, EV_JUMP, f->position, index, 3);
        }
        if (f->action == ACTION_SUPER)
            UpdateSuper(g, index, dt);
        if (f->action != ACTION_IDLE && f->actionTime >= MoveDuration(f))
            f->action = ACTION_IDLE;
    }

    // Physics.
    f->velocity.y += (f->launched ? 2100 : 1900) * dt;
    f->position.x += f->velocity.x * dt;
    if (f->position.x < WALL_MIN || f->position.x > WALL_MAX) {
        if (f->launched && !f->wallBounced && fabsf(f->velocity.x) > 380) {
            f->velocity.x = -f->velocity.x * 0.45f;
            f->velocity.y = fminf(f->velocity.y, -430);
            f->wallBounced = true;
            Emit(g, EV_WALL_BOUNCE, (Vector2){f->position.x < WALL_MIN ? 40 : ARENA_W - 40, f->position.y - 100},
                 1 - index, 3);
        }
        f->position.x = ClampValue(f->position.x, WALL_MIN, WALL_MAX);
    }
    f->position.y += f->velocity.y * dt;
    if (f->position.y >= GROUND_Y) {
        bool wasAirborne = !f->grounded;
        f->position.y = GROUND_Y;
        f->velocity.y = 0;
        f->grounded = true;
        if (wasAirborne) {
            f->airUsed = false;
            if (f->launched) {
                f->launched = false;
                f->knockdown = f->health <= 0 ? 99 : 0.62f;
                f->invincible = f->knockdown + 0.1f;
                f->stun = 0;
                f->hurtDuration = 0;
                f->velocity.x *= 0.3f;
                f->juggle = 0;
                f->wallBounced = false;
                Emit(g, EV_KNOCKDOWN, f->position, 1 - index, 2);
            } else {
                Emit(g, EV_LAND, f->position, index, f->action == ACTION_DIVE ? 2 : 1);
                if (f->action == ACTION_AIR || f->action == ACTION_DIVE) {
                    f->lag = f->connected ? 0.03f : f->action == ACTION_DIVE ? 0.2f : 0.07f;
                    f->action = ACTION_IDLE;
                    f->velocity.x *= 0.3f;
                } else if (f->action == ACTION_SPECIAL && f->character == CHAR_KAE && f->actionTime > 0.1f) {
                    f->lag = f->connected ? 0.05f : 0.32f;
                    f->action = ACTION_IDLE;
                }
            }
        }
    }
}

static void ResolveMelee(Game *g, int i) {
    Fighter *f = &g->fighters[i], *target = &g->fighters[1 - i];
    if (f->action == ACTION_SUPER || !FighterAttackActive(f) || f->connected || !Hittable(target))
        return;
    Rectangle box = FighterAttackbox(f);
    Rectangle hurt = FighterHurtbox(target);
    if (!CheckCollisionRecs(box, hurt))
        return;
    f->connected = true;
    Vector2 impact = {target->position.x - f->facing * 18, fmaxf(hurt.y + 20, box.y + box.height * 0.5f)};
    HitSpec spec = MeleeSpec(f);
    float before = target->health;
    Hit(g, i, spec, impact);
    if (f->action == ACTION_SPECIAL && target->health < before)
        f->specialsLanded++;
    if (f->action == ACTION_DIVE) {
        // Dive kicks bounce off the target so the attacker can keep pressure.
        f->velocity = (Vector2){-f->facing * 230.0f, -430};
        f->action = ACTION_IDLE;
        f->lag = 0.12f;
    }
}

static void UpdateProjectiles(Game *g, float dt) {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        Projectile *p = &g->projectiles[i];
        if (!p->active)
            continue;
        p->life -= dt;
        p->position.x += p->velocity * dt;
        if (p->life <= 0 || p->position.x < 40 || p->position.x > ARENA_W - 40) {
            p->active = false;
            continue;
        }
        for (int j = 0; j < MAX_PROJECTILES; ++j) {
            Projectile *other = &g->projectiles[j];
            if (j != i && other->active && other->owner != p->owner &&
                fabsf(other->position.x - p->position.x) < 40 && fabsf(other->position.y - p->position.y) < 35) {
                p->active = other->active = false;
                Emit(g, EV_CLASH, p->position, p->owner, 3);
                break;
            }
        }
        Fighter *target = &g->fighters[1 - p->owner];
        if (p->active && Hittable(target) && CheckCollisionCircleRec(p->position, 18, FighterHurtbox(target))) {
            float before = target->health;
            Hit(g, p->owner, (HitSpec){105, 0.4f, p->velocity > 0 ? 400.0f : -400.0f, 520, 0.09f, 3, .special = true},
                p->position);
            if (target->health < before)
                g->fighters[p->owner].specialsLanded++;
            p->active = false;
        }
    }
}

static FighterInput DummyInput(Game *g, float dt) {
    switch (g->dummy) {
    case DUMMY_GUARD:
        return (FighterInput){.block = true};
    case DUMMY_JUMP:
        return (FighterInput){.jump = g->fighters[1].grounded};
    case DUMMY_CPU:
        return GameAIInput(g, 1, dt);
    default:
        return (FighterInput){0};
    }
}

void GameUpdate(Game *g, FighterInput player, FighterInput opponent, float dt) {
    if (g->paused || dt <= 0)
        return;
    g->eventCount = 0;
    g->totalTime += dt;
    if (g->phase == ROUND_INTRO) {
        if (g->phaseTime == 0)
            Emit(g, EV_ROUND_START, (Vector2){ARENA_W / 2, 300}, 0, (float)g->round);
        g->phaseTime += dt;
        if (g->phaseTime >= 2.2f) {
            g->phase = ROUND_FIGHT;
            g->phaseTime = 0;
            Emit(g, EV_FIGHT, (Vector2){ARENA_W / 2, 300}, 0, 1);
        }
        return;
    }
    if (g->phase == MATCH_END)
        return;
    if (g->superFreeze > 0 || g->hitstop > 0) {
        // Time is frozen, but attack presses still buffer so players can mash through impacts.
        if (g->phase == ROUND_FIGHT && g->mode != MODE_DEMO) {
            BufferInput(&g->fighters[0], player);
            if (g->mode == MODE_LOCAL)
                BufferInput(&g->fighters[1], opponent);
        }
        if (g->superFreeze > 0)
            g->superFreeze = fmaxf(0, g->superFreeze - dt);
        else
            g->hitstop = fmaxf(0, g->hitstop - dt);
        return;
    }
    bool fighting = g->phase == ROUND_FIGHT;
    if (fighting) {
        if (g->mode == MODE_SOLO || g->mode == MODE_DEMO)
            opponent = GameAIInput(g, 1, dt);
        if (g->mode == MODE_DEMO)
            player = GameAIInput(g, 0, dt);
        if (g->mode == MODE_TRAINING) {
            opponent = DummyInput(g, dt);
            g->fighters[0].energy = MAX_ENERGY;
        }
    } else {
        player = opponent = (FighterInput){0};
        g->phaseTime += dt;
        if (g->phaseTime > 3.2f) {
            if (g->fighters[0].wins >= 2 || g->fighters[1].wins >= 2)
                g->phase = MATCH_END;
            else
                GameNextRound(g);
            return;
        }
    }
    FighterInput inputs[2] = {player, opponent};
    for (int i = 0; i < 2; ++i)
        UpdateFighter(g, i, inputs[i], dt);
    Fighter *a = &g->fighters[0], *b = &g->fighters[1];
    float separation = b->position.x - a->position.x;
    bool rushing = (a->action == ACTION_SUPER && a->connected) || (b->action == ACTION_SUPER && b->connected);
    if (fabsf(separation) < 58 && fabsf(a->position.y - b->position.y) < 90 && a->action != ACTION_DASH &&
        b->action != ACTION_DASH && a->knockdown <= 0 && b->knockdown <= 0 && !rushing) {
        float push = (58 - fabsf(separation)) * 0.5f;
        float sign = separation >= 0 ? 1 : -1;
        a->position.x = ClampValue(a->position.x - sign * push, WALL_MIN, WALL_MAX);
        b->position.x = ClampValue(b->position.x + sign * push, WALL_MIN, WALL_MAX);
    }
    if (fighting) {
        ResolveMelee(g, 0);
        ResolveMelee(g, 1);
        UpdateProjectiles(g, dt);
    }
    // A combo ends when the victim is back on their feet.
    for (int i = 0; i < 2; ++i) {
        Fighter *s = &g->fighters[i], *t = &g->fighters[1 - i];
        if (s->combo > 0 && t->stun <= 0 && !t->launched) {
            s->lastComboDamage = s->comboDamage;
            s->combo = 0;
            s->comboDamage = 0;
        }
    }
    if (g->mode == MODE_TRAINING) {
        Fighter *dummy = &g->fighters[1];
        if (a->combo == 0 && a->comboTime <= 0 && FighterCanAct(dummy) && dummy->health < MAX_HEALTH)
            dummy->health = dummy->healthTrail = MAX_HEALTH;
        if (dummy->health <= 0) {
            dummy->health = 1;
        }
        if (b->combo == 0 && FighterCanAct(a) && a->health < MAX_HEALTH)
            a->health = a->healthTrail = MAX_HEALTH;
        if (a->health <= 0)
            a->health = 1;
        return;
    }
    if (!fighting)
        return;
    g->roundTime = fmaxf(0, g->roundTime - dt);
    if (a->health <= 0 || b->health <= 0 || g->roundTime <= 0) {
        g->timeUp = a->health > 0 && b->health > 0;
        g->winner = a->health == b->health ? -1 : a->health > b->health ? 0 : 1;
        if (g->winner >= 0)
            g->fighters[g->winner].wins++;
        g->phase = ROUND_END;
        g->phaseTime = 0;
        memset(g->projectiles, 0, sizeof(g->projectiles));
        int loser = g->winner >= 0 ? 1 - g->winner : 1;
        Emit(g, g->timeUp ? EV_TIME_UP : EV_KO, Chest(&g->fighters[loser]), g->winner, 4);
        if (g->winner >= 0 && g->fighters[g->winner].health >= MAX_HEALTH) {
            g->perfect = true;
            Emit(g, EV_PERFECT, Chest(&g->fighters[g->winner]), g->winner, 4);
        }
    }
}
