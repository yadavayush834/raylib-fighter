#include "ui.h"
#include "stage.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------------------------
// Shape helpers
// ---------------------------------------------------------------------------------------------
static void Tri(Vector2 a, Vector2 b, Vector2 c, Color color) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0) {
        Vector2 t = b;
        b = c;
        c = t;
    }
    DrawTriangle(a, b, c, color);
}
static void Quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color) {
    Tri(a, b, c, color);
    Tri(a, c, d, color);
}
// A parallelogram whose top edge is shifted by `slant` relative to its bottom edge.
static void Para(float x, float y, float w, float h, float slant, Color color) {
    if (w <= 0)
        return;
    Quad((Vector2){x + slant, y}, (Vector2){x + w + slant, y}, (Vector2){x + w, y + h}, (Vector2){x, y + h}, color);
}
static float Ease(float t) {
    t = fmaxf(0, fminf(1, t));
    return 1 - powf(1 - t, 3);
}
static float Clamp01(float t) {
    return fmaxf(0, fminf(1, t));
}
static const char *PlayerTag(const Game *g, int index) {
    if (g->mode == MODE_DEMO)
        return "CPU";
    if (index == 0)
        return "PLAYER 1";
    return g->mode == MODE_LOCAL ? "PLAYER 2" : g->mode == MODE_TRAINING ? "DUMMY" : "CPU";
}
static void Wrap(Font font, const char *text, Vector2 pos, float width, float size, float leading, Color color) {
    char line[256] = {0};
    char word[64];
    const char *p = text;
    float y = pos.y;
    while (*p) {
        int n = 0;
        while (*p == ' ')
            p++;
        while (*p && *p != ' ' && n < 63)
            word[n++] = *p++;
        word[n] = 0;
        if (!n)
            break;
        char trial[320];
        snprintf(trial, sizeof(trial), "%s%s%s", line, line[0] ? " " : "", word);
        if (MeasureTextEx(font, trial, size, 0.5f).x > width && line[0]) {
            TextLabel(font, line, (Vector2){pos.x, y}, size, color);
            y += leading;
            snprintf(line, sizeof(line), "%s", word);
        } else
            snprintf(line, sizeof(line), "%.250s", trial);
    }
    if (line[0])
        TextLabel(font, line, (Vector2){pos.x, y}, size, color);
}
static void Keycap(const Art *a, const char *key, float x, float y) {
    float w = fmaxf(26, MeasureTextEx(a->body, key, 14, 0.5f).x + 14);
    DrawRectangleRounded((Rectangle){x, y, w, 24}, 0.25f, 4, Fade(PAPER, 0.1f));
    DrawRectangleRoundedLinesEx((Rectangle){x, y, w, 24}, 0.25f, 4, 1, Fade(PAPER, 0.35f));
    CenterLabelAt(a->body, key, x + w / 2, y + 4, 14, PAPER);
}
static float KeyHint(const Art *a, const char *key, const char *label, float x, float y, Color color) {
    Keycap(a, key, x, y);
    float w = fmaxf(26, MeasureTextEx(a->body, key, 14, 0.5f).x + 14);
    TextLabel(a->body, label, (Vector2){x + w + 8, y + 5}, 13, color);
    return x + w + 16 + MeasureTextEx(a->body, label, 13, 0.5f).x + 18;
}

// ---------------------------------------------------------------------------------------------
// Match HUD
// ---------------------------------------------------------------------------------------------
static void HealthBar(const Art *a, const Game *g, const Fx *fx, int index) {
    const Fighter *f = &g->fighters[index];
    const CharacterInfo *c = CharacterGet(f->character);
    bool right = index == 1;
    Color accent = FighterColor(g, index);
    const float barW = 452, barH = 28, y = 42, slant = 14;
    float x = right ? 708 : 120;
    float outer = right ? x + barW : x; // outer edge (screen side)
    float fraction = fmaxf(0, f->health) / MAX_HEALTH;
    float trail = fmaxf(0, f->healthTrail) / MAX_HEALTH;
    // Portrait chip.
    Rectangle chip = {right ? 1180 : 28, 22, 72, 72};
    DrawRectangleRec((Rectangle){chip.x - 3, chip.y - 3, chip.width + 6, chip.height + 6}, accent);
    DrawRectangleRec(chip, INK);
    DrawPortrait(a, f->character, chip, right, f->stun > 0 ? (Color){255, 190, 190, 255} : WHITE);
    if (index == 1 && g->fighters[0].character == g->fighters[1].character)
        DrawRectangleRec(chip, Fade(accent, 0.18f));
    // Frame.
    Para(x - 4, y - 4, barW + 8, barH + 8, right ? -slant : slant, Fade(INK, 0.85f));
    Para(x - 2, y - 2, barW + 4, barH + 4, right ? -slant : slant, Fade(PAPER, 0.18f));
    Para(x, y, barW, barH, right ? -slant : slant, Fade(INK, 0.95f));
    // Bars drain toward the screen edge, anchored at the timer.
    float trailW = barW * trail, fillW = barW * fraction;
    float trailX = right ? x : x + barW - trailW;
    float fillX = right ? x : x + barW - fillW;
    float s = right ? -slant : slant;
    Para(trailX, y, trailW, barH, s, f->healthTrail > f->health + 1 ? DANGER : Fade(PAPER, 0.4f));
    bool low = fraction < 0.25f && f->health > 0;
    float pulse = low ? 0.5f + 0.5f * sinf(fx->time * 10) : 0;
    Color fill = low ? MixColor(accent, DANGER, pulse) : accent;
    Para(fillX, y, fillW, barH, s, Shade(fill, 0.75f));
    Para(fillX, y, fillW, barH * 0.45f, s * 0.45f, fill);
    Para(fillX, y + 2, fillW, 3, s * 0.1f, Fade(WHITE, 0.45f));
    for (int i = 1; i < 10; ++i) {
        float tx = x + i * barW / 10;
        DrawLineEx((Vector2){tx + s, y}, (Vector2){tx, y + barH}, 1, Fade(INK, 0.35f));
    }
    // Name plate and round wins.
    float nameX = right ? outer - 4 : outer + 4;
    if (right) {
        RightLabel(a->display, c->name, nameX, y + barH + 6, 34, PAPER);
        float w = MeasureTextEx(a->display, c->name, 34, 0.8f).x;
        RightLabel(a->body, c->title, nameX - w - 12, y + barH + 20, 13, Fade(accent, 0.9f));
        RightLabel(a->body, PlayerTag(g, index), nameX, y + barH + 44, 11, MUTED);
    } else {
        TextLabel(a->display, c->name, (Vector2){nameX, y + barH + 6}, 34, PAPER);
        float w = MeasureTextEx(a->display, c->name, 34, 0.8f).x;
        TextLabel(a->body, c->title, (Vector2){nameX + w + 12, y + barH + 20}, 13, Fade(accent, 0.9f));
        TextLabel(a->body, PlayerTag(g, index), (Vector2){nameX, y + barH + 44}, 11, MUTED);
    }
    if (g->mode != MODE_TRAINING)
        for (int i = 0; i < 2; ++i) {
            float px = right ? x + 18 + i * 26 : x + barW - 18 - i * 26;
            Vector2 c0 = {px, y + barH + 22};
            bool won = i < f->wins;
            Quad((Vector2){c0.x, c0.y - 9}, (Vector2){c0.x + 9, c0.y}, (Vector2){c0.x, c0.y + 9}, (Vector2){c0.x - 9, c0.y},
                 won ? SPIRIT_GOLD : Fade(PAPER, 0.15f));
            if (won)
                Glow((int)c0.x, (int)c0.y, 18, Fade(SPIRIT_GOLD, 0.35f), BLANK);
        }
}
static void SpiritMeter(const Art *a, const Game *g, const Fx *fx, int index) {
    const Fighter *f = &g->fighters[index];
    const CharacterInfo *c = CharacterGet(f->character);
    bool right = index == 1;
    Color accent = FighterColor(g, index);
    const float w = 330, h = 14, y = 672;
    float x = right ? ARENA_W - 40 - w : 40;
    float s = right ? -8 : 8;
    float fraction = f->energy / MAX_ENERGY;
    bool full = f->energy >= SUPER_COST;
    Para(x - 3, y - 3, w + 6, h + 6, s, Fade(INK, 0.85f));
    Para(x, y, w, h, s, Fade(PAPER, 0.08f));
    float fillW = w * fraction;
    float fillX = right ? x + w - fillW : x;
    Color fill = full ? MixColor(accent, WHITE, 0.35f + 0.25f * sinf(fx->time * 12)) : accent;
    Para(fillX, y, fillW, h, s, Shade(fill, full ? 1 : 0.8f));
    float specialX = right ? x + w - w * SPECIAL_COST / 100 : x + w * SPECIAL_COST / 100;
    DrawLineEx((Vector2){specialX + s, y - 4}, (Vector2){specialX, y + h + 4}, 2, Fade(PAPER, 0.6f));
    for (int i = 1; i < 4; ++i) {
        float tx = x + w * i / 4;
        DrawLineEx((Vector2){tx + s, y}, (Vector2){tx, y + h}, 1, Fade(INK, 0.5f));
    }
    if (full) {
        BeginBlendMode(BLEND_ADDITIVE);
        Para(x, y - 2, w, h + 4, s, Fade(accent, 0.25f + 0.15f * sinf(fx->time * 12)));
        EndBlendMode();
    }
    const char *label = full ? TextFormat("SPIRIT ART READY  /  %s", c->superName)
                             : TextFormat("SPIRIT  %03i", (int)f->energy);
    Color labelColor = full ? MixColor(accent, WHITE, 0.4f) : Fade(PAPER, 0.75f);
    if (right)
        RightLabel(a->body, label, x + w, y - 22, 13, labelColor);
    else
        TextLabel(a->body, label, (Vector2){x, y - 22}, 13, labelColor);
    const char *special = TextFormat("%s", c->specialName);
    Color specialColor = f->energy >= SPECIAL_COST ? Fade(accent, 0.9f) : Fade(MUTED, 0.4f);
    if (right)
        TextLabel(a->body, special, (Vector2){x, y - 22}, 11, specialColor);
    else
        RightLabel(a->body, special, x + w, y - 22, 11, specialColor);
}
static void ComboCounter(const Art *a, const Game *g, const Fx *fx, int index) {
    if (fx->comboTimer[index] <= 0 || fx->comboShown[index] < 2)
        return;
    bool right = index == 1;
    Color accent = FighterColor(g, index);
    float alpha = Clamp01(fx->comboTimer[index] / 0.4f);
    float pop = 1 + fx->comboPop[index] * 0.45f;
    float x = right ? ARENA_W - 60 : 60;
    float y = 150;
    const char *num = TextFormat("%i", fx->comboShown[index]);
    float size = 96 * pop;
    float w = MeasureTextEx(a->display, num, size, 0.8f).x;
    float nx = right ? x - w : x;
    TextLabel(a->display, num, (Vector2){nx + 4, y + 4 - (size - 96) / 2}, size, Fade(INK, 0.6f * alpha));
    TextLabel(a->display, num, (Vector2){nx, y - (size - 96) / 2}, size, Fade(MixColor(accent, WHITE, fx->comboPop[index]), alpha));
    const char *hits = "HITS";
    const char *damage = TextFormat("%i DAMAGE", (int)fx->comboDamage[index]);
    if (right) {
        RightLabel(a->display, hits, x - w - 10, y + 50, 34, Fade(PAPER, alpha));
        RightLabel(a->body, damage, x, y + 100, 15, Fade(SPIRIT_GOLD, alpha));
    } else {
        TextLabel(a->display, hits, (Vector2){x + w + 10, y + 50}, 34, Fade(PAPER, alpha));
        TextLabel(a->body, damage, (Vector2){x, y + 100}, 15, Fade(SPIRIT_GOLD, alpha));
    }
}
static void Timer(const Art *a, const Game *g, const Fx *fx) {
    Vector2 c = {ARENA_W / 2, 64};
    Color frame = Fade(INK, 0.9f);
    Quad((Vector2){c.x - 50, c.y - 44}, (Vector2){c.x + 50, c.y - 44}, (Vector2){c.x + 40, c.y + 40},
         (Vector2){c.x - 40, c.y + 40}, frame);
    DrawLineEx((Vector2){c.x - 50, c.y - 44}, (Vector2){c.x + 50, c.y - 44}, 2, Fade(SPIRIT_GOLD, 0.6f));
    DrawLineEx((Vector2){c.x - 40, c.y + 40}, (Vector2){c.x + 40, c.y + 40}, 2, Fade(SPIRIT_GOLD, 0.6f));
    bool training = g->mode == MODE_TRAINING;
    bool urgent = !training && g->roundTime < 10;
    float scale = urgent ? 1 + 0.08f * fmaxf(0, sinf(fx->time * 12)) : 1;
    const char *label = training ? "--" : TextFormat("%02i", (int)ceilf(g->roundTime));
    float size = 60 * scale;
    CenterLabel(a->display, label, c.y - 36 - (size - 60) / 2, size, urgent ? DANGER : PAPER);
    bool final = g->fighters[0].wins == 1 && g->fighters[1].wins == 1;
    CenterLabel(a->body, training ? "TRAINING" : final ? "FINAL ROUND" : TextFormat("ROUND %i", g->round), c.y + 46,
                12, final ? DANGER : SPIRIT_GOLD);
}
static void HistoryIcon(const Art *a, unsigned bits, float x, float y) {
    const char *labels[] = {"<", ">", "^", "G", "J", "K", "L", "D", "I"};
    const Color colors[] = {PAPER, PAPER, PAPER, CYAN, (Color){255, 230, 140, 255}, (Color){255, 170, 110, 255},
                            (Color){210, 150, 255, 255}, MUTED, (Color){255, 120, 160, 255}};
    float cx = x;
    for (int k = 0; k < 9; ++k)
        if (bits & (1u << k)) {
            DrawCircleV((Vector2){cx + 9, y + 9}, 10, Fade(colors[k], 0.2f));
            CenterLabelAt(a->body, labels[k], cx + 9, y + 1, 15, colors[k]);
            cx += 22;
        }
    if (!bits)
        DrawCircleV((Vector2){cx + 9, y + 9}, 3, Fade(MUTED, 0.5f));
}
static void TrainingPanel(const Art *a, const Game *g, const InputHistory *history, bool showHistory) {
    const char *dummy[] = {"STAND", "GUARD", "JUMP", "CPU"};
    const Fighter *p = &g->fighters[0];
    Rectangle panel = {1022, 330, 230, 150};
    DrawRectangleRounded(panel, 0.08f, 6, Fade(INK, 0.72f));
    DrawRectangleRoundedLinesEx(panel, 0.08f, 6, 1, Fade(CYAN, 0.35f));
    TextLabel(a->display, "TRAINING ROOM", (Vector2){1036, 338}, 22, CYAN);
    TextLabel(a->body, TextFormat("LAST COMBO   %i DMG", (int)fmaxf(p->comboDamage, p->lastComboDamage)), (Vector2){1036, 368},
              14, PAPER);
    TextLabel(a->body, TextFormat("BEST COMBO   %i HITS", p->maxCombo), (Vector2){1036, 388}, 14, PAPER);
    KeyHint(a, "T", TextFormat("DUMMY  %s", dummy[g->dummy]), 1036, 412, SPIRIT_GOLD);
    float x = KeyHint(a, "R", "RESET", 1036, 444, MUTED);
    KeyHint(a, "H", "HITBOXES", x, 444, MUTED);
    if (!showHistory || !history)
        return;
    for (int i = 0; i < history->count && i < 11; ++i) {
        float y = 336 + i * 26;
        DrawRectangle(28, (int)y - 2, 150, 24, Fade(INK, 0.45f - i * 0.03f));
        TextLabel(a->body, TextFormat("%3i", (int)fminf(99, history->hold[i] * 60 + 1)), (Vector2){34, y + 2}, 12,
                  Fade(MUTED, 0.7f));
        HistoryIcon(a, history->bits[i], 66, y + 1);
    }
}

void DrawHud(const Art *a, const Game *g, const Fx *fx, const InputHistory *history, bool showHistory) {
    DrawRectangleGradientV(0, 0, (int)ARENA_W, 150, Fade(INK, 0.55f), BLANK);
    DrawRectangleGradientV(0, 610, (int)ARENA_W, 110, BLANK, Fade(INK, 0.6f));
    HealthBar(a, g, fx, 0);
    HealthBar(a, g, fx, 1);
    Timer(a, g, fx);
    SpiritMeter(a, g, fx, 0);
    SpiritMeter(a, g, fx, 1);
    ComboCounter(a, g, fx, 0);
    ComboCounter(a, g, fx, 1);
    if (g->mode == MODE_TRAINING)
        TrainingPanel(a, g, history, showHistory);
    CenterLabel(a->body, StageName(g->stage), 696, 10, Fade(MUTED, 0.55f));
}

void DrawPopups(const Art *a, const Fx *fx, bool reducedMotion) {
    Camera2D cam = FxCamera(fx, 1, reducedMotion);
    for (int i = 0; i < FX_POPUPS; ++i) {
        const FxPopup *p = &fx->popups[i];
        if (p->life <= 0)
            continue;
        float t = 1 - p->life / p->maxLife;
        float pop = t < 0.08f ? 1.7f - t / 0.08f * 0.7f : 1;
        float alpha = t > 0.7f ? (1 - t) / 0.3f : 1;
        Vector2 s = GetWorldToScreen2D(p->pos, cam);
        float size = p->size * pop;
        float w = MeasureTextEx(a->display, p->text, size, 0.8f).x;
        float x = fmaxf(20, fminf(ARENA_W - w - 20, s.x - w / 2));
        Para(x - 14, s.y + 4, w + 28, size - 4, 10, Fade(INK, 0.7f * alpha));
        TextLabel(a->display, p->text, (Vector2){x, s.y}, size, Fade(p->color, alpha));
    }
}

// ---------------------------------------------------------------------------------------------
// Announcer and Spirit Art cut-in
// ---------------------------------------------------------------------------------------------
static void Banner(const Art *a, const char *text, float y, float size, Color color, float t, float out) {
    float in = Ease(t / 0.18f);
    float leave = out > 0 ? Ease(out / 0.2f) : 0;
    float bandX = -ARENA_W * (1 - in) + ARENA_W * leave;
    Para(bandX - 60, y - 16, ARENA_W + 120, size + 26, 40, Fade(INK, 0.8f));
    Para(bandX - 60, y - 16, ARENA_W + 120, 3, 40, Fade(color, 0.8f));
    Para(bandX - 60, y + size + 7, ARENA_W + 120, 3, 40, Fade(color, 0.8f));
    float scale = 1 + (1 - Ease(t / 0.22f)) * 0.6f;
    float s = size * scale;
    float w = MeasureTextEx(a->display, text, s, 0.8f).x;
    TextLabel(a->display, text, (Vector2){ARENA_W / 2 - w / 2 + ARENA_W * leave * 0.5f, y - (s - size) / 2}, s,
              Fade(color, in * (1 - leave)));
}
static void SuperCutIn(const Art *a, const Game *g, const Fx *fx) {
    if (fx->superTime <= 0)
        return;
    int owner = fx->superOwner;
    const CharacterInfo *c = CharacterGet(g->fighters[owner].character);
    Color accent = FighterColor(g, owner);
    float t = SUPER_FREEZE - fx->superTime;
    float in = Ease(t / 0.14f);
    float out = fx->superTime < 0.12f ? Ease(1 - fx->superTime / 0.12f) : 0;
    bool right = owner == 1;
    float dir = right ? -1.0f : 1.0f;
    float offset = (1 - in) * -ARENA_W * dir + out * ARENA_W * dir;
    float y = 228;
    Para(offset - 80, y, ARENA_W + 160, 170, 60, Fade(Shade(accent, 0.35f), 0.92f));
    for (int i = 0; i < 14; ++i) {
        float sx = fmodf(i * 120 + fx->time * 900 * dir, ARENA_W + 240) - 120 + offset;
        Para(sx, y, 30, 170, 60, Fade(accent, 0.12f));
    }
    Para(offset - 80, y - 4, ARENA_W + 160, 4, 60, accent);
    Para(offset - 80, y + 170, ARENA_W + 160, 4, 60, accent);
    // Eyes-and-face crop of the fighter, oversized for drama.
    Rectangle face = {right ? 820 + offset : 120 + offset, y + 8, 300, 154};
    BeginScissorMode((int)(face.x * RenderDensity()), (int)(face.y * RenderDensity()), (int)(face.width * RenderDensity()),
                     (int)(face.height * RenderDensity()));
    DrawPortrait(a, g->fighters[owner].character, (Rectangle){face.x - 20, face.y - 110, 340, 340}, right, WHITE);
    EndScissorMode();
    DrawRectangleGradientH((int)face.x, (int)face.y, 60, (int)face.height, Shade(accent, 0.35f), BLANK);
    DrawRectangleGradientH((int)(face.x + face.width - 60), (int)face.y, 60, (int)face.height, BLANK, Shade(accent, 0.35f));
    float textX = right ? 140 + offset : 560 + offset;
    TextLabel(a->body, "SPIRIT ART", (Vector2){textX, y + 26}, 18, Fade(PAPER, 0.85f));
    TextLabel(a->display, c->superName, (Vector2){textX - 2, y + 48}, 62, WHITE);
    TextLabel(a->display, c->name, (Vector2){textX, y + 116}, 30, accent);
}

void DrawAnnouncer(const Art *a, const Game *g, const Fx *fx) {
    SuperCutIn(a, g, fx);
    if (g->phase == ROUND_INTRO) {
        float t = g->phaseTime;
        bool final = g->fighters[0].wins == 1 && g->fighters[1].wins == 1;
        if (t < 1.45f) {
            const char *label = final ? "FINAL ROUND" : TextFormat("ROUND %i", g->round);
            Banner(a, label, 262, 92, final ? DANGER : PAPER, t, t > 1.25f ? t - 1.25f : 0);
            if (t > 0.25f && t < 1.25f)
                CenterLabel(a->body, StageTagline(g->stage), 392, 15, Fade(PAPER, 0.75f * Clamp01((t - 0.25f) / 0.2f)));
        } else {
            float k = t - 1.45f;
            float scale = 1 + (1 - Ease(k / 0.16f)) * 1.4f;
            float size = 150 * scale;
            float w = MeasureTextEx(a->display, "CLASH!", size, 0.8f).x;
            TextLabel(a->display, "CLASH!", (Vector2){ARENA_W / 2 - w / 2 + 5, 240 - (size - 150) / 2 + 5}, size,
                      Fade(INK, 0.6f * Clamp01(k / 0.16f)));
            TextLabel(a->display, "CLASH!", (Vector2){ARENA_W / 2 - w / 2, 240 - (size - 150) / 2}, size,
                      Fade(SPIRIT_GOLD, Clamp01(k / 0.1f)));
        }
    } else if (g->phase == ROUND_FIGHT && fx->fightTime > 0) {
        float k = 1 - fx->fightTime / 0.7f;
        float size = 150 * (1 + k * 0.5f);
        float w = MeasureTextEx(a->display, "CLASH!", size, 0.8f).x;
        TextLabel(a->display, "CLASH!", (Vector2){ARENA_W / 2 - w / 2, 240 - (size - 150) / 2}, size,
                  Fade(SPIRIT_GOLD, 1 - k));
    } else if (g->phase == ROUND_END) {
        float t = g->phaseTime;
        const char *word = g->timeUp ? "TIME" : g->winner < 0 ? "DOUBLE K.O." : "K.O.";
        float scale = 1 + (1 - Ease(t / 0.14f)) * 2.2f;
        float size = 170 * scale;
        float jitter = t < 0.4f ? sinf(t * 120) * (0.4f - t) * 20 : 0;
        float w = MeasureTextEx(a->display, word, size, 0.8f).x;
        float alpha = Clamp01(t / 0.08f) * (t > 2.9f ? Clamp01((3.2f - t) / 0.3f) : 1);
        float y = 196 - (size - 170) / 2;
        TextLabel(a->display, word, (Vector2){ARENA_W / 2 - w / 2 + 6 + jitter, y + 6}, size, Fade(INK, 0.7f * alpha));
        TextLabel(a->display, word, (Vector2){ARENA_W / 2 - w / 2 + jitter, y}, size, Fade(DANGER, alpha));
        TextLabel(a->display, word, (Vector2){ARENA_W / 2 - w / 2 + jitter, y}, size,
                  Fade(SPIRIT_GOLD, alpha * Clamp01(1 - t * 3)));
        if (t > 1.1f) {
            float k = t - 1.1f;
            const char *who = g->winner < 0 ? "NO ONE YIELDS"
                                            : TextFormat("%s WINS", CharacterGet(g->fighters[g->winner].character)->name);
            Color color = g->winner < 0 ? PAPER : FighterColor(g, g->winner);
            Banner(a, who, 408, 54, color, k, t > 2.9f ? t - 2.9f : 0);
            if (g->perfect && k > 0.25f)
                CenterLabel(a->display, "PERFECT", 488, 46, Fade(SPIRIT_GOLD, Clamp01((k - 0.25f) / 0.15f)));
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Title
// ---------------------------------------------------------------------------------------------
Rectangle TitleItemRect(int item) {
    return (Rectangle){70, 360 + item * 58.0f, 440, 50};
}
void DrawTitle(const Art *a, float time, int selected, float cursor, int difficulty, bool muted) {
    StageDrawBack(a->backdrop, STAGE_SANCTUARY, time, ARENA_W / 2 + sinf(time * 0.2f) * 40, 0, false);
    DrawRectangle(0, 0, (int)ARENA_W, (int)ARENA_H, Fade(INK, 0.35f));
    // Rivals face off on the right.
    float breathe = sinf(time * 2) * 3;
    BeginBlendMode(BLEND_ADDITIVE);
    Glow(820, 470, 300, Fade(CYAN, 0.18f), BLANK);
    Glow(1120, 470, 300, Fade(ROSE, 0.18f), BLANK);
    EndBlendMode();
    DrawCharacterShowcase(a, CHAR_REN, (Vector2){840, 690 + breathe}, 2.05f, 1, 0, WHITE);
    DrawCharacterShowcase(a, CHAR_KAE, (Vector2){1100, 690 - breathe}, 2.05f, -1, 0, WHITE);
    // A spirit slash between them.
    float slash = fmodf(time, 4.0f);
    if (slash < 0.5f) {
        BeginBlendMode(BLEND_ADDITIVE);
        float k = slash / 0.5f;
        DrawLineEx((Vector2){960 - 300 * (1 - k), 160 + 420 * (1 - k)}, (Vector2){960 + 200 * k, 160 + 420 * (1 - k) - 300 * k}, 6 * (1 - k),
                   Fade(WHITE, 1 - k));
        EndBlendMode();
    }
    DrawRectangleGradientH(0, 0, 760, (int)ARENA_H, Fade(INK, 0.97f), BLANK);
    DrawRectangleGradientV(0, 600, (int)ARENA_W, 120, BLANK, Fade(INK, 0.9f));
    // Logo with a travelling sheen.
    TextLabel(a->display, "RAYLIB FIGHTER", (Vector2){76, 40}, 18, CYAN);
    TextLabel(a->display, "SPIRIT", (Vector2){68, 60}, 132, PAPER);
    TextLabel(a->display, "CLASH", (Vector2){68, 176}, 132, CYAN);
    float sheen = fmodf(time * 0.45f, 1.8f) - 0.2f;
    if (sheen > 0 && sheen < 1) {
        float d = RenderDensity();
        BeginScissorMode((int)((60 + sheen * 300) * d), (int)(60 * d), (int)(36 * d), (int)(270 * d));
        BeginBlendMode(BLEND_ADDITIVE);
        TextLabel(a->display, "SPIRIT", (Vector2){68, 60}, 132, Fade(WHITE, 0.55f));
        TextLabel(a->display, "CLASH", (Vector2){68, 176}, 132, Fade(WHITE, 0.55f));
        EndBlendMode();
        EndScissorMode();
    }
    TextLabel(a->body, "Two rivals. Two arenas. One fighting spirit.", (Vector2){76, 316}, 18, MUTED);
    const char *labels[] = {"SOLO DUEL", "LOCAL VERSUS", "TRAINING", "MOVE LIST", "QUIT"};
    const char *details[] = {"FACE THE CPU, BEST OF THREE", "TWO PLAYERS, ONE KEYBOARD OR TWO PADS",
                             "LAB YOUR COMBOS ON A DUMMY", "EVERY STRIKE, SPECIAL AND SPIRIT ART", "LEAVE THE SANCTUARY"};
    Rectangle bar = TitleItemRect(0);
    bar.y = 360 + cursor * 58;
    Para(bar.x, bar.y, bar.width, bar.height, 12, CYAN);
    Para(bar.x + bar.width + 6, bar.y, 10, bar.height, 12, Fade(CYAN, 0.6f));
    for (int i = 0; i < TITLE_COUNT; ++i) {
        Rectangle r = TitleItemRect(i);
        bool active = i == selected;
        TextLabel(a->display, labels[i], (Vector2){r.x + 22 + (active ? 8 : 0), r.y + 5}, 32, active ? INK : PAPER);
        TextLabel(a->body, details[i], (Vector2){r.x + 24 + (active ? 8 : 0), r.y + 34}, 11,
                  active ? Fade(INK, 0.75f) : Fade(MUTED, 0.7f));
    }
    const char *levels[] = {"EASY", "NORMAL", "HARD"};
    const Color levelColors[] = {(Color){140, 230, 160, 255}, SPIRIT_GOLD, DANGER};
    TextLabel(a->body, "CPU LEVEL", (Vector2){78, 662}, 13, MUTED);
    TextLabel(a->display, TextFormat("<  %s  >", levels[difficulty]), (Vector2){160, 654}, 28, levelColors[difficulty]);
    float x = 760;
    x = KeyHint(a, "ENTER", "SELECT", x, 662, MUTED);
    x = KeyHint(a, "W S", "MOVE", x, 662, MUTED);
    x = KeyHint(a, "M", muted ? "SOUND OFF" : "SOUND ON", x, 662, MUTED);
    TextLabel(a->body, IsGamepadAvailable(0) ? "GAMEPAD READY" : "GAMEPADS SUPPORTED", (Vector2){x, 667}, 12,
              IsGamepadAvailable(0) ? CYAN : Fade(MUTED, 0.6f));
}

// ---------------------------------------------------------------------------------------------
// Character and stage select
// ---------------------------------------------------------------------------------------------
static void StatRow(const Art *a, const char *label, int value, float x, float y, Color accent) {
    TextLabel(a->body, label, (Vector2){x, y}, 13, MUTED);
    for (int i = 0; i < 5; ++i)
        Para(x + 96 + i * 30, y + 2, 24, 12, 5, i < value ? accent : Fade(PAPER, 0.12f));
}
static void FighterPanel(const Art *a, const SelectState *s, int side, float time) {
    int character = s->cursor[side];
    const CharacterInfo *c = CharacterGet(character);
    bool mirror = side == 1 && s->cursor[0] == s->cursor[1];
    Color accent = mirror ? (character == CHAR_REN ? (Color){255, 200, 110, 255} : (Color){190, 160, 255, 255}) : c->accent;
    bool right = side == 1;
    bool locked = s->locked[side];
    float pop = locked ? Ease((time - s->lockTime[side]) / 0.25f) : 0;
    // Full-body art.
    float fx = right ? 905 : 375;
    BeginBlendMode(BLEND_ADDITIVE);
    Glow((int)fx, 470, 300, Fade(accent, locked ? 0.3f : 0.16f), BLANK);
    EndBlendMode();
    Color tint = mirror ? (character == CHAR_REN ? (Color){255, 214, 170, 255} : (Color){184, 196, 255, 255}) : WHITE;
    DrawCharacterShowcase(a, character, (Vector2){fx, 700 + sinf(time * 2 + side) * 3}, 2.25f + pop * 0.08f, right ? -1 : 1,
                          locked ? 1 : 0, tint);
    if (locked && time - s->lockTime[side] < 0.3f) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCharacterShowcase(a, character, (Vector2){fx, 700}, 2.25f + pop * 0.08f, right ? -1 : 1, 1,
                              Fade(WHITE, 0.8f * (1 - (time - s->lockTime[side]) / 0.3f)));
        EndBlendMode();
    }
    // Text panel on the outer edge.
    float x = right ? ARENA_W - 330 : 40;
    TextLabel(a->body, side == 0 ? "PLAYER 1" : s->mode == MODE_LOCAL ? "PLAYER 2" : s->mode == MODE_TRAINING ? "DUMMY" : "CPU",
              (Vector2){x, 96}, 13, accent);
    TextLabel(a->display, c->name, (Vector2){x - 3, 112}, 78, PAPER);
    TextLabel(a->body, c->title, (Vector2){x, 192}, 16, accent);
    float chipW = MeasureTextEx(a->body, c->style, 12, 0.5f).x + 18;
    Para(x, 220, chipW, 20, 6, Fade(accent, 0.25f));
    TextLabel(a->body, c->style, (Vector2){x + 10, 223}, 12, PAPER);
    StatRow(a, "POWER", c->power, x, 256, accent);
    StatRow(a, "SPEED", c->speed, x, 278, accent);
    StatRow(a, "REACH", c->reach, x, 300, accent);
    StatRow(a, "TECHNIQUE", c->technique, x, 322, accent);
    TextLabel(a->body, "SPECIAL", (Vector2){x, 356}, 11, MUTED);
    TextLabel(a->display, c->specialName, (Vector2){x, 370}, 24, PAPER);
    TextLabel(a->body, "SPIRIT ART", (Vector2){x, 402}, 11, MUTED);
    TextLabel(a->display, c->superName, (Vector2){x, 416}, 24, accent);
    DrawRectangle((int)x - 8, 452, 296, 70, Fade(INK, 0.55f));
    Wrap(a->body, c->blurb, (Vector2){x, 458}, 280, 14, 19, Fade(PAPER, 0.8f));
    if (locked)
        TextLabel(a->display, "READY", (Vector2){x, 530}, 38, Fade(SPIRIT_GOLD, pop));
}
void DrawSelect(const Art *a, const SelectState *s, float time, int difficulty) {
    Color left = CharacterGet(s->cursor[0])->deep, right = CharacterGet(s->cursor[1])->deep;
    DrawRectangleGradientH(0, 0, (int)ARENA_W, (int)ARENA_H, Shade(left, 0.6f), Shade(right, 0.6f));
    for (int i = 0; i < 18; ++i) {
        float x = fmodf(i * 90 + time * 40, ARENA_W + 300) - 150;
        Para(x, 0, 26, ARENA_H, 260, Fade(PAPER, 0.025f));
    }
    DrawRectangleGradientV(0, 0, (int)ARENA_W, 200, Fade(INK, 0.8f), BLANK);
    DrawRectangleGradientV(0, 520, (int)ARENA_W, 200, BLANK, Fade(INK, 0.95f));
    FighterPanel(a, s, 0, time);
    FighterPanel(a, s, 1, time);
    const char *heading = s->step == 1      ? "CHOOSE THE ARENA"
                          : s->cpuSide      ? (s->mode == MODE_TRAINING ? "CHOOSE YOUR SPARRING PARTNER" : "CHOOSE YOUR OPPONENT")
                          : s->mode == MODE_LOCAL ? "CHOOSE YOUR FIGHTERS"
                                                  : "CHOOSE YOUR FIGHTER";
    CenterLabel(a->display, heading, 26, 44, PAPER);
    const char *modes[] = {"SOLO DUEL", "LOCAL VERSUS", "TRAINING", "EXHIBITION"};
    const char *levels[] = {"EASY", "NORMAL", "HARD"};
    CenterLabel(a->body,
                s->mode == MODE_SOLO ? TextFormat("%s  /  CPU %s", modes[s->mode], levels[difficulty]) : modes[s->mode], 78,
                13, SPIRIT_GOLD);
    // Roster tiles.
    for (int i = 0; i < CHAR_COUNT; ++i) {
        Rectangle tile = {566, 120 + i * 168.0f, 148, 148};
        DrawRectangleRec((Rectangle){tile.x - 3, tile.y - 3, tile.width + 6, tile.height + 6}, Fade(INK, 0.9f));
        DrawPortrait(a, i, tile, false, WHITE);
        DrawRectangleGradientV((int)tile.x, (int)(tile.y + 100), (int)tile.width, 48, BLANK, Fade(INK, 0.9f));
        TextLabel(a->display, CharacterGet(i)->name, (Vector2){tile.x + 8, tile.y + 112}, 30, PAPER);
        for (int side = 0; side < 2; ++side) {
            if (s->cursor[side] != i)
                continue;
            bool active = s->step == 0 && (s->mode == MODE_LOCAL || (side == 1) == s->cpuSide);
            Color c = side == 0 ? CYAN : ROSE;
            float inset = side == 0 ? -6 : -12;
            float pulse = active ? 0.6f + 0.4f * sinf(time * 8) : 0.5f;
            DrawRectangleLinesEx((Rectangle){tile.x + inset, tile.y + inset, tile.width - inset * 2, tile.height - inset * 2},
                                 side == 0 ? 4 : 3, Fade(c, pulse));
            const char *tag = side == 0 ? "1P" : s->mode == MODE_LOCAL ? "2P" : "CPU";
            float tx = side == 0 ? tile.x - 10 : tile.x + tile.width - 30;
            Para(tx, tile.y - 22, 40, 20, 5, c);
            CenterLabelAt(a->body, tag, tx + 22, tile.y - 20, 13, INK);
        }
    }
    // Stage picker.
    Rectangle preview = {500, 470, 280, 158};
    DrawRectangleRec((Rectangle){preview.x - 4, preview.y - 4, preview.width + 8, preview.height + 8},
                     s->step == 1 ? SPIRIT_GOLD : Fade(PAPER, 0.25f));
    DrawStagePreview(a, (StageId)s->stage, preview, time);
    CenterLabel(a->body, "ARENA", 448, 12, MUTED);
    CenterLabel(a->display, TextFormat("%s  %s  %s", s->step == 1 ? "<" : "", StageName((StageId)s->stage), s->step == 1 ? ">" : ""),
                634, 28, s->step == 1 ? SPIRIT_GOLD : PAPER);
    float x = 360;
    x = KeyHint(a, "A D", "CHOOSE", x, 688, MUTED);
    x = KeyHint(a, "J / ENTER", "CONFIRM", x, 688, MUTED);
    x = KeyHint(a, "K / ESC", "BACK", x, 688, MUTED);
    if (s->mode == MODE_LOCAL)
        KeyHint(a, "< >  ,", "P2", x, 688, ROSE);
}

void DrawVersus(const Art *a, const Game *g, float t) {
    StageDrawBack(a->backdrop, g->stage, t, ARENA_W / 2, 0, false);
    DrawRectangle(0, 0, (int)ARENA_W, (int)ARENA_H, Fade(INK, 0.55f));
    float in = Ease(t / 0.35f);
    for (int side = 0; side < 2; ++side) {
        const CharacterInfo *c = CharacterGet(g->fighters[side].character);
        Color accent = FighterColor(g, side);
        bool right = side == 1;
        float slide = (1 - in) * 700 * (right ? 1 : -1);
        if (right)
            Quad((Vector2){700 + slide, 0}, (Vector2){ARENA_W + 200, 0}, (Vector2){ARENA_W + 200, ARENA_H},
                 (Vector2){580 + slide, ARENA_H}, Fade(Shade(accent, 0.35f), 0.85f));
        else
            Quad((Vector2){-200, 0}, (Vector2){700 + slide, 0}, (Vector2){580 + slide, ARENA_H}, (Vector2){-200, ARENA_H},
                 Fade(Shade(accent, 0.35f), 0.85f));
        Color tint = right && g->fighters[0].character == g->fighters[1].character
                         ? (c == CharacterGet(CHAR_REN) ? (Color){255, 214, 170, 255} : (Color){184, 196, 255, 255})
                         : WHITE;
        DrawCharacterShowcase(a, g->fighters[side].character, (Vector2){(right ? 960 : 320) + slide * 1.2f, 760}, 2.9f,
                              right ? -1 : 1, t > 0.9f ? 1 : 0, tint);
        float tx = right ? ARENA_W - 60 : 60;
        float k = Ease((t - 0.35f) / 0.25f);
        if (right) {
            RightLabel(a->display, c->name, tx + (1 - k) * 200, 560, 110, Fade(PAPER, k));
            RightLabel(a->body, c->title, tx + (1 - k) * 200, 672, 20, Fade(accent, k));
        } else {
            TextLabel(a->display, c->name, (Vector2){tx - (1 - k) * 200, 560}, 110, Fade(PAPER, k));
            TextLabel(a->body, c->title, (Vector2){tx - (1 - k) * 200, 672}, 20, Fade(accent, k));
        }
    }
    float k = Ease((t - 0.45f) / 0.2f);
    if (k > 0) {
        float size = 200 * (1 + (1 - k) * 1.5f);
        float w = MeasureTextEx(a->display, "VS", size, 0.8f).x;
        TextLabel(a->display, "VS", (Vector2){ARENA_W / 2 - w / 2 + 6, 220 - (size - 200) / 2 + 6}, size, Fade(INK, 0.7f * k));
        TextLabel(a->display, "VS", (Vector2){ARENA_W / 2 - w / 2, 220 - (size - 200) / 2}, size, Fade(SPIRIT_GOLD, k));
    }
    CenterLabel(a->body, TextFormat("%s  /  BEST OF THREE", StageName(g->stage)), 40, 16, Fade(PAPER, in));
    if (t < 0.6f && t > 0.45f) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawRectangle(0, 0, (int)ARENA_W, (int)ARENA_H, Fade(WHITE, (0.6f - t) / 0.15f * 0.6f));
        EndBlendMode();
    }
}

// ---------------------------------------------------------------------------------------------
// Overlays
// ---------------------------------------------------------------------------------------------
Rectangle PauseItemRect(int item) {
    return (Rectangle){120, 250 + item * 58.0f, 420, 48};
}
void DrawPause(const Art *a, const Game *g, int selected, bool muted, bool reducedMotion) {
    DrawRectangle(0, 0, (int)ARENA_W, (int)ARENA_H, Fade(INK, 0.82f));
    Para(-40, 0, 680, ARENA_H, 120, Fade(INK, 0.6f));
    TextLabel(a->body, "TAKE A BREATHER", (Vector2){124, 150}, 15, CYAN);
    TextLabel(a->display, "PAUSED", (Vector2){118, 166}, 76, PAPER);
    const char *items[] = {"RESUME", "MOVE LIST", "RESTART MATCH", "CHARACTER SELECT", "QUIT TO TITLE"};
    for (int i = 0; i < PAUSE_COUNT; ++i) {
        Rectangle r = PauseItemRect(i);
        bool active = i == selected;
        if (active)
            Para(r.x, r.y, r.width, r.height, 12, CYAN);
        TextLabel(a->display, items[i], (Vector2){r.x + 22, r.y + 8}, 32, active ? INK : MUTED);
    }
    float x = 124;
    x = KeyHint(a, "M", muted ? "SOUND OFF" : "SOUND ON", x, 560, MUTED);
    x = KeyHint(a, "V", reducedMotion ? "CALM CAMERA ON" : "CALM CAMERA OFF", x, 560, MUTED);
    KeyHint(a, "F10", "FULLSCREEN", x, 560, MUTED);
    // Quick reference on the right.
    float rx = 720, y = 150;
    TextLabel(a->display, "CONTROLS", (Vector2){rx, y}, 34, PAPER);
    const char *rows[][3] = {{"MOVE", "A D", "< >"},           {"JUMP", "W", "^"},           {"GUARD", "S", "v"},
                             {"STRIKE", "J", ","},             {"HEAVY", "K", "."},          {"SPECIAL", "L", "/"},
                             {"SPIRIT ART", "I  or  K+L", ";"}, {"DASH", "SPACE", "R-SHIFT"}, {"BACKDASH", "back+SPACE", "back+R-SHIFT"}};
    TextLabel(a->body, "P1", (Vector2){rx + 160, y + 50}, 13, CYAN);
    TextLabel(a->body, g->mode == MODE_LOCAL ? "P2" : "", (Vector2){rx + 330, y + 50}, 13, ROSE);
    for (int i = 0; i < 9; ++i) {
        float ry = y + 76 + i * 34;
        TextLabel(a->body, rows[i][0], (Vector2){rx, ry + 4}, 14, MUTED);
        Keycap(a, rows[i][1], rx + 160, ry);
        if (g->mode == MODE_LOCAL)
            Keycap(a, rows[i][2], rx + 330, ry);
    }
    TextLabel(a->body, "GAMEPAD   X strike  Y heavy  B special  A jump  RB dash  LB guard  RT spirit art", (Vector2){rx, y + 396},
              12, Fade(MUTED, 0.8f));
}

void DrawMoveList(const Art *a, float time) {
    DrawRectangle(0, 0, (int)ARENA_W, (int)ARENA_H, Fade(INK, 0.97f));
    CenterLabel(a->display, "MOVE LIST", 24, 56, PAPER);
    CenterLabel(a->body, "Keyboard shown for player one. Every move works the same on a gamepad.", 86, 14, MUTED);
    const char *common[][2] = {
        {"J", "STRIKE  /  fast jab, chains into itself up to three times"},
        {"K", "HEAVY KICK  /  cancels from strikes; ending a chain with it launches"},
        {"J or K in air", "SKY JAB  /  DIVE KICK that bounces off a hit"},
        {"S", "GUARD  /  each blocked hit costs 7 spirit; empty spirit means GUARD BREAK"},
        {"SPACE", "DASH  /  hold back for an invulnerable backdash"},
        {"L", "SPECIAL  /  35 spirit"},
        {"I  or  K+L", "SPIRIT ART  /  100 spirit, freezes time"},
    };
    for (int i = 0; i < 7; ++i) {
        float y = 130 + i * 34;
        Keycap(a, common[i][0], 70, y);
        TextLabel(a->body, common[i][1], (Vector2){230, y + 4}, 15, PAPER);
    }
    for (int c = 0; c < CHAR_COUNT; ++c) {
        const CharacterInfo *info = CharacterGet(c);
        float x = c == 0 ? 70 : 660, y = 390;
        DrawRectangleRounded((Rectangle){x - 16, y - 12, 560, 200}, 0.06f, 6, Fade(info->deep, 0.4f));
        DrawPortrait(a, c, (Rectangle){x, y, 64, 64}, false, WHITE);
        TextLabel(a->display, info->name, (Vector2){x + 80, y - 4}, 40, info->accent);
        TextLabel(a->body, TextFormat("%s  /  %s", info->title, info->style), (Vector2){x + 80, y + 40}, 13, MUTED);
        TextLabel(a->display, info->specialName, (Vector2){x, y + 78}, 24, PAPER);
        TextLabel(a->display, info->superName, (Vector2){x, y + 128}, 24, info->accent);
        const char *special = c == CHAR_REN ? "A spinning wind orb that travels the arena. Clashes with other shots."
                                            : "Invulnerable rising uppercut. Beats jump-ins and launches for juggles.";
        const char *super = c == CHAR_REN ? "A screen-long lightning lance. Nine hits; jump it or eat it."
                                          : "Invulnerable rush into a nine-hit flurry that ends in a launch.";
        TextLabel(a->body, special, (Vector2){x, y + 104}, 13, Fade(PAPER, 0.75f));
        TextLabel(a->body, super, (Vector2){x, y + 154}, 13, Fade(PAPER, 0.75f));
    }
    CenterLabel(a->body, "COMBO ROUTE   J > J > J > K > L > I     /     Counter-hits deal +20% and heavy counters launch     /     Launched foes bounce off walls once",
                612, 13, SPIRIT_GOLD);
    CenterLabel(a->body, "ESC  /  K  /  B   BACK", 668, 13, Fade(MUTED, 0.6f + 0.4f * sinf(time * 4)));
}

Rectangle ResultItemRect(int item) {
    return (Rectangle){760, 520 + item * 52.0f, 400, 44};
}
void DrawResults(const Art *a, const Game *g, int selected, float t) {
    DrawRectangle(0, 0, (int)ARENA_W, (int)ARENA_H, Fade(INK, 0.86f * Ease(t / 0.4f)));
    if (t < 0.2f)
        return;
    int w = g->winner < 0 ? 0 : g->winner;
    const CharacterInfo *c = CharacterGet(g->fighters[w].character);
    Color accent = FighterColor(g, w);
    float in = Ease((t - 0.2f) / 0.4f);
    BeginBlendMode(BLEND_ADDITIVE);
    Glow(360, 470, 330, Fade(accent, 0.25f * in), BLANK);
    EndBlendMode();
    DrawCharacterShowcase(a, g->fighters[w].character, (Vector2){340 - (1 - in) * 200, 730}, 2.6f, 1, 1, Fade(WHITE, in));
    TextLabel(a->body, "THE SANCTUARY HAS A CHAMPION", (Vector2){650, 70}, 15, SPIRIT_GOLD);
    TextLabel(a->display, TextFormat("%s WINS", c->name), (Vector2){644 + (1 - in) * 100, 86}, 104, Fade(accent, in));
    TextLabel(a->body, TextFormat("%s  /  %s", PlayerTag(g, w), c->title), (Vector2){650, 194}, 15, PAPER);
    // Stats table.
    float y = 240;
    TextLabel(a->body, CharacterGet(g->fighters[0].character)->name, (Vector2){960, y}, 15, FighterColor(g, 0));
    TextLabel(a->body, CharacterGet(g->fighters[1].character)->name, (Vector2){1090, y}, 15, FighterColor(g, 1));
    const char *labels[] = {"ROUNDS WON", "BEST COMBO", "DAMAGE DEALT", "HITS LANDED", "SPECIALS LANDED", "SPIRIT ARTS"};
    for (int i = 0; i < 6; ++i) {
        float ry = y + 30 + i * 36;
        DrawRectangle(650, (int)ry - 4, 520, 30, Fade(PAPER, i % 2 ? 0.03f : 0.06f));
        TextLabel(a->body, labels[i], (Vector2){664, ry + 2}, 14, MUTED);
        for (int p = 0; p < 2; ++p) {
            const Fighter *f = &g->fighters[p];
            int values[] = {f->wins, f->maxCombo, (int)f->damageDealt, f->hitsLanded, f->specialsLanded, f->supersLanded};
            TextLabel(a->display, TextFormat("%i", values[i]), (Vector2){960 + p * 130, ry - 4}, 26, PAPER);
        }
    }
    const char *items[] = {"REMATCH", "CHARACTER SELECT", "BACK TO TITLE"};
    for (int i = 0; i < RESULT_COUNT; ++i) {
        Rectangle r = ResultItemRect(i);
        bool active = i == selected;
        if (active)
            Para(r.x, r.y, r.width, r.height, 12, accent);
        TextLabel(a->display, items[i], (Vector2){r.x + 22, r.y + 6}, 30, active ? INK : MUTED);
    }
}
