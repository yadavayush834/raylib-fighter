#include "render.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const Color INK = {15, 15, 39, 255};
static const Color PAPER = {244, 241, 255, 255};
static const Color MUTED = {172, 175, 206, 255};
static const Color CYAN = {117, 229, 255, 255};
static const Color ROSE = {255, 153, 172, 255};
static const Color SPIRIT_GOLD = {255, 221, 157, 255};

static const char *Asset(const char *file) {
    static char path[2048];
    snprintf(path, sizeof(path), "%sassets/%s", GetApplicationDirectory(), file);
    if (!FileExists(path))
        snprintf(path, sizeof(path), "%s/%s", ASSET_DIR, file);
    return path;
}
void ArtLoad(Art *a) {
    memset(a, 0, sizeof(*a));
    if (FileExists(Asset("moonlit-sanctuary.png")))
        a->backdrop = LoadTexture(Asset("moonlit-sanctuary.png"));
    if (FileExists(Asset("fighters.png")))
        a->fighters = LoadTexture(Asset("fighters.png"));
    if (FileExists(Asset("walks.png")))
        a->walks = LoadTexture(Asset("walks.png"));
    if (a->fighters.id)
        SetTextureFilter(a->fighters, TEXTURE_FILTER_BILINEAR);
    if (a->walks.id)
        SetTextureFilter(a->walks, TEXTURE_FILTER_BILINEAR);
    a->display = LoadFontEx(Asset("fonts/BarlowCondensed-Bold.ttf"), 240, NULL, 0);
    a->body = LoadFontEx(Asset("fonts/Barlow-SemiBold.ttf"), 36, NULL, 0);
    if (!a->display.texture.id)
        a->display = GetFontDefault();
    if (!a->body.texture.id)
        a->body = GetFontDefault();
    if (a->backdrop.id)
        SetTextureFilter(a->backdrop, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(a->display.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(a->body.texture, TEXTURE_FILTER_BILINEAR);
}
void ArtUnload(Art *a) {
    if (a->backdrop.id)
        UnloadTexture(a->backdrop);
    if (a->fighters.id)
        UnloadTexture(a->fighters);
    if (a->walks.id)
        UnloadTexture(a->walks);
    if (a->display.texture.id != GetFontDefault().texture.id)
        UnloadFont(a->display);
    if (a->body.texture.id != GetFontDefault().texture.id)
        UnloadFont(a->body);
}
void TextLabel(Font font, const char *text, Vector2 pos, float size, Color color) {
    DrawTextEx(font, text, pos, size, size >= 40 ? 0.8f : 0.5f, color);
}
void CenterLabel(Font font, const char *text, float y, float size, Color color) {
    float width = MeasureTextEx(font, text, size, size >= 40 ? 0.8f : 0.5f).x;
    TextLabel(font, text, (Vector2){(ARENA_W - width) / 2, y}, size, color);
}
static void RightLabel(Font font, const char *text, float right, float y, float size, Color color) {
    TextLabel(font, text, (Vector2){right - MeasureTextEx(font, text, size, size >= 40 ? 0.8f : 0.5f).x, y},
              size, color);
}
static void FilledTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0) {
        Vector2 swap = b;
        b = c;
        c = swap;
    }
    DrawTriangle(a, b, c, color);
}
static void Quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color) {
    // raylib's triangle renderer expects counterclockwise vertices in screen space.
    Vector2 points[6] = {a, d, c, c, b, a};
    FilledTriangle(points[0], points[1], points[2], color);
    FilledTriangle(points[3], points[4], points[5], color);
}
static void Limb(Vector2 a, Vector2 b, float width, Color color) {
    DrawLineEx(a, b, width, color);
    DrawCircleV(a, width * 0.5f, color);
    DrawCircleV(b, width * 0.5f, color);
}
static Color Shade(Color color, float factor) {
    return (Color){(unsigned char)(color.r * factor), (unsigned char)(color.g * factor),
                   (unsigned char)(color.b * factor), color.a};
}
static Vector2 Pose(const Fighter *f, float x, float y, float scale, float bob) {
    return (Vector2){f->position.x + x * f->facing * scale, f->position.y + (y + bob) * scale};
}

void DrawArena(const Art *a, float time, bool darken) {
    if (a->backdrop.id)
        DrawTexturePro(a->backdrop, (Rectangle){0, 0, (float)a->backdrop.width, (float)a->backdrop.height},
                       (Rectangle){0, 0, ARENA_W, ARENA_H}, (Vector2){0}, 0, WHITE);
    else
        DrawRectangleGradientV(0, 0, ARENA_W, ARENA_H, (Color){27, 30, 72, 255}, (Color){147, 103, 142, 255});
    DrawRectangleGradientV(0, 0, ARENA_W, 230, Fade(INK, .78f), Fade(INK, 0));
    DrawRectangleGradientV(0, 580, ARENA_W, 140, Fade(INK, .05f), Fade(INK, .88f));
    for (int i = 0; i < 36; ++i) {
        float x = fmodf(i * 157.7f + time * (8 + i % 9), ARENA_W + 30) - 15;
        float y = fmodf(i * 71.3f + time * (12 + i % 7), 650);
        float swing = sinf(time * .7f + i) * 20;
        DrawEllipse((int)(x + swing), (int)y, 3 + (float)(i % 2), 1.7f,
                    Fade((Color){255, 191, 218, 255}, .24f + (i % 4) * .12f));
    }
    if (darken)
        DrawRectangle(0, 0, ARENA_W, ARENA_H, Fade(INK, .43f));
}

static void DrawFallbackFighter(const Fighter *f, int index, float time, float scale) {
    Color accent = index == 0 ? CYAN : ROSE;
    Color coat = index == 0 ? (Color){49, 66, 111, 255} : (Color){146, 58, 80, 255};
    Color skin = index == 0 ? (Color){244, 208, 183, 255} : (Color){245, 195, 169, 255};
    Color hair = index == 0 ? (Color){218, 233, 255, 255} : (Color){75, 29, 60, 255};
    if (f->stun > .1f && ((int)(time * 25) % 2 == 0)) {
        coat = WHITE;
        skin = PAPER;
        hair = WHITE;
    }
    float walk = fminf(1, fabsf(f->velocity.x) / 250);
    float stride = sinf(time * 15) * 17 * walk;
    float bob = f->grounded ? sinf(time * 4.5f) * 1.8f : -4;
    bool light = f->action == ACTION_LIGHT && f->actionTime > .05f && f->actionTime < .22f;
    bool heavy = f->action == ACTION_HEAVY && f->actionTime > .16f && f->actionTime < .39f;
    bool burst = f->action == ACTION_SPECIAL;
    bool dash = f->action == ACTION_DASH;
    float lean = dash ? 20 : light ? 8 : heavy ? -8 : f->blocking ? -5 : 0;
    if (!f->grounded)
        stride = 16;
    float groundShadow = 1 - fminf(.5f, (GROUND_Y - f->position.y) / 500);
    DrawEllipse((int)f->position.x, (int)(GROUND_Y + 3), 38 * scale * groundShadow, 8 * scale * groundShadow,
                Fade(INK, .45f));
    if (dash || burst) {
        for (int i = 3; i > 0; --i)
            DrawEllipse((int)(f->position.x - f->facing * i * 18), (int)(f->position.y - 65 * scale),
                        23 * scale, 50 * scale, Fade(accent, .04f * (4 - i)));
    }
    Vector2 neck = Pose(f, lean, -111, scale, bob);
    Vector2 hip = Pose(f, 0, -51, scale, bob);
    Vector2 backKnee = Pose(f, -15 - stride * .35f, -29, scale, bob);
    Vector2 backFoot = Pose(f, -22 - stride, -5, scale, 0);
    Vector2 frontKnee = Pose(f, 12 + stride * .3f, -29, scale, bob);
    Vector2 frontFoot = Pose(f, 19 + stride, -5, scale, 0);
    if (heavy) {
        frontKnee = Pose(f, 42, -68, scale, bob);
        frontFoot = Pose(f, 91, -78, scale, bob);
    }
    Color pants = (Color){26, 29, 53, 255};
    Limb(hip, backKnee, 19 * scale, INK);
    Limb(backKnee, backFoot, 16 * scale, INK);
    Limb(hip, backKnee, 13 * scale, Shade(pants, .85f));
    Limb(backKnee, backFoot, 11 * scale, pants);
    Limb(hip, frontKnee, 20 * scale, INK);
    Limb(frontKnee, frontFoot, 17 * scale, INK);
    Limb(hip, frontKnee, 14 * scale, pants);
    Limb(frontKnee, frontFoot, 12 * scale, Shade(pants, 1.2f));
    Limb(backFoot, Pose(f, -10 - stride, -4, scale, 0), 13 * scale, (Color){43, 47, 69, 255});
    Limb(frontFoot, (Vector2){frontFoot.x + 11 * f->facing * scale, frontFoot.y}, 13 * scale,
         (Color){48, 52, 76, 255});
    DrawLineEx((Vector2){frontFoot.x - 4 * scale, frontFoot.y + 4 * scale},
               (Vector2){frontFoot.x + 13 * f->facing * scale, frontFoot.y + 4 * scale}, 3 * scale, accent);
    // A long ribbon and asymmetric layers give the two silhouettes a distinct identity.
    Vector2 tailA = Pose(f, lean - 12, -108, scale, bob);
    Vector2 tailB = Pose(f, lean - 68 - walk * 20, -94 + sinf(time * 7) * 9, scale, bob);
    Vector2 tailC = Pose(f, lean - 51, -82 + sinf(time * 7 + 1) * 8, scale, bob);
    FilledTriangle(tailA, f->facing > 0 ? tailC : tailB, f->facing > 0 ? tailB : tailC, Shade(accent, .8f));
    Vector2 leftShoulder = Pose(f, lean - 19, -98, scale, bob);
    Vector2 rightShoulder = Pose(f, lean + 18, -98, scale, bob);
    Vector2 rightWaist = Pose(f, 17, -51, scale, bob);
    Vector2 leftWaist = Pose(f, -18, -51, scale, bob);
    Quad(leftShoulder, rightShoulder, rightWaist, leftWaist, INK);
    Quad(Pose(f, lean - 15, -96, scale, bob), Pose(f, lean + 14, -96, scale, bob),
         Pose(f, 13, -54, scale, bob), Pose(f, -14, -54, scale, bob), coat);
    Quad(Pose(f, lean + 1, -94, scale, bob), Pose(f, lean + 10, -91, scale, bob), Pose(f, 9, -55, scale, bob),
         Pose(f, -1, -55, scale, bob), Shade(coat, 1.23f));
    Limb(Pose(f, -16, -50, scale, bob), Pose(f, 16, -50, scale, bob), 7 * scale, accent);
    Limb(neck, Pose(f, lean, -105, scale, bob), 14 * scale, skin);
    // Rear arm, then the leading arm. Attacks have a visible wind-up and extension.
    Vector2 rearElbow = Pose(f, lean - 27, -73, scale, bob);
    Vector2 rearHand = Pose(f, lean - 9, -72, scale, bob);
    Vector2 frontElbow = Pose(f, lean + 28, -77, scale, bob);
    Vector2 frontHand = Pose(f, lean + 37, -99, scale, bob);
    if (light) {
        frontElbow = Pose(f, lean + 40, -96, scale, bob);
        frontHand = Pose(f, lean + 75, -96, scale, bob);
    }
    if (burst) {
        frontElbow = Pose(f, lean + 28, -85, scale, bob);
        frontHand = Pose(f, lean + 46, -83, scale, bob);
        rearHand = Pose(f, lean + 36, -75, scale, bob);
    }
    if (f->blocking) {
        frontElbow = Pose(f, 30, -89, scale, bob);
        frontHand = Pose(f, 21, -121, scale, bob);
        rearHand = Pose(f, 29, -105, scale, bob);
    }
    Limb(leftShoulder, rearElbow, 17 * scale, INK);
    Limb(rearElbow, rearHand, 13 * scale, INK);
    Limb(leftShoulder, rearElbow, 12 * scale, Shade(coat, .8f));
    Limb(rearElbow, rearHand, 8 * scale, skin);
    Limb(rightShoulder, frontElbow, 18 * scale, INK);
    Limb(frontElbow, frontHand, 14 * scale, INK);
    Limb(rightShoulder, frontElbow, 13 * scale, coat);
    Limb(frontElbow, frontHand, 9 * scale, skin);
    DrawCircleV(frontHand, 8 * scale, INK);
    DrawCircleV(frontHand, 5.5f * scale, accent);
    DrawCircleV(rearHand, 6 * scale, Shade(accent, .7f));
    Vector2 head = Pose(f, lean + 3, -126, scale, bob);
    DrawCircleV(head, 18 * scale, INK);
    DrawCircleV(head, 15.5f * scale, skin);
    Vector2 chin = Pose(f, lean + 9, -107, scale, bob);
    FilledTriangle(head, Pose(f, lean - 9, -118, scale, bob), chin, skin);
    if (index == 1) {
        Vector2 pony = Pose(f, lean - 28, -128, scale, bob);
        Limb(Pose(f, lean - 9, -134, scale, bob), pony, 14 * scale, INK);
        Limb(pony, Pose(f, lean - 40, -94 + sinf(time * 5) * 6, scale, bob), 10 * scale, hair);
    }
    DrawCircleSector(Pose(f, lean, -129, scale, bob), 17 * scale, 180, 360, 18, hair);
    for (int i = 0; i < 5; ++i) {
        Vector2 baseA = Pose(f, lean - 17 + i * 7, -133, scale, bob);
        Vector2 tip = Pose(f, lean - 21 + i * 10, -146 - (i % 3) * 5, scale, bob);
        Vector2 baseB = Pose(f, lean - 10 + i * 7, -125, scale, bob);
        FilledTriangle(baseA, f->facing > 0 ? baseB : tip, f->facing > 0 ? tip : baseB, hair);
    }
    Limb(Pose(f, lean + 5, -132, scale, bob), Pose(f, lean + 15, -128, scale, bob), 5 * scale, hair);
    Vector2 eye = Pose(f, lean + 12, -125, scale, bob);
    DrawLineEx((Vector2){eye.x - 4 * f->facing * scale, eye.y},
               (Vector2){eye.x + 3 * f->facing * scale, eye.y + 1 * scale}, 2 * scale, INK);
    DrawCircleV(eye, 1.6f * scale, index == 0 ? (Color){33, 125, 180, 255} : (Color){171, 49, 84, 255});
    Limb(Pose(f, lean - 9, -109, scale, bob), Pose(f, lean + 11, -107, scale, bob), 6 * scale, accent);
    if (f->blocking)
        DrawCircleLines((int)(f->position.x + f->facing * 26), (int)(f->position.y - 79), 55 * scale,
                        Fade(CYAN, .7f));
    if (burst && f->actionTime < .26f) {
        float charge = f->actionTime / .26f;
        DrawCircleV(frontHand, 15 * charge * scale, Fade(accent, .28f));
        DrawCircleLinesV(frontHand, (20 - charge * 8) * scale, Fade(accent, .9f));
    }
    if (FighterAttackActive(f)) {
        Vector2 origin = Pose(f, 27, heavy ? -73 : -96, scale, bob);
        DrawRing(origin, heavy ? 45 : 24, heavy ? 49 : 28, f->facing > 0 ? -65 : 115,
                 f->facing > 0 ? 55 : 235, 18, Fade(accent, .8f));
    }
}

void DrawFighter(const Art *a, const Fighter *f, int index, float time, float scale) {
    if (!a->fighters.id) {
        DrawFallbackFighter(f, index, time, scale);
        return;
    }
    Color accent = index == 0 ? CYAN : ROSE;
    float size = .34f * scale;
    bool light = f->action == ACTION_LIGHT && f->actionTime > .05f && f->actionTime < .22f;
    bool heavy = f->action == ACTION_HEAVY && f->actionTime > .16f && f->actionTime < .39f;
    bool special = f->action == ACTION_SPECIAL;
    bool dash = f->action == ACTION_DASH;
    int pose = heavy ? 2 : light || special ? 1 : f->blocking ? 3 : 0;
    Texture2D sheet = a->fighters;
    float frameWidth = (float)sheet.width / 4;
    float frameHeight = (float)sheet.height / 2;
    const float pivots[4] = {178, 178, 154, 196};
    float pivot = pivots[pose] / 384 * frameWidth;
    float baseline = (index == 0 ? 476 : 494) / 512.0f * frameHeight;
    float bob = f->grounded && f->action == ACTION_IDLE ? sinf(time * 4) * 1.5f : 0;
    if (a->walks.id && fabsf(f->velocity.x) > 40 && f->action == ACTION_IDLE && f->grounded && !f->blocking) {
        sheet = a->walks;
        pose = (int)(time * 9) % 4;
        frameWidth = (float)sheet.width / 4;
        frameHeight = (float)sheet.height / 2;
        pivot = frameWidth * .56f;
        baseline = frameHeight * (index == 0 ? .922f : .951f);
        bob = 0;
    }
    Rectangle source = {pose * frameWidth, index * frameHeight, f->facing > 0 ? frameWidth : -frameWidth,
                        frameHeight};
    float anchor = f->facing > 0 ? pivot : frameWidth - pivot;
    Rectangle destination = {f->position.x, f->position.y + bob, frameWidth * size, frameHeight * size};
    Vector2 origin = {anchor * size, baseline * size};
    float shadow = 1 - fminf(.5f, (GROUND_Y - f->position.y) / 500);
    DrawEllipse((int)f->position.x, (int)(GROUND_Y + 3), 34 * scale * shadow, 7 * scale * shadow,
                Fade(INK, .4f));
    if (dash) {
        for (int i = 3; i > 0; --i) {
            Rectangle trail = destination;
            trail.x -= f->facing * i * 25;
            DrawTexturePro(sheet, source, trail, origin, 0, Fade(accent, .12f * (4 - i)));
        }
    }
    Color tint = f->stun > .1f ? (Color){255, 200, 200, 255} : WHITE;
    float tilt = dash ? f->facing * 9 : !f->grounded ? f->facing * -4 : 0;
    DrawTexturePro(sheet, source, destination, origin, tilt, tint);
    if (f->blocking)
        DrawCircleLines((int)(f->position.x + f->facing * 25), (int)(f->position.y - 125), 70 * scale,
                        Fade(CYAN, .55f));
    if (special && f->actionTime < .25f) {
        Vector2 hand = {f->position.x + f->facing * 64 * scale, f->position.y - 116 * scale};
        float charge = f->actionTime / .25f;
        DrawCircleV(hand, (8 + charge * 13) * scale, Fade(accent, .4f));
        DrawCircleLinesV(hand, (30 - charge * 9) * scale, accent);
    }
    if (FighterAttackActive(f)) {
        Vector2 center = {f->position.x + f->facing * 35 * scale, f->position.y - 115 * scale};
        DrawRing(center, heavy ? 51 : 28, heavy ? 55 : 31, f->facing > 0 ? -65 : 115,
                 f->facing > 0 ? 55 : 235, 18, Fade(accent, .8f));
    }
}

static void StatusBar(const Art *a, const Fighter *f, int index) {
    bool right = index == 1;
    Color accent = right ? ROSE : CYAN;
    float x = right ? 762 : 64;
    const char *name = right ? "KAE" : "REN";
    const char *subtitle = right ? "EMBERHEART" : "THE STORMBOUND";
    TextLabel(a->display, name, (Vector2){x, 43}, 37, PAPER);
    TextLabel(a->body, subtitle, (Vector2){x + 64, 56}, 13, Fade(accent, .9f));
    TextLabel(a->body, right ? "02" : "01", (Vector2){x - 21, 59}, 11, Fade(MUTED, .65f));
    DrawRectangle((int)x, 92, 454, 21, Fade(INK, .75f));
    DrawRectangleLinesEx((Rectangle){x - 2, 90, 458, 25}, 1, Fade(PAPER, .3f));
    float fraction = fmaxf(0, f->health) / MAX_HEALTH;
    float trail = fmaxf(0, f->healthTrail) / MAX_HEALTH;
    float healthX = right ? x + 454 * (1 - fraction) : x;
    float trailX = right ? x + 454 * (1 - trail) : x;
    DrawRectangle((int)trailX, 94, (int)(454 * trail), 17, Fade(SPIRIT_GOLD, .75f));
    DrawRectangleGradientH((int)healthX, 94, (int)(454 * fraction), 17, Shade(accent, .7f), accent);
    for (int i = 1; i < 10; ++i)
        DrawLine((int)(x + i * 45.4f), 94, (int)(x + i * 45.4f), 111, Fade(INK, .3f));
    float meterX = right ? x + 454 - f->energy * 4.54f : x;
    DrawRectangle((int)x, 123, 454, 3, Fade(PAPER, .12f));
    DrawRectangle((int)meterX, 123, (int)(f->energy * 4.54f), 3, accent);
    TextLabel(a->body, "SPIRIT", (Vector2){x, 133}, 10, Fade(accent, .8f));
    RightLabel(a->body, TextFormat("%03i", (int)f->health), x + 454, 133, 10, MUTED);
    for (int i = 0; i < 2; ++i) {
        float pipX = right ? x + 433 - i * 21 : x + 10 + i * 21;
        DrawCircleV((Vector2){pipX, 163}, 5, i < f->wins ? SPIRIT_GOLD : Fade(PAPER, .15f));
        DrawCircleLines((int)pipX, 163, 5, Fade(PAPER, .25f));
    }
}

static void Keycap(const Art *a, const char *key, float x, float y, float width) {
    DrawRectangleRounded((Rectangle){x, y, width, 24}, .18f, 4, Fade(PAPER, .07f));
    DrawRectangleRoundedLinesEx((Rectangle){x, y, width, 24}, .18f, 4, 1, Fade(PAPER, .18f));
    float textWidth = MeasureTextEx(a->body, key, 13, .5f).x;
    TextLabel(a->body, key, (Vector2){x + (width - textWidth) / 2, y + 4}, 13, PAPER);
}
static void Controls(const Art *a, bool local) {
    DrawRectangle(0, 647, ARENA_W, 73, Fade(INK, .8f));
    DrawLine(36, 647, 1244, 647, Fade(PAPER, .1f));
    TextLabel(a->body, "PLAYER 01", (Vector2){40, 665}, 12, CYAN);
    Keycap(a, "A D", 112, 661, 45);
    TextLabel(a->body, "MOVE", (Vector2){165, 667}, 11, MUTED);
    Keycap(a, "W", 209, 661, 24);
    TextLabel(a->body, "JUMP", (Vector2){241, 667}, 11, MUTED);
    Keycap(a, "J", 287, 661, 24);
    TextLabel(a->body, "STRIKE", (Vector2){319, 667}, 11, MUTED);
    Keycap(a, "K", 368, 661, 24);
    TextLabel(a->body, "HEAVY", (Vector2){400, 667}, 11, MUTED);
    Keycap(a, "S", 450, 661, 24);
    TextLabel(a->body, "GUARD", (Vector2){482, 667}, 11, MUTED);
    Keycap(a, "L", 534, 661, 24);
    TextLabel(a->body, "BURST", (Vector2){566, 667}, 11, MUTED);
    Keycap(a, "SPACE", 617, 661, 48);
    TextLabel(a->body, "DASH", (Vector2){673, 667}, 11, MUTED);
    if (local)
        TextLabel(a->body, "P2  ARROWS + , . /   |   R-SHIFT DASH", (Vector2){815, 667}, 13, ROSE);
    else
        TextLabel(a->body, "ESC  PAUSE    /    M  SOUND    /    F10  FULLSCREEN", (Vector2){895, 667}, 12,
                  MUTED);
    CenterLabel(a->body, "MOONLIT SANCTUARY     /     SPIRIT CLASH", 701, 9, Fade(MUTED, .7f));
}

void DrawMatch(const Art *a, const Game *g, bool hitboxes, bool reducedMotion) {
    DrawArena(a, reducedMotion ? 0 : g->totalTime, false);
    float shake = reducedMotion ? 0 : g->shake;
    rlPushMatrix();
    rlTranslatef(sinf(g->totalTime * 175) * shake, cosf(g->totalTime * 143) * shake * .5f, 0);
    DrawFighter(a, &g->fighters[0], 0, g->totalTime, 1.45f);
    DrawFighter(a, &g->fighters[1], 1, g->totalTime + 1, 1.45f);
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        const Projectile *p = &g->projectiles[i];
        if (!p->active)
            continue;
        Color accent = p->owner == 0 ? CYAN : ROSE;
        for (int j = 3; j > 0; --j)
            DrawCircleV((Vector2){p->position.x - p->velocity * .015f * j, p->position.y}, 13 + (float)j,
                        Fade(accent, .1f * (4 - j)));
        DrawCircleV(p->position, 24, Fade(accent, .15f));
        DrawCircleV(p->position, 15, accent);
        DrawCircleV(p->position, 8, PAPER);
        DrawCircleLinesV(p->position, 21 + sinf(g->totalTime * 30) * 3, accent);
    }
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        const Particle *p = &g->particles[i];
        if (p->life <= 0)
            continue;
        float life = p->life / p->maxLife;
        DrawLineEx(p->position,
                   (Vector2){p->position.x - p->velocity.x * .025f, p->position.y - p->velocity.y * .025f},
                   p->radius * life, Fade(p->color, life));
    }
    if (hitboxes) {
        for (int i = 0; i < 2; ++i) {
            DrawRectangleLinesEx(FighterHurtbox(&g->fighters[i]), 1, LIME);
            if (FighterAttackActive(&g->fighters[i]))
                DrawRectangleLinesEx(FighterAttackbox(&g->fighters[i]), 1, RED);
        }
    }
    rlPopMatrix();
    TextLabel(a->display, "SPIRIT / CLASH", (Vector2){36, 14}, 15, Fade(PAPER, .7f));
    RightLabel(a->body,
               g->mode == MODE_LOCAL      ? "LOCAL VERSUS"
               : g->mode == MODE_TRAINING ? "TRAINING ROOM"
               : g->mode == MODE_DEMO     ? "EXHIBITION"
                                          : "SOLO DUEL  /  CPU",
               1243, 15, 12, Fade(PAPER, .65f));
    StatusBar(a, &g->fighters[0], 0);
    StatusBar(a, &g->fighters[1], 1);
    CenterLabel(a->body, "BEST OF THREE", 43, 11, MUTED);
    CenterLabel(a->display, g->mode == MODE_TRAINING ? "--" : TextFormat("%02i", (int)ceilf(g->roundTime)),
                58, 53, g->roundTime < 10 ? ROSE : PAPER);
    CenterLabel(a->body, TextFormat("ROUND %02i", g->round), 124, 12, SPIRIT_GOLD);
    for (int i = 0; i < 2; ++i) {
        if (g->fighters[i].combo >= 2) {
            float x = i == 0 ? 81 : 1090;
            TextLabel(a->display, TextFormat("%i", g->fighters[i].combo), (Vector2){x, 240}, 60,
                      i == 0 ? CYAN : ROSE);
            TextLabel(a->body, "HIT STREAK", (Vector2){x, 303}, 13, PAPER);
        }
    }
    Controls(a, g->mode == MODE_LOCAL);
    if (g->phase == ROUND_INTRO) {
        float t = g->phaseTime;
        const char *label = t < 1.25f ? TextFormat("ROUND %02i", g->round) : "ENGAGE!";
        CenterLabel(a->display, label, 259, 90, t < 1.25f ? PAPER : SPIRIT_GOLD);
        CenterLabel(a->body, t < 1.25f ? "FIND YOUR SPIRIT." : "MAKE THE FIRST MOVE.", 361, 14,
                    Fade(PAPER, .8f));
    } else if (g->phase == ROUND_END) {
        DrawRectangle(0, 205, ARENA_W, 230, Fade(INK, .32f));
        CenterLabel(a->display, g->winner < 0 ? "DRAW" : "K.O.", 244, 110, SPIRIT_GOLD);
        CenterLabel(a->body,
                    g->winner < 0    ? "ANOTHER CHANCE. SAME FIGHTING SPIRIT."
                    : g->winner == 0 ? "REN TAKES THE ROUND"
                                     : "KAE TAKES THE ROUND",
                    361, 20, PAPER);
    }
}

static void MenuButton(const Art *a, const char *label, const char *detail, float y, int item, int selected) {
    bool active = item == selected;
    Rectangle rec = {76, y, 410, 64};
    DrawRectangleRounded(rec, .1f, 4, Fade(active ? CYAN : INK, active ? .95f : .6f));
    if (!active)
        DrawRectangleRoundedLinesEx(rec, .1f, 4, 1, Fade(PAPER, .2f));
    TextLabel(a->display, label, (Vector2){95, y + 7}, 28, active ? INK : PAPER);
    TextLabel(a->body, detail, (Vector2){96, y + 40}, 12, active ? Fade(INK, .7f) : MUTED);
    TextLabel(a->body, active ? ">" : "", (Vector2){456, y + 17}, 25, INK);
}
void DrawMenu(const Art *a, float time, int selected, int difficulty, bool muted) {
    DrawArena(a, time, true);
    DrawRectangleGradientH(0, 0, 830, ARENA_H, Fade(INK, .97f), Fade(INK, 0));
    TextLabel(a->display, "RAYLIB FIGHTER", (Vector2){78, 47}, 17, CYAN);
    TextLabel(a->body, "A SMALL GAME. A FIGHTING SPIRIT.", (Vector2){78, 77}, 12, MUTED);
    TextLabel(a->display, "SPIRIT", (Vector2){70, 104}, 113, PAPER);
    TextLabel(a->display, "CLASH", (Vector2){70, 203}, 113, CYAN);
    TextLabel(a->body, "Two rivals. One moonlit arena.", (Vector2){78, 329}, 23, PAPER);
    TextLabel(a->body, "Dash through danger. Turn the tide. Find your rhythm.", (Vector2){78, 362}, 17,
              MUTED);
    MenuButton(a, "SOLO DUEL", "YOU VS THE COMPUTER", 414, 0, selected);
    MenuButton(a, "LOCAL VERSUS", "TWO PLAYERS. ONE KEYBOARD.", 490, 1, selected);
    MenuButton(a, "TRAINING", "PRACTISE WITHOUT A TIMER", 566, 2, selected);
    const char *levels[] = {"EASY", "NORMAL", "HARD"};
    TextLabel(a->body, TextFormat("<  CPU DIFFICULTY: %s  >", levels[difficulty]), (Vector2){77, 655}, 15,
              SPIRIT_GOLD);
    RightLabel(a->body, "MOONLIT SANCTUARY", 1210, 61, 14, PAPER);
    RightLabel(a->body, "ORIGINAL ARENA  /  ORIGINAL FIGHTERS", 1210, 84, 10, MUTED);
    Fighter showcase = {.position = {895, 609}, .grounded = true, .facing = -1};
    DrawFighter(a, &showcase, 0, time, 2.35f);
    DrawLine(664, 625, 1188, 625, Fade(CYAN, .3f));
    TextLabel(a->display, "REN", (Vector2){720, 642}, 43, PAPER);
    TextLabel(a->body, "THE STORMBOUND", (Vector2){801, 665}, 14, CYAN);
    RightLabel(a->body, muted ? "M  SOUND OFF" : "M  SOUND ON", 1210, 686, 12, MUTED);
    TextLabel(a->body, "ENTER  SELECT    /    UP DOWN  NAVIGATE    /    Q  QUIT", (Vector2){77, 694}, 11,
              Fade(MUTED, .75f));
}
void DrawPause(const Art *a, int selected, bool muted, bool reducedMotion) {
    DrawRectangle(0, 0, ARENA_W, ARENA_H, Fade(INK, .8f));
    CenterLabel(a->body, "TAKE A BREATHER", 156, 15, CYAN);
    CenterLabel(a->display, "PAUSED", 190, 77, PAPER);
    const char *items[] = {"RESUME FIGHT", "RESTART MATCH", "BACK TO TITLE"};
    for (int i = 0; i < 3; ++i) {
        Rectangle rec = {449, 300 + i * 56, 382, 45};
        DrawRectangleRounded(rec, .15f, 4, Fade(i == selected ? CYAN : PAPER, i == selected ? .95f : .05f));
        CenterLabel(a->display, items[i], rec.y + 7, 25, i == selected ? INK : MUTED);
    }
    CenterLabel(a->body, muted ? "M  SOUND OFF" : "M  SOUND ON", 502, 15, MUTED);
    CenterLabel(a->body, reducedMotion ? "V  REDUCED MOTION ON" : "V  REDUCED MOTION OFF", 530, 15, MUTED);
    CenterLabel(a->body, "ESC  RESUME    /    ENTER  SELECT", 579, 13, Fade(MUTED, .7f));
}
void DrawResults(const Art *a, const Game *g, int selected) {
    DrawRectangle(0, 0, ARENA_W, ARENA_H, Fade(INK, .78f));
    CenterLabel(a->body, "THE SANCTUARY HAS A CHAMPION", 141, 15, SPIRIT_GOLD);
    CenterLabel(a->display, g->winner == 0 ? "REN WINS" : "KAE WINS", 175, 104, g->winner == 0 ? CYAN : ROSE);
    CenterLabel(a->body, TextFormat("FINAL SCORE    %i  :  %i", g->fighters[0].wins, g->fighters[1].wins),
                296, 23, PAPER);
    const char *items[] = {"RUN IT BACK", "BACK TO TITLE"};
    for (int i = 0; i < 2; ++i) {
        DrawRectangleRounded((Rectangle){449, 381 + i * 62, 382, 49}, .12f, 4,
                             i == selected ? CYAN : Fade(PAPER, .07f));
        CenterLabel(a->display, items[i], 390 + i * 62, 27, i == selected ? INK : MUTED);
    }
    CenterLabel(a->body, "EVERY CLASH IS A NEW BEGINNING.", 562, 14, MUTED);
}
