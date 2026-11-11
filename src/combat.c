#include "combat.h"
#include <math.h>
#include <string.h>

static float ClampValue(float v, float lo, float hi) {
    return fminf(hi, fmaxf(lo, v));
}
static float RandomFloat(Game *game) {
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
static void EmitSound(Game *g, SoundEvent event) {
    g->soundEvents |= 1u << event;
}

Rectangle FighterHurtbox(const Fighter *f) {
    return (Rectangle){f->position.x - 27, f->position.y - 190, 54, 190};
}
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
    default:
        return 0;
    }
}
bool FighterAttackActive(const Fighter *f) {
    if (f->stun > 0 || f->health <= 0)
        return false;
    if (f->action == ACTION_LIGHT)
        return f->actionTime >= 0.08f && f->actionTime < 0.15f;
    if (f->action == ACTION_HEAVY)
        return f->actionTime >= 0.21f && f->actionTime < 0.32f;
    return false;
}
Rectangle FighterAttackbox(const Fighter *f) {
    float reach = f->action == ACTION_HEAVY ? 100 : 90;
    return (Rectangle){f->facing > 0 ? f->position.x + 15 : f->position.x - reach - 15,
                       f->position.y - (f->action == ACTION_HEAVY ? 178 : 174), reach, 60};
}

void GameParticles(Game *g, Vector2 position, Color color, int count, float force) {
    for (int i = 0; i < MAX_PARTICLES && count > 0; ++i) {
        Particle *p = &g->particles[i];
        if (p->life > 0)
            continue;
        float angle = RandomFloat(g) * 2 * PI;
        float speed = force * (0.3f + RandomFloat(g) * 0.7f);
        *p = (Particle){.position = position,
                        .velocity = {cosf(angle) * speed, sinf(angle) * speed},
                        .life = 0.25f + RandomFloat(g) * 0.3f,
                        .radius = 2 + RandomFloat(g) * 3,
                        .color = color};
        p->maxLife = p->life;
        count--;
    }
}

static void ResetRound(Game *g) {
    int wins[2] = {g->fighters[0].wins, g->fighters[1].wins};
    memset(g->fighters, 0, sizeof(g->fighters));
    memset(g->projectiles, 0, sizeof(g->projectiles));
    memset(g->brains, 0, sizeof(g->brains));
    for (int i = 0; i < 2; ++i) {
        g->fighters[i] = (Fighter){.position = {i == 0 ? 365 : 915, GROUND_Y},
                                   .health = MAX_HEALTH,
                                   .healthTrail = MAX_HEALTH,
                                   .energy = 40,
                                   .facing = i == 0 ? 1 : -1,
                                   .grounded = true,
                                   .wins = wins[i]};
    }
    g->roundTime = 75;
    g->phaseTime = 0;
    g->phase = ROUND_INTRO;
    g->winner = -1;
    g->hitstop = 0;
    g->shake = 0;
}
void GameStart(Game *g, MatchMode mode, int difficulty, uint32_t seed) {
    memset(g, 0, sizeof(*g));
    g->mode = mode;
    g->difficulty = (int)ClampValue((float)difficulty, 0, 2);
    g->random = seed ? seed : 1;
    g->round = 1;
    ResetRound(g);
}
void GameNextRound(Game *g) {
    g->round++;
    ResetRound(g);
}

static void BeginAction(Game *g, Fighter *f, Action action, float direction) {
    f->action = action;
    f->actionTime = 0;
    f->connected = false;
    f->blocking = false;
    if (action == ACTION_DASH) {
        if (direction != 0)
            f->facing = direction > 0 ? 1 : -1;
        f->energy -= 12;
        f->velocity.x = f->facing * 1050;
        f->invincible = 0.14f;
        EmitSound(g, EVENT_DASH);
    } else if (action == ACTION_SPECIAL)
        f->energy -= 35;
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
        EmitSound(g, EVENT_BURST);
        return;
    }
}

static void UpdateFighter(Game *g, int index, FighterInput input, float dt) {
    Fighter *f = &g->fighters[index];
    Fighter *opponent = &g->fighters[1 - index];
    f->stun = fmaxf(0, f->stun - dt);
    f->invincible = fmaxf(0, f->invincible - dt);
    f->comboTime = fmaxf(0, f->comboTime - dt);
    f->bufferTime = fmaxf(0, f->bufferTime - dt);
    if (input.special || input.heavy || input.light) {
        f->bufferedAction = input.special ? ACTION_SPECIAL : input.heavy ? ACTION_HEAVY : ACTION_LIGHT;
        f->bufferTime = .14f;
    }
    if (f->comboTime <= 0)
        f->combo = 0;
    f->energy = fminf(100, f->energy + dt * 5);
    f->healthTrail = Approach(f->healthTrail, f->health, dt * 180);
    input.move = ClampValue(input.move, -1, 1);
    if (f->stun <= 0 && f->action == ACTION_IDLE) {
        f->facing = f->position.x < opponent->position.x ? 1 : -1;
        f->blocking = input.block && f->grounded && f->energy > 0;
        if (f->blocking)
            f->energy = fmaxf(0, f->energy - dt * 6);
        if (input.dash && f->energy >= 12)
            BeginAction(g, f, ACTION_DASH, input.move);
        else if (f->bufferTime > 0 && (!f->blocking || input.heavy || input.light || input.special)) {
            if (f->bufferedAction != ACTION_SPECIAL || f->energy >= 35) {
                BeginAction(g, f, f->bufferedAction, 0);
                f->bufferTime = 0;
            }
        }
        if (input.jump && f->grounded && f->action == ACTION_IDLE && !f->blocking) {
            f->velocity.y = -860;
            f->grounded = false;
        }
    } else
        f->blocking = false;
    if (f->stun <= 0 && f->action == ACTION_IDLE) {
        float speed = f->blocking ? 85 : 320;
        f->velocity.x = Approach(f->velocity.x, input.move * speed, dt * 2500);
    } else if (f->action != ACTION_DASH)
        f->velocity.x = Approach(f->velocity.x, 0, dt * 1300);
    if (f->action != ACTION_IDLE) {
        float previous = f->actionTime;
        f->actionTime += dt;
        if (f->action == ACTION_SPECIAL && previous < 0.24f && f->actionTime >= 0.24f)
            SpawnProjectile(g, index);
        if (f->actionTime >= ActionDuration(f->action))
            f->action = ACTION_IDLE;
    }
    f->velocity.y += 1900 * dt;
    f->position.x = ClampValue(f->position.x + f->velocity.x * dt, 85, ARENA_W - 85);
    f->position.y += f->velocity.y * dt;
    if (f->position.y >= GROUND_Y) {
        f->position.y = GROUND_Y;
        f->velocity.y = 0;
        f->grounded = true;
    }
}

static void Hit(Game *g, int attacker, float damage, float push, Vector2 impact, bool heavy) {
    Fighter *target = &g->fighters[1 - attacker];
    Fighter *source = &g->fighters[attacker];
    if (target->invincible > 0 || target->health <= 0)
        return;
    bool guarded = target->blocking && ((target->position.x < impact.x) == (target->facing > 0));
    if (guarded && target->energy >= 7) {
        target->health = fmaxf(1, target->health - damage * 0.07f);
        target->energy -= 7;
        target->velocity.x += push * 0.3f;
        GameParticles(g, impact, (Color){157, 216, 255, 255}, 10, 160);
        g->hitstop = fmaxf(g->hitstop, 0.025f);
        EmitSound(g, EVENT_BLOCK);
    } else {
        target->health = fmaxf(0, target->health - damage);
        target->stun = heavy ? 0.3f : 0.17f;
        target->hurtDuration = target->stun;
        target->hurtDirection = push >= 0 ? 1 : -1;
        target->blocking = false;
        target->action = ACTION_IDLE;
        target->velocity.x = push;
        source->combo++;
        source->comboTime = 1.2f;
        source->energy = fminf(100, source->energy + (heavy ? 10 : 7));
        GameParticles(g, impact, (Color){255, 221, 145, 255}, heavy ? 24 : 15, heavy ? 400 : 250);
        g->hitstop = fmaxf(g->hitstop, heavy ? 0.075f : 0.045f);
        g->shake = fmaxf(g->shake, heavy ? 7 : 4);
        EmitSound(g, heavy ? EVENT_HEAVY : EVENT_LIGHT);
    }
}

FighterInput GameAIInput(Game *g, int index, float dt) {
    Brain *brain = &g->brains[index];
    Fighter *self = &g->fighters[index];
    Fighter *target = &g->fighters[1 - index];
    FighterInput input = {0};
    brain->decision -= dt;
    brain->block = fmaxf(0, brain->block - dt);
    brain->jumpCooldown = fmaxf(0, brain->jumpCooldown - dt);
    float distance = fabsf(self->position.x - target->position.x);
    if (brain->decision <= 0) {
        brain->decision = 0.17f - g->difficulty * 0.04f + RandomFloat(g) * 0.1f;
        float towards = self->position.x < target->position.x ? 1 : -1;
        brain->move = distance > 92 ? towards : distance < 65 ? -towards * 0.5f : 0;
        float roll = RandomFloat(g);
        if (target->action != ACTION_IDLE && distance < 170 && roll < 0.22f + g->difficulty * 0.2f) {
            brain->block = 0.25f;
        } else if (distance < 95 && roll < 0.55f)
            input.light = true;
        else if (distance < 130 && roll < 0.8f)
            input.heavy = true;
        else if (distance > 230 && self->energy >= 35 && roll < 0.34f)
            input.special = true;
        else if (distance > 300 && self->energy > 18 && roll > 0.75f)
            input.dash = true;
        if (brain->jumpCooldown <= 0 && RandomFloat(g) < 0.12f) {
            input.jump = true;
            brain->jumpCooldown = 1;
        }
    }
    input.move = brain->move;
    input.block = brain->block > 0;
    return input;
}

void GameUpdate(Game *g, FighterInput player, FighterInput opponent, float dt) {
    if (g->paused || dt <= 0)
        return;
    g->soundEvents = 0;
    g->totalTime += dt;
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        Particle *p = &g->particles[i];
        if (p->life <= 0)
            continue;
        p->life -= dt;
        p->velocity.y += dt * 450;
        p->position.x += p->velocity.x * dt;
        p->position.y += p->velocity.y * dt;
    }
    g->shake = Approach(g->shake, 0, dt * 22);
    if (g->phase == ROUND_INTRO) {
        g->phaseTime += dt;
        if (g->phaseTime >= 2.2f) {
            g->phase = ROUND_FIGHT;
            g->phaseTime = 0;
        }
        return;
    }
    if (g->phase == ROUND_END) {
        g->phaseTime += dt;
        if (g->phaseTime > 2.6f) {
            if (g->fighters[0].wins >= 2 || g->fighters[1].wins >= 2)
                g->phase = MATCH_END;
            else
                GameNextRound(g);
        }
        return;
    }
    if (g->phase == MATCH_END)
        return;
    if (g->hitstop > 0) {
        g->hitstop = fmaxf(0, g->hitstop - dt);
        return;
    }
    if (g->mode == MODE_SOLO || g->mode == MODE_DEMO)
        opponent = GameAIInput(g, 1, dt);
    if (g->mode == MODE_DEMO)
        player = GameAIInput(g, 0, dt);
    if (g->mode == MODE_TRAINING) {
        g->fighters[0].energy = 100;
        if (g->fighters[1].health <= 0 || g->fighters[0].comboTime <= 0)
            g->fighters[1].health = MAX_HEALTH;
    }
    FighterInput inputs[2] = {player, opponent};
    for (int i = 0; i < 2; ++i)
        UpdateFighter(g, i, inputs[i], dt);
    Fighter *a = &g->fighters[0], *b = &g->fighters[1];
    float separation = b->position.x - a->position.x;
    if (fabsf(separation) < 58 && fabsf(a->position.y - b->position.y) < 90 && a->action != ACTION_DASH &&
        b->action != ACTION_DASH) {
        float push = (58 - fabsf(separation)) * 0.5f;
        float sign = separation >= 0 ? 1 : -1;
        a->position.x = ClampValue(a->position.x - sign * push, 85, ARENA_W - 85);
        b->position.x = ClampValue(b->position.x + sign * push, 85, ARENA_W - 85);
    }
    for (int i = 0; i < 2; ++i) {
        Fighter *f = &g->fighters[i], *target = &g->fighters[1 - i];
        if (FighterAttackActive(f) && !f->connected && target->invincible <= 0 &&
            CheckCollisionRecs(FighterAttackbox(f), FighterHurtbox(target))) {
            f->connected = true;
            bool heavy = f->action == ACTION_HEAVY;
            Vector2 impact = {target->position.x - f->facing * 18, target->position.y - 85};
            Hit(g, i, heavy ? 118 : 64, f->facing * (heavy ? 450 : 270), impact, heavy);
        }
    }
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
        for (int j = i + 1; j < MAX_PROJECTILES; ++j) {
            Projectile *other = &g->projectiles[j];
            if (other->active && other->owner != p->owner && fabsf(other->position.x - p->position.x) < 40 &&
                fabsf(other->position.y - p->position.y) < 35) {
                p->active = other->active = false;
                GameParticles(g, p->position, (Color){200, 210, 255, 255}, 24, 300);
                EmitSound(g, EVENT_BLOCK);
                break;
            }
        }
        Fighter *target = &g->fighters[1 - p->owner];
        if (p->active && target->invincible <= 0 &&
            CheckCollisionCircleRec(p->position, 18, FighterHurtbox(target))) {
            Hit(g, p->owner, 105, p->velocity > 0 ? 400 : -400, p->position, true);
            p->active = false;
        }
    }
    if (g->mode == MODE_TRAINING)
        return;
    g->roundTime = fmaxf(0, g->roundTime - dt);
    if (a->health <= 0 || b->health <= 0 || g->roundTime <= 0) {
        g->winner = a->health == b->health ? -1 : a->health > b->health ? 0 : 1;
        if (g->winner >= 0)
            g->fighters[g->winner].wins++;
        g->phase = ROUND_END;
        g->phaseTime = 0;
        EmitSound(g, EVENT_KO);
    }
}
