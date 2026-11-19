#include "fx.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const Color WHITE_HOT = {255, 252, 240, 255};
static const Color GUARD_BLUE = {150, 214, 255, 255};
static const Color GOLD_FX = {255, 214, 120, 255};
static const Color DUST = {196, 184, 200, 255};

float FxRandom(Fx *fx) {
    uint32_t x = fx->seed ? fx->seed : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    fx->seed = x;
    return (float)(x & 0xffffffu) / 16777215.0f;
}
static float Range(Fx *fx, float lo, float hi) {
    return lo + (hi - lo) * FxRandom(fx);
}
static Color Mix(Color a, Color b, float t) {
    return (Color){(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                   (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}

Color FighterColor(const Game *g, int index) {
    const CharacterInfo *c = CharacterGet(g->fighters[index].character);
    bool mirror = index == 1 && g->fighters[0].character == g->fighters[1].character;
    if (!mirror)
        return c->accent;
    return g->fighters[1].character == CHAR_REN ? (Color){255, 200, 110, 255} : (Color){190, 160, 255, 255};
}

void FxReset(Fx *fx) {
    uint32_t seed = fx->seed;
    memset(fx, 0, sizeof(*fx));
    fx->seed = seed ? seed : 1234567u;
    fx->camCenter = (Vector2){ARENA_W / 2, ARENA_H / 2};
    fx->camZoom = 1;
    fx->thunderDelay = 5;
}

// ---------------------------------------------------------------------------------------------
// Spawners
// ---------------------------------------------------------------------------------------------
static FxParticle *NewParticle(Fx *fx) {
    FxParticle *oldest = &fx->particles[0];
    for (int i = 0; i < FX_PARTICLES; ++i) {
        if (fx->particles[i].life <= 0)
            return &fx->particles[i];
        if (fx->particles[i].life < oldest->life)
            oldest = &fx->particles[i];
    }
    return oldest;
}
void FxBurst(Fx *fx, Vector2 pos, Color color, int count, float force, ParticleKind kind) {
    for (int i = 0; i < count; ++i) {
        FxParticle *p = NewParticle(fx);
        float angle = Range(fx, 0, 2 * PI);
        float speed = force * Range(fx, 0.25f, 1);
        *p = (FxParticle){.pos = pos,
                          .vel = {cosf(angle) * speed, sinf(angle) * speed},
                          .life = Range(fx, 0.2f, 0.5f),
                          .size = Range(fx, 2, 4.5f),
                          .angle = angle,
                          .spin = Range(fx, -8, 8),
                          .drag = 3,
                          .gravity = 600,
                          .color = color,
                          .kind = (unsigned char)kind,
                          .additive = kind == PK_SPARK || kind == PK_EMBER || kind == PK_BOLT};
        switch (kind) {
        case PK_DUST:
            p->vel.y = -fabsf(p->vel.y) * 0.35f;
            p->vel.x *= 1.3f;
            p->life = Range(fx, 0.35f, 0.75f);
            p->size = Range(fx, 9, 20);
            p->gravity = -40;
            p->drag = 4;
            break;
        case PK_EMBER:
            p->vel.y = -fabsf(p->vel.y) - force * 0.4f;
            p->life = Range(fx, 0.4f, 0.9f);
            p->gravity = -120;
            p->drag = 1.5f;
            p->size = Range(fx, 2, 4);
            break;
        case PK_SHARD:
            p->size = Range(fx, 5, 11);
            p->life = Range(fx, 0.35f, 0.7f);
            p->gravity = 1300;
            p->drag = 1.2f;
            break;
        case PK_SPLASH:
            p->vel.y = -fabsf(p->vel.y);
            p->life = Range(fx, 0.18f, 0.32f);
            p->size = Range(fx, 1.5f, 2.5f);
            p->gravity = 1400;
            break;
        default:
            break;
        }
        p->maxLife = p->life;
    }
}
static void Ring(Fx *fx, Vector2 pos, float radius, float thickness, float life, Color color) {
    for (int i = 0; i < FX_RINGS; ++i)
        if (fx->rings[i].life <= 0) {
            fx->rings[i] = (FxRing){pos, life, life, radius, thickness, color};
            return;
        }
}
static void Star(Fx *fx, Vector2 pos, float size, float life, Color color) {
    for (int i = 0; i < FX_STARS; ++i)
        if (fx->stars[i].life <= 0) {
            fx->stars[i] = (FxStar){pos, life, life, size, Range(fx, 0, 90), color};
            return;
        }
}
static void Streaks(Fx *fx, Vector2 pos, int count, float length, Color color, float baseAngle, float spread) {
    for (int n = 0; n < count; ++n)
        for (int i = 0; i < FX_STREAKS; ++i)
            if (fx->streaks[i].life <= 0) {
                float a = baseAngle + Range(fx, -spread, spread);
                float life = Range(fx, 0.1f, 0.2f);
                fx->streaks[i] = (FxStreak){pos, {cosf(a), sinf(a)}, life, life, length * Range(fx, 0.6f, 1.2f),
                                            Range(fx, 2, 5), color};
                break;
            }
}
static void Popup(Fx *fx, const char *text, Vector2 pos, float size, Color color) {
    int slot = 0;
    for (int i = 0; i < FX_POPUPS; ++i)
        if (fx->popups[i].life < fx->popups[slot].life)
            slot = i;
    FxPopup *p = &fx->popups[slot];
    snprintf(p->text, sizeof(p->text), "%s", text);
    p->pos = pos;
    p->life = p->maxLife = 1.0f;
    p->size = size;
    p->color = color;
}
static void Ghost(Fx *fx, const Fighter *f, int index, Color color, float life) {
    int slot = 0;
    for (int i = 0; i < FX_GHOSTS; ++i)
        if (fx->ghosts[i].life < fx->ghosts[slot].life)
            slot = i;
    fx->ghosts[slot] = (FxGhost){*f, index, life, life, color};
}
static void AddTrauma(Fx *fx, float amount) {
    fx->trauma = fminf(1, fx->trauma + amount);
}

// ---------------------------------------------------------------------------------------------
// Event reactions
// ---------------------------------------------------------------------------------------------
void FxEvents(Fx *fx, const Game *g, const GameEvent *events, int count) {
    for (int i = 0; i < count; ++i) {
        const GameEvent *e = &events[i];
        int owner = e->owner >= 0 && e->owner < 2 ? e->owner : 0;
        Color accent = FighterColor(g, owner);
        float s = e->strength;
        float facing = (float)g->fighters[owner].facing;
        switch (e->type) {
        case EV_HIT:
        case EV_SUPER_HIT: {
            Color hot = Mix(accent, WHITE_HOT, 0.45f);
            FxBurst(fx, e->position, hot, (int)(10 + s * 7), 260 + s * 110, PK_SPARK);
            FxBurst(fx, e->position, accent, (int)(4 + s * 3), 200 + s * 80, PK_EMBER);
            Star(fx, e->position, 24 + s * 12, 0.08f + s * 0.02f, Fade(Mix(accent, WHITE_HOT, 0.6f), 0.85f));
            Streaks(fx, e->position, (int)(3 + s * 2), 60 + s * 30, hot, facing > 0 ? 0 : PI, 0.6f);
            if (s >= 2)
                Ring(fx, e->position, 70 + s * 30, 4 + s, 0.22f, accent);
            AddTrauma(fx, 0.1f + s * 0.07f);
            fx->zoomPunch = fmaxf(fx->zoomPunch, 0.012f * s);
            fx->punchFocus = e->position;
            if (s >= 3) {
                fx->flash = fmaxf(fx->flash, s >= 4 ? 0.45f : 0.18f);
                fx->flashColor = WHITE_HOT;
            }
            break;
        }
        case EV_COUNTER:
            Popup(fx, "COUNTER", (Vector2){e->position.x, e->position.y - 90}, 34, GOLD_FX);
            Ring(fx, e->position, 120, 6, 0.28f, GOLD_FX);
            AddTrauma(fx, 0.15f);
            break;
        case EV_BLOCK:
            FxBurst(fx, e->position, GUARD_BLUE, (int)(8 + s * 3), 220, PK_SPARK);
            Ring(fx, e->position, 46 + s * 10, 3, 0.16f, GUARD_BLUE);
            AddTrauma(fx, 0.05f + s * 0.02f);
            break;
        case EV_GUARD_BREAK:
            FxBurst(fx, e->position, GUARD_BLUE, 26, 520, PK_SHARD);
            Ring(fx, e->position, 170, 8, 0.35f, GUARD_BLUE);
            Popup(fx, "GUARD BREAK", (Vector2){e->position.x, e->position.y - 110}, 38, GUARD_BLUE);
            AddTrauma(fx, 0.45f);
            fx->flash = 0.25f;
            fx->flashColor = GUARD_BLUE;
            break;
        case EV_LAUNCH:
            Streaks(fx, e->position, 6, 110, Mix(accent, WHITE, 0.4f), -PI / 2, 0.5f);
            break;
        case EV_WALL_BOUNCE:
            FxBurst(fx, e->position, DUST, 18, 300, PK_DUST);
            FxBurst(fx, e->position, (Color){200, 190, 180, 255}, 14, 600, PK_SHARD);
            Ring(fx, e->position, 140, 7, 0.3f, WHITE_HOT);
            Popup(fx, "WALL BOUNCE", (Vector2){e->position.x < ARENA_W / 2 ? 230 : ARENA_W - 230, e->position.y - 60},
                  30, accent);
            AddTrauma(fx, 0.4f);
            break;
        case EV_KNOCKDOWN:
            FxBurst(fx, (Vector2){e->position.x, GROUND_Y - 6}, DUST, 22, 340, PK_DUST);
            AddTrauma(fx, 0.25f);
            break;
        case EV_LAND:
            FxBurst(fx, (Vector2){e->position.x, GROUND_Y - 4}, DUST, s >= 2 ? 12 : 5, s >= 2 ? 260 : 150, PK_DUST);
            if (s >= 2)
                AddTrauma(fx, 0.12f);
            break;
        case EV_JUMP:
            FxBurst(fx, (Vector2){e->position.x, GROUND_Y - 4}, DUST, 5, 140, PK_DUST);
            if (s >= 3) {
                FxBurst(fx, (Vector2){e->position.x, GROUND_Y - 30}, accent, 26, 380, PK_EMBER);
                Ring(fx, (Vector2){e->position.x, GROUND_Y - 10}, 90, 5, 0.25f, accent);
            }
            break;
        case EV_DASH:
            FxBurst(fx, (Vector2){e->position.x, GROUND_Y - 4}, DUST, 8, 220, PK_DUST);
            Streaks(fx, (Vector2){e->position.x, e->position.y - 100}, 4, 90, Fade(accent, 0.8f),
                    g->fighters[owner].velocity.x > 0 ? PI : 0, 0.12f);
            break;
        case EV_FIRE:
            Ring(fx, e->position, 60, 4, 0.18f, accent);
            FxBurst(fx, e->position, accent, 14, 260, PK_SPARK);
            Star(fx, e->position, 50, 0.1f, WHITE_HOT);
            AddTrauma(fx, 0.08f);
            break;
        case EV_CLASH:
            Ring(fx, e->position, 200, 9, 0.4f, WHITE_HOT);
            Ring(fx, e->position, 120, 5, 0.3f, accent);
            FxBurst(fx, e->position, WHITE_HOT, 40, 520, PK_SPARK);
            Star(fx, e->position, 80, 0.18f, WHITE_HOT);
            Popup(fx, "CLASH", (Vector2){e->position.x, e->position.y - 80}, 34, WHITE_HOT);
            AddTrauma(fx, 0.35f);
            fx->flash = 0.2f;
            fx->flashColor = WHITE_HOT;
            break;
        case EV_SUPER_FLASH:
            fx->superTime = SUPER_FREEZE;
            fx->superOwner = owner;
            fx->flash = 0.5f;
            fx->flashColor = Mix(accent, WHITE, 0.5f);
            Ring(fx, e->position, 260, 10, 0.5f, accent);
            FxBurst(fx, e->position, accent, 60, 500, PK_EMBER);
            AddTrauma(fx, 0.3f);
            break;
        case EV_KO:
            fx->slowmo = 1.6f;
            fx->koFocus = e->position;
            fx->flash = 0.8f;
            fx->flashColor = WHITE_HOT;
            Ring(fx, e->position, 320, 12, 0.6f, WHITE_HOT);
            Ring(fx, e->position, 200, 6, 0.45f, GOLD_FX);
            FxBurst(fx, e->position, GOLD_FX, 60, 700, PK_SPARK);
            Streaks(fx, e->position, 16, 180, WHITE_HOT, 0, PI);
            AddTrauma(fx, 0.9f);
            break;
        case EV_FIGHT:
            fx->fightTime = 0.7f;
            fx->flash = 0.3f;
            fx->flashColor = GOLD_FX;
            Ring(fx, (Vector2){ARENA_W / 2, 320}, 420, 8, 0.5f, GOLD_FX);
            break;
        default:
            break;
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Simulation of presentation state
// ---------------------------------------------------------------------------------------------
float FxTimeScale(const Fx *fx) {
    if (fx->slowmo <= 0)
        return 1;
    // Hold a heavy slow-motion, then ease back to full speed.
    return fx->slowmo > 0.5f ? 0.22f : 0.22f + (0.5f - fx->slowmo) / 0.5f * 0.78f;
}

void FxUpdate(Fx *fx, const Game *g, float dt, bool reducedMotion) {
    float scaled = dt * FxTimeScale(fx);
    if (g->hitstop > 0 || g->superFreeze > 0)
        scaled *= 0.35f; // particles hang in the air during impact freeze
    fx->time += dt;
    fx->trauma = fmaxf(0, fx->trauma - dt * 1.7f);
    fx->zoomPunch = fmaxf(0, fx->zoomPunch - dt * 0.12f);
    fx->flash = fmaxf(0, fx->flash - dt * 3.2f);
    fx->slowmo = fmaxf(0, fx->slowmo - dt);
    fx->superTime = fmaxf(0, fx->superTime - dt);
    fx->fightTime = fmaxf(0, fx->fightTime - dt);
    fx->lightning = fmaxf(0, fx->lightning - dt * 2.4f);
    fx->thunder = false;
    if (g->stage == STAGE_STORM_TERRACE && !reducedMotion) {
        fx->thunderDelay -= dt;
        if (fx->thunderDelay <= 0) {
            fx->lightning = 1;
            fx->thunder = true;
            fx->thunderDelay = Range(fx, 5, 11);
        }
    }
    for (int i = 0; i < FX_PARTICLES; ++i) {
        FxParticle *p = &fx->particles[i];
        if (p->life <= 0)
            continue;
        p->life -= scaled;
        float drag = expf(-p->drag * scaled);
        p->vel.x *= drag;
        p->vel.y = p->vel.y * drag + p->gravity * scaled;
        p->pos.x += p->vel.x * scaled;
        p->pos.y += p->vel.y * scaled;
        p->angle += p->spin * scaled;
        if (p->kind != PK_DUST && p->kind != PK_PETAL && p->pos.y > GROUND_Y + 4 && p->vel.y > 0) {
            p->pos.y = GROUND_Y + 4;
            p->vel.y *= -0.3f;
            p->vel.x *= 0.6f;
        }
    }
    for (int i = 0; i < FX_RINGS; ++i)
        fx->rings[i].life -= scaled;
    for (int i = 0; i < FX_STARS; ++i)
        fx->stars[i].life -= scaled;
    for (int i = 0; i < FX_STREAKS; ++i)
        fx->streaks[i].life -= scaled;
    for (int i = 0; i < FX_GHOSTS; ++i)
        fx->ghosts[i].life -= scaled;
    for (int i = 0; i < FX_POPUPS; ++i) {
        fx->popups[i].life -= dt;
        fx->popups[i].pos.y -= dt * 40;
    }
    // Afterimages, auras and per-move particles.
    for (int i = 0; i < 2; ++i) {
        const Fighter *f = &g->fighters[i];
        Color accent = FighterColor(g, i);
        fx->ghostTimer[i] -= scaled;
        bool fast = f->action == ACTION_DASH || f->action == ACTION_BACKDASH || f->action == ACTION_DIVE ||
                    (f->action == ACTION_SPECIAL && f->character == CHAR_KAE && !f->grounded) ||
                    (f->action == ACTION_SUPER && f->character == CHAR_KAE && g->superFreeze <= 0) ||
                    (f->launched && fabsf(f->velocity.x) > 420);
        if (fast && fx->ghostTimer[i] <= 0) {
            fx->ghostTimer[i] = 0.028f;
            Ghost(fx, f, i, accent, 0.24f);
        }
        if (f->action == ACTION_SPECIAL && f->character == CHAR_KAE && !f->grounded && FxRandom(fx) < 0.8f)
            FxBurst(fx, (Vector2){f->position.x + f->facing * 30, f->position.y - 150}, accent, 2, 160, PK_EMBER);
        if (f->action == ACTION_SUPER && f->character == CHAR_KAE && f->connected && FxRandom(fx) < 0.6f)
            FxBurst(fx, (Vector2){f->position.x + f->facing * 60, f->position.y - 130}, accent, 2, 260, PK_EMBER);
        fx->auraTimer[i] -= dt;
        if (f->energy >= SUPER_COST && fx->auraTimer[i] <= 0 && f->health > 0) {
            fx->auraTimer[i] = 0.05f;
            FxBurst(fx, (Vector2){f->position.x + Range(fx, -30, 30), f->position.y - Range(fx, 10, 170)}, accent,
                    1, 60, PK_EMBER);
        }
        // Combo counter presentation.
        if (f->combo >= 2) {
            if (f->combo != fx->comboShown[i])
                fx->comboPop[i] = 1;
            fx->comboShown[i] = f->combo;
            fx->comboDamage[i] = f->comboDamage;
            fx->comboTimer[i] = 1.8f;
        } else
            fx->comboTimer[i] = fmaxf(0, fx->comboTimer[i] - dt);
        fx->comboPop[i] = fmaxf(0, fx->comboPop[i] - dt * 5);
    }

    // Camera: frame both fighters, lean in when close, punch toward impacts.
    const Fighter *a = &g->fighters[0], *b = &g->fighters[1];
    float distance = fabsf(a->position.x - b->position.x);
    float zoom = 1 + fminf(0.14f, fmaxf(0, (560 - distance) / 560 * 0.14f));
    Vector2 focus = {(a->position.x + b->position.x) / 2, 0};
    if (fx->superTime > 0) {
        const Fighter *s = &g->fighters[fx->superOwner];
        zoom = 1.32f;
        focus.x = s->position.x + s->facing * 60;
    } else if (fx->slowmo > 0.3f) {
        zoom = 1.28f;
        focus.x = fx->koFocus.x;
    }
    zoom += fx->zoomPunch;
    if (fx->zoomPunch > 0)
        focus.x += (fx->punchFocus.x - focus.x) * fminf(1, fx->zoomPunch * 6);
    if (reducedMotion)
        zoom = 1, focus.x = ARENA_W / 2;
    float k = 1 - expf(-dt * (fx->superTime > 0 || fx->slowmo > 0 ? 10 : 5));
    fx->camZoom += (zoom - fx->camZoom) * k;
    fx->camCenter.x += (focus.x - fx->camCenter.x) * k;
    float halfW = ARENA_W / 2 / fx->camZoom;
    fx->camCenter.x = fmaxf(halfW, fminf(ARENA_W - halfW, fx->camCenter.x));
    fx->camCenter.y = GROUND_Y - 230 / fx->camZoom;
}

Camera2D FxCamera(const Fx *fx, float density, bool reducedMotion) {
    float shake = reducedMotion ? 0 : fx->trauma * fx->trauma;
    float t = fx->time;
    Vector2 jitter = {(sinf(t * 83.1f) + sinf(t * 47.3f + 1.3f)) * 0.5f * 16 * shake,
                      (sinf(t * 71.7f + 2.1f) + sinf(t * 39.1f)) * 0.5f * 11 * shake};
    return (Camera2D){.offset = {ARENA_W / 2 * density, ARENA_H / 2 * density},
                      .target = {fx->camCenter.x + jitter.x, fx->camCenter.y + jitter.y},
                      .rotation = sinf(t * 57.0f) * 1.2f * shake,
                      .zoom = fx->camZoom * density};
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------
static void DrawStarShape(Vector2 c, float size, float angle, Color color) {
    for (int k = 0; k < 4; ++k) {
        float a = (angle + k * 90) * DEG2RAD;
        Vector2 tip = {c.x + cosf(a) * size, c.y + sinf(a) * size};
        Vector2 l = {c.x + cosf(a + 1.4f) * size * 0.14f, c.y + sinf(a + 1.4f) * size * 0.14f};
        Vector2 r = {c.x + cosf(a - 1.4f) * size * 0.14f, c.y + sinf(a - 1.4f) * size * 0.14f};
        DrawTriangle(tip, l, r, color);
        DrawTriangle(tip, r, l, color);
    }
}

void FxDrawWorld(const Fx *fx, bool front) {
    if (!front) {
        for (int i = 0; i < FX_PARTICLES; ++i) {
            const FxParticle *p = &fx->particles[i];
            if (p->life <= 0 || p->kind != PK_DUST)
                continue;
            float t = p->life / p->maxLife;
            DrawCircleV(p->pos, p->size * (1.6f - t * 0.6f), Fade(p->color, 0.32f * t));
        }
        return;
    }
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < FX_RINGS; ++i) {
        const FxRing *r = &fx->rings[i];
        if (r->life <= 0)
            continue;
        float t = 1 - r->life / r->maxLife;
        float radius = r->radius * (0.25f + 0.75f * sqrtf(t));
        float width = r->thickness * (1 - t) + 1;
        DrawRing(r->pos, radius - width, radius, 0, 360, 48, Fade(r->color, 0.9f * (1 - t)));
    }
    for (int i = 0; i < FX_STREAKS; ++i) {
        const FxStreak *s = &fx->streaks[i];
        if (s->life <= 0)
            continue;
        float t = 1 - s->life / s->maxLife;
        Vector2 a = {s->pos.x + s->dir.x * s->length * t * 0.8f, s->pos.y + s->dir.y * s->length * t * 0.8f};
        Vector2 b = {a.x + s->dir.x * s->length * (1 - t), a.y + s->dir.y * s->length * (1 - t)};
        DrawLineEx(a, b, s->width * (1 - t) + 0.5f, Fade(s->color, 1 - t));
    }
    for (int i = 0; i < FX_PARTICLES; ++i) {
        const FxParticle *p = &fx->particles[i];
        if (p->life <= 0 || !p->additive)
            continue;
        float t = p->life / p->maxLife;
        if (p->kind == PK_SPARK) {
            Vector2 tail = {p->pos.x - p->vel.x * 0.03f, p->pos.y - p->vel.y * 0.03f};
            DrawLineEx(p->pos, tail, p->size * t + 0.5f, Fade(p->color, t));
        } else {
            DrawCircleV(p->pos, p->size * (0.5f + t), Fade(p->color, 0.25f * t));
            DrawCircleV(p->pos, p->size * 0.5f * (0.5f + t), Fade(p->color, t));
        }
    }
    for (int i = 0; i < FX_STARS; ++i) {
        const FxStar *s = &fx->stars[i];
        if (s->life <= 0)
            continue;
        float t = s->life / s->maxLife;
        DrawCircleV(s->pos, s->size * 0.3f * t, Fade(s->color, 0.35f * t));
        DrawStarShape(s->pos, s->size * (1.2f - t * 0.4f), s->angle, Fade(s->color, 0.8f * t));
    }
    EndBlendMode();
    for (int i = 0; i < FX_PARTICLES; ++i) {
        const FxParticle *p = &fx->particles[i];
        if (p->life <= 0 || p->additive || p->kind == PK_DUST)
            continue;
        float t = p->life / p->maxLife;
        if (p->kind == PK_SHARD) {
            Vector2 c = p->pos;
            float s = p->size;
            Vector2 v1 = {c.x + cosf(p->angle) * s, c.y + sinf(p->angle) * s};
            Vector2 v2 = {c.x + cosf(p->angle + 2.3f) * s * 0.6f, c.y + sinf(p->angle + 2.3f) * s * 0.6f};
            Vector2 v3 = {c.x + cosf(p->angle + 4.0f) * s * 0.5f, c.y + sinf(p->angle + 4.0f) * s * 0.5f};
            DrawTriangle(v1, v2, v3, Fade(p->color, t));
            DrawTriangle(v1, v3, v2, Fade(p->color, t));
        } else
            DrawCircleV(p->pos, p->size, Fade(p->color, t));
    }
}

void FxDrawScreen(const Fx *fx, const Game *g, bool reducedMotion) {
    if (fx->superTime > 0 && !reducedMotion) {
        // Radial speed lines around the screen edge during a Spirit Art freeze.
        float t = fx->superTime / SUPER_FREEZE;
        Color accent = FighterColor(g, fx->superOwner);
        for (int i = 0; i < 48; ++i) {
            float a = i * (2 * PI / 48) + fx->time * 0.6f;
            float jitter = sinf(i * 12.9898f + fx->time * 40) * 0.5f + 0.5f;
            float inner = 380 + jitter * 120;
            Vector2 c = {ARENA_W / 2, ARENA_H / 2};
            Vector2 p0 = {c.x + cosf(a) * inner, c.y + sinf(a) * inner * 0.62f};
            Vector2 p1 = {c.x + cosf(a) * 900, c.y + sinf(a) * 560};
            DrawLineEx(p0, p1, 2 + jitter * 3, Fade(i % 3 == 0 ? accent : WHITE, 0.35f * t));
        }
    }
    if (fx->flash > 0 && !reducedMotion)
        DrawRectangle(0, 0, (int)ARENA_W, (int)ARENA_H, Fade(fx->flashColor, fminf(0.7f, fx->flash)));
    else if (fx->flash > 0)
        DrawRectangle(0, 0, (int)ARENA_W, (int)ARENA_H, Fade(fx->flashColor, fminf(0.15f, fx->flash * 0.3f)));
}
