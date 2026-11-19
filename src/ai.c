// Computer opponent. It perceives the situation each step, schedules a response after a
// human-like reaction delay, confirms hits into combos and plays to its character's range.
// All randomness comes from the game's seeded generator, so matches replay deterministically.
#include "combat.h"
#include <math.h>

enum { R_NONE, R_BLOCK, R_ANTIAIR, R_JUMP_OVER, R_COUNTERFIRE, R_DASH_THROUGH, R_PUNISH, R_BACKDASH, R_SUPER };
enum { S_NONE, S_ATTACK, S_JUMPIN, S_PROJECTILE, S_WHIFF, S_DASHIN };

static const float REACTION[3] = {0.36f, 0.22f, 0.13f};
static const float ACCURACY[3] = {0.30f, 0.62f, 0.88f};
static const float COMBO_SKILL[3] = {0.3f, 0.6f, 0.97f};
static const float AGGRESSION[3] = {0.40f, 0.58f, 0.74f};
static const float GUARD_READ[3] = {0.25f, 0.45f, 0.8f}; // chance to anticipate pressure
static const float REVERSAL[3] = {0.04f, 0.12f, 0.2f};   // chance to answer pressure with an invincible move
static const float PUNISH[3] = {0.25f, 0.7f, 0.95f};     // chance to punish a blocked heavy or special

static float Sign(float v) {
    return v >= 0 ? 1.0f : -1.0f;
}
static bool Attacking(const Fighter *f) {
    return f->action == ACTION_LIGHT || f->action == ACTION_HEAVY || f->action == ACTION_SUPER ||
           f->action == ACTION_DIVE || f->action == ACTION_AIR ||
           (f->action == ACTION_SPECIAL && f->character == CHAR_KAE);
}
static float ActiveEnd(const Fighter *f) {
    switch (f->action) {
    case ACTION_LIGHT:
        return 0.15f;
    case ACTION_HEAVY:
        return 0.32f;
    case ACTION_SPECIAL:
        return f->character == CHAR_KAE ? 0.32f : 0.3f;
    case ACTION_SUPER:
        return f->character == CHAR_KAE ? 0.45f : 0.82f;
    default:
        return 0.3f;
    }
}

static int Perceive(const Game *g, int index) {
    const Fighter *self = &g->fighters[index];
    const Fighter *t = &g->fighters[1 - index];
    float dx = t->position.x - self->position.x;
    float distance = fabsf(dx);
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        const Projectile *p = &g->projectiles[i];
        if (!p->active || p->owner == index)
            continue;
        float gap = self->position.x - p->position.x;
        if (Sign(p->velocity) == Sign(gap) && fabsf(gap) < 470)
            return S_PROJECTILE;
    }
    if (t->action == ACTION_SUPER && t->character == CHAR_REN)
        return S_ATTACK;
    if (!t->grounded && !t->launched && distance < 280 && t->velocity.y > -320 &&
        (fabsf(t->velocity.x) < 60 || Sign(t->velocity.x) == Sign(-dx)))
        return S_JUMPIN;
    if (Attacking(t) && distance < 230 && t->actionTime < ActiveEnd(t))
        return S_ATTACK;
    if (t->action == ACTION_DASH && distance < 400 && Sign(t->velocity.x) == Sign(-dx))
        return S_DASHIN;
    bool whiffed = t->action != ACTION_IDLE && t->action != ACTION_DASH && !t->connected &&
                   t->action != ACTION_BACKDASH && t->actionTime > ActiveEnd(t);
    if (((whiffed && t->grounded) || (t->lag > 0.08f && t->grounded)) && distance < 280)
        return S_WHIFF;
    return S_NONE;
}

static int ChooseReaction(Game *g, int index, int stimulus) {
    Fighter *self = &g->fighters[index];
    Fighter *t = &g->fighters[1 - index];
    float distance = fabsf(t->position.x - self->position.x);
    float roll = GameRandom(g);
    bool ren = self->character == CHAR_REN;
    switch (stimulus) {
    case S_ATTACK:
        if (t->action == ACTION_SUPER)
            return roll < 0.5f && self->grounded ? R_JUMP_OVER : R_BLOCK;
        return g->difficulty == 2 && roll < 0.18f && self->energy >= 12 ? R_BACKDASH : R_BLOCK;
    case S_JUMPIN:
        return roll < 0.82f ? R_ANTIAIR : R_BLOCK;
    case S_PROJECTILE:
        if (self->energy >= 100 && !ren && distance < 420 && g->difficulty >= 1 && roll < 0.4f)
            return R_SUPER;
        if (ren && self->energy >= 35 && roll < 0.55f)
            return R_COUNTERFIRE;
        if (distance > 230 && roll < 0.7f)
            return R_JUMP_OVER;
        return R_BLOCK;
    case S_DASHIN:
        return roll < 0.6f ? R_ANTIAIR : R_BLOCK;
    case S_WHIFF:
        return R_PUNISH;
    default:
        return R_NONE;
    }
}

static void Execute(Game *g, int index, int reaction, FighterInput *input) {
    Brain *brain = &g->brains[index];
    Fighter *self = &g->fighters[index];
    Fighter *t = &g->fighters[1 - index];
    float towards = Sign(t->position.x - self->position.x);
    float distance = fabsf(t->position.x - self->position.x);
    switch (reaction) {
    case R_BLOCK:
        brain->block = 0.32f + GameRandom(g) * 0.12f;
        break;
    case R_ANTIAIR:
        if (self->character == CHAR_KAE && self->energy >= SPECIAL_COST)
            input->special = true;
        else
            input->heavy = true;
        break;
    case R_JUMP_OVER:
        input->jump = true;
        brain->move = towards;
        break;
    case R_COUNTERFIRE:
        input->special = true;
        break;
    case R_DASH_THROUGH:
        input->dash = true;
        input->move = towards;
        break;
    case R_BACKDASH:
        input->dash = true;
        input->move = -towards;
        break;
    case R_SUPER:
        input->super = true;
        break;
    case R_PUNISH:
        if (Attacking(t) && t->actionTime < ActiveEnd(t)) {
            brain->block = 0.25f; // they recovered and swung again: keep guarding
            break;
        }
        if (self->energy >= SUPER_COST && g->difficulty >= 1 &&
            (self->character == CHAR_REN || distance < 330))
            input->super = true;
        else if (distance > 125 && self->energy >= DASH_COST) {
            input->dash = true;
            input->move = towards;
            brain->pending = R_PUNISH; // follow up once the dash lands
            brain->reaction = 0.16f;
        } else if (distance <= 125)
            input->light = true;
        else
            brain->move = towards;
        break;
    default:
        break;
    }
}

static void Neutral(Game *g, int index, FighterInput *input) {
    Brain *brain = &g->brains[index];
    Fighter *self = &g->fighters[index];
    Fighter *t = &g->fighters[1 - index];
    int d = g->difficulty;
    bool ren = self->character == CHAR_REN;
    float towards = Sign(t->position.x - self->position.x);
    float distance = fabsf(t->position.x - self->position.x);
    float aggression = AGGRESSION[d] + (self->health < t->health - 250 ? 0.15f : 0);
    float preferred = ren ? (self->energy >= SPECIAL_COST ? 420 : 240) : 150;
    if (t->knockdown > 0 || (t->invincible > 0 && t->grounded)) {
        // Walk up, then either guard their wake-up or time a jab for the first vulnerable frame.
        brain->move = distance > 115 ? towards : 0;
        if (t->knockdown <= 0 && t->invincible < 0.06f && distance < 120) {
            if (GameRandom(g) < 0.35f + d * 0.15f)
                input->light = true;
            else
                brain->block = 0.3f;
        }
        return;
    }
    if (brain->think > 0)
        return;
    brain->think = 0.2f - d * 0.035f + GameRandom(g) * 0.14f;
    if (Attacking(t) && distance < 170 && d > 0) {
        // Never swing into active pressure: that is how counter-hits happen. Guard or step out.
        if (GameRandom(g) < 0.5f + d * 0.2f)
            brain->block = 0.25f;
        else
            brain->move = -towards;
        return;
    }
    float roll = GameRandom(g);
    if (self->energy >= SUPER_COST && d >= 1 && roll < 0.12f + d * 0.06f) {
        bool reachable = ren ? t->grounded && !t->blocking : distance < 300;
        if (reachable && t->knockdown <= 0) {
            input->super = true;
            return;
        }
    }
    roll = GameRandom(g);
    if (distance > preferred + 70) {
        brain->move = towards;
        if (self->energy > 30 && roll < aggression * 0.22f)
            input->dash = true, input->move = towards;
        else if (brain->jumpCooldown <= 0 && roll > 1 - (0.06f + aggression * 0.08f + (ren ? 0 : 0.05f))) {
            input->jump = true;
            brain->jumpCooldown = 1.4f - d * 0.3f;
        } else if (ren && distance > 300 && self->energy >= SPECIAL_COST && roll < 0.42f + d * 0.08f &&
                   t->grounded)
            input->special = true;
    } else if (distance < preferred - 70 && ren) {
        brain->move = -towards;
        if (distance < 115 && roll < 0.45f)
            input->light = true;
        else if (self->energy > 25 && roll > 0.86f)
            input->dash = true, input->move = -towards;
    } else {
        brain->move = (GameRandom(g) - 0.45f) * towards * 0.9f;
        if (ren && distance > 300 && self->energy >= SPECIAL_COST && roll < 0.35f + d * 0.1f && t->grounded)
            input->special = true;
        else if (distance <= 112 && roll < (t->action == ACTION_IDLE ? 0.42f + aggression * 0.2f - d * 0.12f : 0.15f))
            input->light = true;
        else if (distance > 112 && distance < 145 && roll < 0.3f + aggression * 0.15f + d * 0.1f)
            input->heavy = true;
        else if (distance < 140 && roll > 0.9f)
            brain->block = 0.25f;
        else if (!ren && distance < 260 && brain->jumpCooldown <= 0 && roll > 0.82f) {
            input->jump = true;
            brain->move = towards;
            brain->jumpCooldown = 1.2f - d * 0.25f;
        }
    }
    if (d == 0 && GameRandom(g) < 0.12f)
        brain->move = -brain->move; // easy CPU wanders and second-guesses itself
}

FighterInput GameAIInput(Game *g, int index, float dt) {
    Brain *brain = &g->brains[index];
    Fighter *self = &g->fighters[index];
    Fighter *t = &g->fighters[1 - index];
    FighterInput input = {0};
    int d = g->difficulty;
    brain->think -= dt;
    brain->reaction -= dt;
    brain->block = fmaxf(0, brain->block - dt);
    brain->jumpCooldown = fmaxf(0, brain->jumpCooldown - dt);
    brain->seenJumps = fmaxf(0, brain->seenJumps - dt * 0.05f);
    float towards = Sign(t->position.x - self->position.x);
    float distance = fabsf(t->position.x - self->position.x);

    // 1. Confirm hits into combos.
    bool victimLocked = t->stun > 0 || t->launched;
    if (self->connected && victimLocked &&
        (self->action == ACTION_LIGHT || self->action == ACTION_HEAVY || self->action == ACTION_SPECIAL)) {
        if (!brain->chainRolled || brain->chainFor != self->action || brain->chainStamp != (float)self->chain) {
            brain->chainRolled = true;
            brain->chainFor = self->action;
            brain->chainStamp = (float)self->chain;
            brain->chainGo = GameRandom(g) < COMBO_SKILL[d];
            if (brain->chainGo) {
                bool super = self->energy >= SUPER_COST && d >= 1 && GameRandom(g) < 0.6f;
                if (self->action == ACTION_LIGHT)
                    input.light = self->chain < 2, input.heavy = self->chain >= 2;
                if (self->action == ACTION_HEAVY) {
                    if (super)
                        input.super = true;
                    else if (self->energy >= SPECIAL_COST)
                        input.special = true;
                }
                if (self->action == ACTION_SPECIAL && super && self->character == CHAR_KAE)
                    input.super = true;
            }
        }
        return input;
    }
    if (self->action == ACTION_IDLE)
        brain->chainRolled = false;

    // 2. Chase juggles.
    if (t->launched && FighterCanAct(self) && self->grounded) {
        brain->move = towards;
        if (distance < 140 && t->velocity.y > 0 && t->position.y > GROUND_Y - 190 &&
            GameRandom(g) < COMBO_SKILL[d] * 0.4f)
            input.heavy = true;
        input.move = brain->move;
        return input;
    }

    // 3a. Defensive instincts: read pressure before it lands and plan the punish while guarding.
    brain->patience -= dt;
    bool close = distance < 160;
    bool approaching = t->velocity.x * Sign(self->position.x - t->position.x) > 60 || t->action == ACTION_DASH;
    bool startup = Attacking(t) && t->actionTime < ActiveEnd(t);
    if (self->grounded && !self->launched && self->knockdown <= 0 && close && (startup || approaching) &&
        brain->patience <= 0 && self->action == ACTION_IDLE) {
        brain->patience = 0.16f + GameRandom(g) * 0.08f;
        float roll = GameRandom(g);
        if (roll < GUARD_READ[d]) {
            brain->block = fmaxf(brain->block, 0.26f + GameRandom(g) * 0.16f);
        } else if (roll < GUARD_READ[d] + REVERSAL[d] && FighterCanAct(self)) {
            if (self->character == CHAR_KAE && self->energy >= SPECIAL_COST)
                input.special = true;
            else if (self->energy >= SUPER_COST)
                input.super = true;
            else if (self->energy >= DASH_COST)
                input.dash = true, input.move = -towards;
        }
    }
    bool canCancel = t->connected && t->energy >= SPECIAL_COST; // a blocked heavy may still cancel
    bool punishable = (t->action == ACTION_HEAVY && t->actionTime > 0.3f && !canCancel) ||
                      (t->action == ACTION_SPECIAL && t->character == CHAR_KAE && t->actionTime > 0.32f) ||
                      (t->action == ACTION_SPECIAL && t->character == CHAR_REN && t->actionTime > 0.3f && distance < 200) ||
                      t->lag > 0.12f;
    if (punishable && (self->blockstun > 0 || brain->block > 0) && brain->pending != R_PUNISH && distance < 260 &&
        brain->stimulus != S_WHIFF) {
        brain->stimulus = S_WHIFF;
        if (GameRandom(g) < PUNISH[d]) {
            brain->pending = R_PUNISH;
            brain->reaction = 0;
            brain->block = 0;
        }
    }
    if (self->knockdown > 0 && self->knockdown < 0.1f && GameRandom(g) < GUARD_READ[d])
        brain->block = 0.3f; // guard on wake-up

    // 3b. Read the situation and react after a delay.
    int stimulus = Perceive(g, index);
    if (stimulus != S_NONE && stimulus != brain->stimulus) {
        float accuracy = ACCURACY[d];
        if (stimulus == S_JUMPIN)
            accuracy = fminf(0.97f, accuracy + brain->seenJumps * 0.06f), brain->seenJumps += 1;
        if (stimulus == S_PROJECTILE)
            brain->seenShots += 1;
        if (GameRandom(g) < accuracy) {
            brain->pending = ChooseReaction(g, index, stimulus);
            brain->reaction = REACTION[d] * (0.75f + GameRandom(g) * 0.5f);
        }
    }
    brain->stimulus = stimulus;
    if (brain->pending != R_NONE && brain->reaction <= 0 && FighterCanAct(self) && self->action == ACTION_IDLE) {
        int reaction = brain->pending;
        brain->pending = R_NONE;
        Execute(g, index, reaction, &input);
    }

    // 4. Air control: dive or jab on the way down.
    if (!self->grounded && !self->launched && !self->airUsed && self->action == ACTION_IDLE &&
        self->velocity.y > -150) {
        brain->move = towards;
        if (distance < 170 && distance > 70 && self->position.y < GROUND_Y - 80)
            input.heavy = true;
        else if (distance <= 90)
            input.light = true;
    }

    // 5. Footsies.
    bool walkingIn = approaching && distance > 105 && distance < 150 && t->grounded && t->action == ACTION_IDLE;
    if (walkingIn && !Attacking(t) && FighterCanAct(self) && self->action == ACTION_IDLE && brain->block <= 0 &&
        GameRandom(g) < 0.04f + d * 0.05f)
        input.heavy = true; // stuff out a careless walk-in
    if (FighterCanAct(self) && self->action == ACTION_IDLE && self->grounded && brain->block <= 0 &&
        brain->pending == R_NONE && !input.heavy && !input.special && !input.super && !input.dash)
        Neutral(g, index, &input);

    if (input.move == 0)
        input.move = brain->move;
    input.block = brain->block > 0 || self->blockstun > 0;
    if (input.block)
        input.move = 0;
    return input;
}
