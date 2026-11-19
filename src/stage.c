#include "stage.h"
#include "render.h"
#include <math.h>

static Color Lerp(Color a, Color b, float t) {
    t = fmaxf(0, fminf(1, t));
    return (Color){(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                   (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}
static float Hash(float n) {
    float s = sinf(n * 127.1f) * 43758.5453f;
    return s - floorf(s);
}
static void Strip(float (*height)(float), float left, float right, float bottom, float shift, Color color) {
    for (float x = left; x < right; x += 16) {
        float y0 = height(x + shift), y1 = height(x + 16 + shift);
        Vector2 a = {x, y0}, b = {x, bottom}, c = {x + 16.5f, bottom}, d = {x + 16.5f, y1};
        DrawTriangle(a, b, c, color);
        DrawTriangle(a, c, d, color);
    }
}
static float FarRidge(float x) {
    return 360 - (sinf(x * 0.0042f + 1.1f) * 55 + sinf(x * 0.0113f) * 26 + sinf(x * 0.031f + 2) * 9);
}
static float NearRidge(float x) {
    return 432 - (sinf(x * 0.0061f + 3.2f) * 42 + sinf(x * 0.017f + 0.4f) * 16 + sinf(x * 0.045f) * 6);
}

bool StageHasReflections(StageId stage) {
    return stage == STAGE_STORM_TERRACE;
}

// ---------------------------------------------------------------------------------------------
// Moonlit Sanctuary: the painted arena, now with parallax, glowing lanterns and drifting petals.
// ---------------------------------------------------------------------------------------------
static void SanctuaryBack(Texture2D backdrop, float time, float camX, bool reducedMotion) {
    const float scale = 1.06f;
    float w = ARENA_W * scale, h = ARENA_H * scale;
    float shift = (camX - ARENA_W / 2) * 0.22f;
    if (backdrop.id)
        DrawTexturePro(backdrop, (Rectangle){0, 0, (float)backdrop.width, (float)backdrop.height},
                       (Rectangle){ARENA_W / 2 - w / 2 + shift, GROUND_Y - GROUND_Y * scale, w, h}, (Vector2){0}, 0,
                       WHITE);
    else
        DrawRectangleGradientV(-40, -40, (int)ARENA_W + 80, (int)ARENA_H + 80, (Color){27, 30, 72, 255},
                               (Color){147, 103, 142, 255});
    float t = reducedMotion ? 0 : time;
    BeginBlendMode(BLEND_ADDITIVE);
    float moon = 0.16f + sinf(t * 0.8f) * 0.04f;
    Glow((int)(920 * scale - 38 + shift), 118, 190, Fade((Color){255, 214, 160, 255}, moon), BLANK);
    const Vector2 lanterns[] = {{76, 493}, {1219, 472}, {217, 429}};
    for (int i = 0; i < 3; ++i) {
        float flicker = 0.32f + sinf(t * 9 + i * 2.1f) * 0.05f + sinf(t * 23 + i) * 0.04f;
        Vector2 p = {ARENA_W / 2 + (lanterns[i].x - ARENA_W / 2) * scale + shift,
                     GROUND_Y + (lanterns[i].y - GROUND_Y) * scale};
        Glow((int)p.x, (int)p.y, 70, Fade((Color){255, 170, 90, 255}, flicker), BLANK);
    }
    EndBlendMode();
    DrawRectangleGradientV(-40, -40, (int)ARENA_W + 80, 270, Fade((Color){15, 15, 39, 255}, .75f), BLANK);
    DrawRectangleGradientV(-40, 600, (int)ARENA_W + 80, 160, BLANK, Fade((Color){15, 15, 39, 255}, .8f));
    for (int i = 0; i < 36; ++i) {
        float x = fmodf(i * 157.7f + t * (8 + i % 9), ARENA_W + 30) - 15;
        float y = fmodf(i * 71.3f + t * (12 + i % 7), 650);
        float swing = sinf(t * .7f + i) * 20;
        DrawEllipse((int)(x + swing), (int)y, 3 + (float)(i % 2), 1.7f,
                    Fade((Color){255, 191, 218, 255}, .24f + (i % 4) * .12f));
    }
}
static void SanctuaryFront(float time, float camX, bool reducedMotion) {
    if (reducedMotion)
        return;
    // A few large, soft petals pass close to the lens.
    for (int i = 0; i < 7; ++i) {
        float speed = 30 + Hash((float)i) * 25;
        float x = fmodf(i * 263.0f + time * speed - (camX - ARENA_W / 2) * 0.4f, ARENA_W + 200) - 100;
        float y = fmodf(i * 131.0f + time * (22 + i * 3), ARENA_H + 100) - 50;
        float r = 3 + Hash(i + 4.0f) * 2.5f;
        DrawEllipse((int)(x + sinf(time + i) * 30), (int)y, r * 1.7f, r * (0.5f + 0.4f * fabsf(sinf(time * 2 + i))),
                    Fade((Color){255, 170, 205, 255}, 0.75f));
    }
}

// ---------------------------------------------------------------------------------------------
// Storm Terrace: a fully procedural cliff-top terrace in a thunderstorm.
// ---------------------------------------------------------------------------------------------
static void Rain(float time, int count, float speed, float length, float alpha, float thickness, float seed) {
    Color drop = {176, 196, 236, 255};
    for (int i = 0; i < count; ++i) {
        float fi = (float)i + seed;
        float x = fmodf(Hash(fi) * 1700 + time * speed * 0.22f, 1600) - 160;
        float y = fmodf(Hash(fi + 31) * 900 + time * speed * (0.85f + Hash(fi + 7) * 0.3f), 860) - 100;
        DrawLineEx((Vector2){x, y}, (Vector2){x - length * 0.22f, y + length}, thickness,
                   Fade(drop, alpha * (0.5f + Hash(fi + 3) * 0.5f)));
    }
}
static void Bolt(float time, float lightning) {
    float seed = floorf(time * 2.0f);
    float x = 200 + Hash(seed) * 880, y = -10;
    Color core = Fade((Color){230, 236, 255, 255}, lightning);
    Color glow = Fade((Color){150, 170, 255, 255}, lightning * 0.4f);
    for (int i = 0; i < 12; ++i) {
        float nx = x + (Hash(seed + i * 1.7f) - 0.5f) * 70, ny = y + 28 + Hash(seed + i * 3.1f) * 18;
        DrawLineEx((Vector2){x, y}, (Vector2){nx, ny}, 9, glow);
        DrawLineEx((Vector2){x, y}, (Vector2){nx, ny}, 2.5f, core);
        if (i == 5) {
            float bx = nx, by = ny;
            for (int k = 0; k < 4; ++k) {
                float cx = bx + 18 + Hash(seed + k * 9.3f) * 30, cy = by + 20 + Hash(seed + k * 5.1f) * 14;
                DrawLineEx((Vector2){bx, by}, (Vector2){cx, cy}, 1.5f, core);
                bx = cx, by = cy;
            }
        }
        x = nx, y = ny;
        if (y > 360)
            break;
    }
}
static void StoneLantern(float x, float y, float time, float lightning, bool reducedMotion) {
    Color stone = Lerp((Color){40, 44, 62, 255}, (Color){120, 128, 168, 255}, lightning * 0.6f);
    DrawRectangle((int)x - 7, (int)y - 46, 14, 46, stone);
    DrawRectangle((int)x - 15, (int)y - 74, 30, 28, stone);
    DrawTriangle((Vector2){x - 24, y - 74}, (Vector2){x + 24, y - 74}, (Vector2){x, y - 94}, stone);
    float flicker = reducedMotion ? 0.8f : 0.75f + sinf(time * 11 + x) * 0.12f + sinf(time * 27 + x * 0.3f) * 0.08f;
    DrawRectangle((int)x - 9, (int)y - 69, 18, 17, Fade((Color){255, 196, 120, 255}, flicker));
    BeginBlendMode(BLEND_ADDITIVE);
    Glow((int)x, (int)y - 60, 95, Fade((Color){255, 150, 70, 255}, 0.42f * flicker), BLANK);
    // Warm reflection streak on the wet floor.
    DrawRectangleGradientV((int)x - 8, (int)GROUND_Y - 22, 16, 150, Fade((Color){255, 160, 80, 255}, 0.3f * flicker),
                           BLANK);
    EndBlendMode();
}
static void TerraceBack(float time, float camX, float lightning, bool reducedMotion) {
    float t = reducedMotion ? 0 : time;
    float px = camX - ARENA_W / 2;
    Color skyTop = Lerp((Color){9, 11, 26, 255}, (Color){70, 78, 130, 255}, lightning * 0.7f);
    Color skyLow = Lerp((Color){62, 74, 110, 255}, (Color){150, 160, 210, 255}, lightning * 0.6f);
    DrawRectangleGradientV(-80, -80, (int)ARENA_W + 160, 560, skyTop, skyLow);
    BeginBlendMode(BLEND_ADDITIVE);
    Glow((int)(310 + px * 0.9f), 120, 220, Fade((Color){120, 140, 200, 255}, 0.18f), BLANK);
    EndBlendMode();
    // Cloud banks drift across the sky; their undersides catch the lightning.
    for (int layer = 0; layer < 3; ++layer) {
        Color cloud = Lerp((Color){22 + layer * 6, 26 + layer * 6, 46 + layer * 6, 235},
                           (Color){120, 130, 180, 235}, lightning * (0.5f - layer * 0.12f));
        for (int i = 0; i < 9; ++i) {
            float fi = (float)(i + layer * 17);
            float x = fmodf(Hash(fi) * 1600 + t * (6 + layer * 5), 1700) - 210 + px * (0.9f - layer * 0.15f);
            float y = 40 + layer * 52 + Hash(fi + 2) * 40;
            float rx = 150 + Hash(fi + 5) * 120, ry = 30 + Hash(fi + 9) * 18;
            for (int k = 0; k < 4; ++k) {
                float spread = 1 - k * 0.18f;
                Color c = cloud;
                c.a = (unsigned char)(70 + k * 30);
                DrawEllipse((int)x, (int)(y + k * 3), rx * spread, ry * spread, c);
                DrawEllipse((int)(x + rx * 0.55f), (int)(y + 10 + k * 2), rx * 0.65f * spread, ry * 0.8f * spread, c);
            }
        }
    }
    if (lightning > 0.45f && !reducedMotion)
        Bolt(time, (lightning - 0.45f) / 0.55f);
    Color far = Lerp((Color){28, 34, 56, 255}, (Color){84, 94, 140, 255}, lightning * 0.55f);
    Color near = Lerp((Color){18, 22, 38, 255}, (Color){52, 60, 96, 255}, lightning * 0.5f);
    Strip(FarRidge, -80, ARENA_W + 80, 520, -px * 0.2f, far);
    DrawRectangleGradientV(-80, 300, (int)ARENA_W + 160, 200, BLANK, Fade(skyLow, 0.45f));
    Strip(NearRidge, -80, ARENA_W + 80, 520, -px * 0.4f, near);
    // A lone watchtower on the far cliff, one window still lit.
    float tx = 1010 + px * 0.55f;
    DrawRectangle((int)tx - 26, 300, 52, 150, near);
    for (int k = 0; k < 3; ++k) {
        float ry = 300 - k * 38.0f, rw = 60 - k * 12.0f;
        DrawTriangle((Vector2){tx - rw, ry + 6}, (Vector2){tx + rw, ry + 6}, (Vector2){tx, ry - 22}, near);
        DrawRectangle((int)(tx - rw * 0.7f), (int)ry + 6, (int)(rw * 1.4f), 18, near);
    }
    float window = 0.7f + sinf(t * 3.3f) * 0.15f;
    DrawRectangle((int)tx - 6, 340, 12, 16, Fade((Color){255, 196, 120, 255}, window));
    BeginBlendMode(BLEND_ADDITIVE);
    Glow((int)tx, 348, 40, Fade((Color){255, 170, 90, 255}, 0.25f * window), BLANK);
    EndBlendMode();
    // Mist between the cliffs and the terrace.
    DrawRectangleGradientV(-80, 400, (int)ARENA_W + 160, 120, BLANK, Fade((Color){60, 72, 100, 255}, 0.55f));
    Rain(time * (reducedMotion ? 0 : 1), 160, 900, 18, 0.28f, 1, 0);
    // Balustrade and pillars at the back of the terrace.
    Color stone = Lerp((Color){44, 48, 68, 255}, (Color){132, 140, 184, 255}, lightning * 0.55f);
    Color stoneDark = Lerp((Color){30, 33, 48, 255}, (Color){90, 96, 130, 255}, lightning * 0.5f);
    float bx = -px * 0.08f;
    DrawRectangle(-80, 508, (int)ARENA_W + 160, 56, stoneDark);
    for (float x = -80 + fmodf(bx, 26); x < ARENA_W + 80; x += 26) {
        DrawRectangle((int)x + 7, 512, 12, 46, stone);
        DrawEllipse((int)x + 13, 534, 8, 9, stone);
    }
    DrawRectangle(-80, 498, (int)ARENA_W + 160, 14, stone);
    DrawRectangle(-80, 498, (int)ARENA_W + 160, 3, Lerp(stone, WHITE, 0.25f + lightning * 0.4f));
    for (int i = 0; i < 7; ++i) {
        float x = -40 + i * 228 + bx;
        DrawRectangle((int)x - 18, 470, 36, 94, stone);
        DrawRectangle((int)x - 24, 462, 48, 12, Lerp(stone, WHITE, 0.15f));
        if (i % 2 == 1)
            StoneLantern(x, 462, t, lightning, reducedMotion);
    }
    // Wet stone floor in perspective.
    Color floorNear = Lerp((Color){12, 14, 22, 255}, (Color){50, 56, 84, 255}, lightning * 0.5f);
    Color floorFar = Lerp((Color){38, 42, 60, 255}, (Color){110, 118, 160, 255}, lightning * 0.55f);
    DrawRectangleGradientV(-80, 562, (int)ARENA_W + 160, 240, floorFar, floorNear);
    DrawRectangle(-80, 562, (int)ARENA_W + 160, 2, Fade(WHITE, 0.12f + lightning * 0.3f));
    Vector2 vp = {ARENA_W / 2 + px * 0.3f, 250};
    for (int i = -12; i <= 24; ++i) {
        float x = i * 96.0f - px * 0.1f;
        float k = (562 - vp.y) / (800 - vp.y);
        Vector2 top = {vp.x + (x - vp.x) * k, 562};
        DrawLineEx(top, (Vector2){x, 800}, 1.5f, Fade(BLACK, 0.28f));
    }
    for (int i = 1; i < 9; ++i) {
        float y = 562 + powf((float)i / 9, 1.7f) * 240;
        DrawLineEx((Vector2){-80, y}, (Vector2){ARENA_W + 80, y}, 1.5f, Fade(BLACK, 0.25f));
    }
    // Puddle ripples.
    if (!reducedMotion)
        for (int i = 0; i < 14; ++i) {
            float phase = fmodf(time * 1.1f + Hash(i + 50.0f) * 7, 1);
            float x = Hash(i + 11.0f) * ARENA_W, y = 575 + Hash(i + 23.0f) * 130;
            DrawEllipseLines((int)x, (int)y, 4 + phase * 26, 1 + phase * 6, Fade((Color){170, 190, 230, 255}, 0.35f * (1 - phase)));
        }
}
static void TerraceFront(float time, float lightning, bool reducedMotion) {
    if (!reducedMotion) {
        Rain(time, 110, 1500, 34, 0.42f, 1.6f, 500);
        for (int i = 0; i < 30; ++i) {
            float phase = fmodf(time * 3 + Hash(i + 90.0f) * 5, 1);
            float x = Hash(i + 70.0f + floorf(time * 3 + Hash(i + 90.0f) * 5)) * ARENA_W;
            float y = 590 + Hash(i + 40.0f) * 110;
            DrawEllipseLines((int)x, (int)y, 3 + phase * 9, 1 + phase * 2, Fade((Color){200, 215, 245, 255}, 0.5f * (1 - phase)));
        }
    }
    DrawRectangleGradientV(-80, 620, (int)ARENA_W + 160, 180, BLANK, Fade((Color){70, 84, 120, 255}, 0.25f));
    if (lightning > 0)
        DrawRectangle(-80, -80, (int)ARENA_W + 160, (int)ARENA_H + 160, Fade((Color){200, 210, 255, 255}, lightning * 0.18f));
}

void StageDrawBack(Texture2D backdrop, StageId stage, float time, float camX, float lightning, bool reducedMotion) {
    if (stage == STAGE_STORM_TERRACE)
        TerraceBack(time, camX, lightning, reducedMotion);
    else
        SanctuaryBack(backdrop, time, camX, reducedMotion);
}
void StageDrawFront(StageId stage, float time, float camX, float lightning, bool reducedMotion) {
    if (stage == STAGE_STORM_TERRACE)
        TerraceFront(time, lightning, reducedMotion);
    else
        SanctuaryFront(time, camX, reducedMotion);
}
