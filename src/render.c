#include "render.h"
#include "rlgl.h"
#include "stage.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static float density = 1;

static const char *Asset(const char *file) {
    static char path[2048];
    snprintf(path, sizeof(path), "%sassets/%s", GetApplicationDirectory(), file);
    if (!FileExists(path))
        snprintf(path, sizeof(path), "%s/%s", ASSET_DIR, file);
    return path;
}
static Texture2D LoadArt(const char *file) {
    Texture2D t = {0};
    if (FileExists(Asset(file))) {
        t = LoadTexture(Asset(file));
        GenTextureMipmaps(&t);
        SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    }
    return t;
}
void ArtLoad(Art *a) {
    memset(a, 0, sizeof(*a));
    a->backdrop = LoadArt("moonlit-sanctuary.png");
    a->fighters = LoadArt("fighters.png");
    a->walks = LoadArt("walks.png");
    a->hurt = LoadArt("hurt.png");
    a->display = LoadFontEx(Asset("fonts/BarlowCondensed-Bold.ttf"), 240, NULL, 0);
    a->body = LoadFontEx(Asset("fonts/Barlow-SemiBold.ttf"), 48, NULL, 0);
    if (!a->display.texture.id)
        a->display = GetFontDefault();
    if (!a->body.texture.id)
        a->body = GetFontDefault();
    SetTextureFilter(a->display.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(a->body.texture, TEXTURE_FILTER_BILINEAR);
}
void ArtUnload(Art *a) {
    Texture2D *textures[] = {&a->backdrop, &a->fighters, &a->walks, &a->hurt};
    for (int i = 0; i < 4; ++i)
        if (textures[i]->id)
            UnloadTexture(*textures[i]);
    if (a->display.texture.id != GetFontDefault().texture.id)
        UnloadFont(a->display);
    if (a->body.texture.id != GetFontDefault().texture.id)
        UnloadFont(a->body);
}

void RenderSetDensity(float d) {
    density = d;
}
float RenderDensity(void) {
    return density;
}
Camera2D RenderBaseCamera(void) {
    return (Camera2D){.zoom = density};
}

// ---------------------------------------------------------------------------------------------
// Text and colour helpers
// ---------------------------------------------------------------------------------------------
static float Spacing(float size) {
    return size >= 40 ? 0.8f : 0.5f;
}
void TextLabel(Font font, const char *text, Vector2 pos, float size, Color color) {
    DrawTextEx(font, text, pos, size, Spacing(size), color);
}
void CenterLabelAt(Font font, const char *text, float x, float y, float size, Color color) {
    float width = MeasureTextEx(font, text, size, Spacing(size)).x;
    TextLabel(font, text, (Vector2){x - width / 2, y}, size, color);
}
void CenterLabel(Font font, const char *text, float y, float size, Color color) {
    CenterLabelAt(font, text, ARENA_W / 2, y, size, color);
}
void RightLabel(Font font, const char *text, float right, float y, float size, Color color) {
    TextLabel(font, text, (Vector2){right - MeasureTextEx(font, text, size, Spacing(size)).x, y}, size, color);
}
Color Shade(Color color, float factor) {
    return (Color){(unsigned char)fminf(255, color.r * factor), (unsigned char)fminf(255, color.g * factor),
                   (unsigned char)fminf(255, color.b * factor), color.a};
}
Color MixColor(Color a, Color b, float t) {
    t = fmaxf(0, fminf(1, t));
    return (Color){(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                   (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}

// ---------------------------------------------------------------------------------------------
// Sprite poses
// ---------------------------------------------------------------------------------------------
typedef enum { SHEET_FIGHT, SHEET_WALK, SHEET_HURT } SheetKind;
typedef struct {
    SheetKind sheet;
    int column;
    float rotation, lift, squash;
} PoseChoice;

static const Texture2D *Sheet(const Art *a, SheetKind kind) {
    if (kind == SHEET_WALK && a->walks.id)
        return &a->walks;
    if (kind == SHEET_HURT && a->hurt.id)
        return &a->hurt;
    return &a->fighters;
}

static PoseChoice ChoosePose(const Fighter *f, const Game *g, int index, float time) {
    float t = f->actionTime;
    float dir = (float)f->facing;
    PoseChoice p = {SHEET_FIGHT, 0, 0, 0, 1};
    if (f->knockdown > 0) {
        float wake = f->health > 0 && f->knockdown < 0.2f ? f->knockdown / 0.2f : 1;
        p.sheet = SHEET_HURT;
        p.column = 1;
        p.rotation = f->hurtDirection * 84 * wake;
        p.lift = 26 * wake;
        return p;
    }
    if (f->launched) {
        float fall = fmaxf(0, fminf(1, (f->velocity.y + 700) / 1400));
        p.sheet = SHEET_HURT;
        p.column = 1;
        p.rotation = f->hurtDirection * (18 + fall * 62);
        return p;
    }
    if (f->stun > 0 && f->hurtDuration > 0) {
        float progress = fminf(1, 1 - f->stun / f->hurtDuration);
        p.sheet = SHEET_HURT;
        p.column = f->health <= 0 ? 1 : f->hurtDuration >= 0.7f ? 2 : (int)fminf(3, progress * 4);
        p.rotation = f->hurtDirection * 8 * (1 - progress);
        return p;
    }
    if (f->blocking || f->blockstun > 0) {
        p.column = 3;
        p.rotation = f->blockstun > 0 ? -dir * 4 : 0;
        return p;
    }
    if (g && (g->phase == ROUND_END || g->phase == MATCH_END) && g->winner == index && g->phaseTime > 0.9f &&
        f->grounded && f->action == ACTION_IDLE) {
        p.column = 1;
        p.squash = 1 + sinf(time * 3) * 0.01f;
        return p;
    }
    switch (f->action) {
    case ACTION_LIGHT:
        p.column = t < 0.05f ? 0 : t < 0.22f ? 1 : 0;
        p.rotation = t < 0.05f ? -dir * 3 : t < 0.15f ? dir * 3 : 0;
        return p;
    case ACTION_HEAVY:
        p.column = t < 0.16f ? 3 : t < 0.40f ? 2 : 0;
        p.rotation = t < 0.16f ? -dir * 6 : 0;
        return p;
    case ACTION_SPECIAL:
        if (f->character == CHAR_KAE) {
            if (t < 0.05f) {
                p.squash = 0.9f;
                return p;
            }
            p.column = 1;
            p.rotation = -dir * (f->velocity.y < 0 ? 52 : 25);
            return p;
        }
        p.column = t < 0.22f ? 3 : 1;
        return p;
    case ACTION_AIR:
        p.column = 1;
        p.rotation = dir * 10;
        return p;
    case ACTION_DIVE:
        p.column = 2;
        p.rotation = dir * 38;
        return p;
    case ACTION_DASH:
        p.sheet = SHEET_WALK;
        p.column = 1;
        p.rotation = dir * 11;
        return p;
    case ACTION_BACKDASH:
        p.rotation = -dir * 9;
        return p;
    case ACTION_SUPER:
        if (g && g->superFreeze > 0 && g->superOwner == index) {
            p.column = 3;
            p.squash = 1 + sinf(time * 30) * 0.015f;
            return p;
        }
        if (f->character == CHAR_REN) {
            p.column = t < 0.1f ? 3 : t < 0.9f ? 1 : 0;
            p.rotation = t > 0.1f && t < 0.85f ? -dir * 4 : 0;
            return p;
        }
        if (!f->connected) {
            p.sheet = SHEET_WALK;
            p.column = 2;
            p.rotation = dir * 15;
            return p;
        }
        p.column = f->superHits % 2 ? 1 : 2;
        p.rotation = dir * (f->superHits % 3 - 1) * 6.0f;
        return p;
    default:
        break;
    }
    if (!f->grounded) {
        p.rotation = f->velocity.y < 0 ? -dir * 5 : dir * 4;
        p.squash = f->velocity.y < -500 ? 1.04f : 1;
        return p;
    }
    if (fabsf(f->velocity.x) > 40) {
        p.sheet = SHEET_WALK;
        p.column = (int)(time * 9) % 4;
        if (f->velocity.x * dir < 0)
            p.column = 3 - p.column; // walk cycle runs backwards when retreating
        return p;
    }
    p.squash = 1 + sinf(time * 4) * 0.008f;
    return p;
}

static void DrawSheetFrame(const Art *a, PoseChoice pose, int character, Vector2 feet, int facing, float scale,
                           Color tint, bool mirrored) {
    const Texture2D *sheet = Sheet(a, pose.sheet);
    if (!sheet->id)
        return;
    float frameWidth = (float)sheet->width / 4, frameHeight = (float)sheet->height / 2;
    float pivot, baseline;
    float base = (character == CHAR_REN ? 476.0f : 494.0f) / 512;
    if (sheet == &a->walks) {
        pivot = frameWidth * 0.56f;
        baseline = frameHeight * (character == CHAR_REN ? 0.922f : 0.951f);
    } else if (sheet == &a->hurt) {
        pivot = frameWidth * 0.5f;
        baseline = frameHeight * base;
    } else {
        const float pivots[4] = {178, 178, 154, 196};
        pivot = pivots[pose.column] / 384 * frameWidth;
        baseline = frameHeight * base;
    }
    float size = 0.34f * scale;
    Rectangle source = {pose.column * frameWidth, character * frameHeight, facing > 0 ? frameWidth : -frameWidth,
                        frameHeight};
    float anchor = facing > 0 ? pivot : frameWidth - pivot;
    float width = frameWidth * size * (2 - pose.squash);
    float height = frameHeight * size * pose.squash;
    Rectangle dest = {feet.x, feet.y - pose.lift * scale / 1.45f, width, height};
    Vector2 origin = {anchor * size * (2 - pose.squash), baseline * size * pose.squash};
    float rotation = pose.rotation;
    if (mirrored) {
        // Reflect about the floor line: flip the frame and mirror the anchor and angle.
        source.height = -source.height;
        origin.y = height - origin.y;
        dest.y = 2 * GROUND_Y - dest.y;
        rotation = -rotation;
    }
    DrawTexturePro(*sheet, source, dest, origin, rotation, tint);
}

void DrawFighterPose(const Art *a, const Fighter *f, const Game *g, int index, float time, float scale, Color tint,
                     bool mirrored) {
    PoseChoice pose = ChoosePose(f, g, index, time);
    Vector2 feet = f->position;
    if (g && g->hitstop > 0 && f->stun > 0 && f->hurtDuration > 0 && !mirrored)
        feet.x += sinf(time * 140) * 5; // the victim shudders while the impact freezes
    DrawSheetFrame(a, pose, f->character, feet, f->facing, scale, tint, mirrored);
    if (mirrored || tint.a < 255 || !g)
        return;
    bool hurt = f->stun > 0 && f->hurtDuration > 0 && !f->launched;
    float progress = hurt ? fminf(1, 1 - f->stun / f->hurtDuration) : 1;
    if ((hurt && f->health > 0 && progress < 0.3f) || (g->hitstop > 0 && f->stun > 0)) {
        BeginBlendMode(BLEND_ADDITIVE);
        float amount = g->hitstop > 0 ? 0.75f : 0.55f * (1 - progress / 0.3f);
        Color flash = f->counterHit ? (Color){255, 150, 90, 255} : WHITE;
        DrawSheetFrame(a, pose, f->character, feet, f->facing, scale, Fade(flash, amount), false);
        EndBlendMode();
    }
}

void DrawCharacterShowcase(const Art *a, int character, Vector2 feet, float scale, int facing, int column, Color tint) {
    PoseChoice pose = {SHEET_FIGHT, column, 0, 0, 1};
    DrawSheetFrame(a, pose, character, feet, facing, scale, tint, false);
}

void DrawPortrait(const Art *a, int character, Rectangle dest, bool flip, Color tint) {
    if (!a->fighters.id) {
        DrawRectangleRec(dest, Fade(CharacterGet(character)->accent, 0.4f));
        return;
    }
    float fw = (float)a->fighters.width / 4, fh = (float)a->fighters.height / 2;
    Rectangle source = {fw * (character == CHAR_REN ? 0.40f : 0.33f), character * fh + fh * 0.015f, fw * 0.36f,
                        fw * 0.36f};
    if (flip)
        source.width = -source.width;
    DrawTexturePro(a->fighters, source, dest, (Vector2){0}, 0, tint);
}

void DrawStagePreview(const Art *a, StageId stage, Rectangle dest, float time) {
    BeginScissorMode((int)(dest.x * density), (int)(dest.y * density), (int)(dest.width * density),
                     (int)(dest.height * density));
    float zoom = dest.width / ARENA_W;
    BeginMode2D((Camera2D){.offset = {dest.x * density, dest.y * density}, .zoom = zoom * density});
    StageDrawBack(a->backdrop, stage, time, ARENA_W / 2, 0, false);
    StageDrawFront(stage, time, ARENA_W / 2, 0, false);
    EndMode2D();
    EndScissorMode();
    BeginMode2D(RenderBaseCamera());
}

// ---------------------------------------------------------------------------------------------
// Effects tied to fighters
// ---------------------------------------------------------------------------------------------
static Vector2 Hand(const Fighter *f, float scale, float reach) {
    return (Vector2){f->position.x + f->facing * reach * scale, f->position.y - 116 * scale};
}
static void Swoosh(Vector2 center, float radius, float width, float from, float to, Color color) {
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < 4; ++i) {
        float lag = i * (to - from) * 0.12f;
        DrawRing(center, radius - width * (1 - i * 0.2f), radius - i * 1.5f, from - lag, to - lag, 24, Fade(color, 0.32f - i * 0.07f));
    }
    EndBlendMode();
}
static void DrawAttackFx(const Fighter *f, const Game *g, Color accent, float time) {
    const float scale = 1.45f;
    float t = f->actionTime;
    bool right = f->facing > 0;
    if (FighterAttackActive(f) && f->action != ACTION_SUPER) {
        Vector2 c = {f->position.x + f->facing * 30 * scale, f->position.y - 112 * scale};
        switch (f->action) {
        case ACTION_LIGHT:
            Swoosh(c, 54, 10, right ? -40 : 140, right ? 40 : 220, accent);
            break;
        case ACTION_HEAVY:
            Swoosh((Vector2){c.x, c.y + 10}, 92, 22, right ? -110 : 120, right ? 60 : 290, accent);
            break;
        case ACTION_AIR:
            Swoosh((Vector2){c.x, c.y + 25}, 60, 12, right ? -20 : 110, right ? 70 : 200, accent);
            break;
        case ACTION_DIVE:
            Swoosh((Vector2){f->position.x + f->facing * 30, f->position.y - 40}, 70, 16, right ? 10 : 100,
                   right ? 80 : 170, accent);
            break;
        case ACTION_SPECIAL:
            BeginBlendMode(BLEND_ADDITIVE);
            for (int i = 0; i < 6; ++i) {
                float y = f->position.y - 40 - i * 34;
                DrawCircleV((Vector2){f->position.x + f->facing * (24 - i * 4), y}, 34 - i * 3,
                            Fade(MixColor(accent, (Color){255, 220, 140, 255}, i / 6.0f), 0.32f));
            }
            EndBlendMode();
            break;
        default:
            break;
        }
    }
    if (f->action == ACTION_SPECIAL && f->character == CHAR_REN && t < 0.26f) {
        Vector2 hand = Hand(f, scale, 58);
        float charge = t / 0.26f;
        BeginBlendMode(BLEND_ADDITIVE);
        Glow((int)hand.x, (int)hand.y, 40 * charge + 8, Fade(accent, 0.7f), BLANK);
        DrawCircleLinesV(hand, (36 - charge * 20), accent);
        DrawCircleV(hand, 6 + charge * 8, Fade(WHITE, 0.9f));
        EndBlendMode();
    }
    if (f->action != ACTION_SUPER)
        return;
    bool frozen = g->superFreeze > 0;
    if (f->character == CHAR_REN) {
        Vector2 hand = Hand(f, scale, 62);
        BeginBlendMode(BLEND_ADDITIVE);
        if (frozen || t < 0.12f) {
            float charge = frozen ? 1 - g->superFreeze / SUPER_FREEZE : 1;
            Glow((int)hand.x, (int)hand.y, 30 + 70 * charge, Fade(accent, 0.8f), BLANK);
            for (int i = 0; i < 3; ++i) {
                float spin = time * 400 * (i % 2 ? 1 : -1) + i * 60;
                float r = 40 + i * 18 - charge * 20;
                DrawRing(hand, r, r + 4, spin, spin + 220, 24, Fade(WHITE, 0.6f));
            }
            DrawCircleV(hand, 10 + charge * 10, WHITE);
        } else if (t < 0.92f) {
            float grow = fminf(1, (t - 0.12f) / 0.07f);
            float fade = t > 0.82f ? 1 - (t - 0.82f) / 0.1f : 1;
            float length = 1600 * grow;
            float wobble = 1 + sinf(time * 70) * 0.08f;
            float x0 = f->facing > 0 ? hand.x : hand.x - length;
            float cy = hand.y;
            DrawRectangleGradientV((int)x0, (int)(cy - 70 * wobble), (int)length, (int)(70 * wobble), BLANK,
                                   Fade(accent, 0.38f * fade));
            DrawRectangleGradientV((int)x0, (int)cy, (int)length, (int)(70 * wobble), Fade(accent, 0.38f * fade), BLANK);
            DrawRectangleGradientV((int)x0, (int)(cy - 24 * wobble), (int)length, (int)(24 * wobble), Fade(accent, 0.1f * fade),
                                   Fade(accent, 0.55f * fade));
            DrawRectangleGradientV((int)x0, (int)cy, (int)length, (int)(24 * wobble), Fade(accent, 0.55f * fade),
                                   Fade(accent, 0.1f * fade));
            DrawRectangle((int)x0, (int)(cy - 6 * wobble), (int)length, (int)(12 * wobble), Fade(WHITE, 0.8f * fade));
            for (int k = 0; k < 3; ++k) {
                float y = cy, x = hand.x;
                for (int s = 0; s < 22; ++s) {
                    float nx = x + f->facing * 70;
                    float ny = cy + sinf(time * 90 + s * 1.7f + k * 2.1f) * 34;
                    DrawLineEx((Vector2){x, y}, (Vector2){nx, ny}, 2, Fade(MixColor(accent, WHITE, 0.6f), 0.55f * fade));
                    x = nx, y = ny;
                }
            }
            Glow(hand.x, cy, 100 * wobble, Fade(MixColor(accent, WHITE, 0.5f), 0.6f * fade), BLANK);
        }
        EndBlendMode();
    } else if (frozen) {
        BeginBlendMode(BLEND_ADDITIVE);
        float charge = 1 - g->superFreeze / SUPER_FREEZE;
        for (int i = 0; i < 10; ++i) {
            float a = i * 36 * DEG2RAD + time * 3;
            float r = 150 - charge * 90;
            Vector2 p = {f->position.x + cosf(a) * r, f->position.y - 120 + sinf(a) * r * 0.8f};
            Glow((int)p.x, (int)p.y, 24, Fade(accent, 0.7f), BLANK);
        }
        EndBlendMode();
    }
}

static void DrawProjectile(const Projectile *p, Color accent, float time) {
    float dir = p->velocity > 0 ? 1.0f : -1.0f;
    BeginBlendMode(BLEND_ADDITIVE);
    for (int j = 6; j > 0; --j)
        DrawCircleV((Vector2){p->position.x - dir * j * 14, p->position.y + sinf(time * 30 + j) * 3}, 18 - j * 2.0f,
                    Fade(accent, 0.07f * (7 - j)));
    Glow((int)p->position.x, (int)p->position.y, 52, Fade(accent, 0.55f), BLANK);
    float spin = time * 720 * dir;
    for (int k = 0; k < 3; ++k)
        DrawRing(p->position, 18, 25, spin + k * 120, spin + k * 120 + 75, 12, Fade(WHITE, 0.8f));
    EndBlendMode();
    DrawCircleV(p->position, 14, accent);
    DrawCircleV(p->position, 8, PAPER);
}

static void DrawShadow(const Fighter *f) {
    float height = fmaxf(0, GROUND_Y - f->position.y);
    float s = 1 - fminf(0.55f, height / 450);
    float width = f->knockdown > 0 ? 90 : 50;
    DrawEllipse((int)f->position.x, (int)(GROUND_Y + 3), width * s, 10 * s, Fade(INK, 0.45f * s + 0.1f));
}

static Color FighterTint(const Game *g, int index) {
    bool mirror = index == 1 && g->fighters[0].character == g->fighters[1].character;
    if (!mirror)
        return WHITE;
    return g->fighters[1].character == CHAR_REN ? (Color){255, 214, 170, 255} : (Color){184, 196, 255, 255};
}

void DrawWorld(const Art *a, const Game *g, const Fx *fx, bool hitboxes, bool reducedMotion) {
    Camera2D cam = FxCamera(fx, density, reducedMotion);
    BeginMode2D(cam);
    float time = reducedMotion ? 0 : g->totalTime;
    StageDrawBack(a->backdrop, g->stage, fx->time, fx->camCenter.x, fx->lightning, reducedMotion);
    if (fx->superTime > 0) {
        float k = fminf(1, fx->superTime / SUPER_FREEZE * 1.6f);
        DrawRectangle(-200, -200, (int)ARENA_W + 400, (int)ARENA_H + 400, Fade((Color){6, 6, 18, 255}, 0.62f * k));
    }
    FxDrawWorld(fx, false);
    if (StageHasReflections(g->stage))
        for (int i = 0; i < 2; ++i)
            DrawFighterPose(a, &g->fighters[i], g, i, g->totalTime + i, 1.45f, Fade((Color){150, 170, 220, 255}, 0.32f),
                            true);
    for (int i = 0; i < 2; ++i)
        DrawShadow(&g->fighters[i]);
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < FX_GHOSTS; ++i) {
        const FxGhost *ghost = &fx->ghosts[i];
        if (ghost->life <= 0)
            continue;
        float k = ghost->life / ghost->maxLife;
        DrawFighterPose(a, &ghost->snapshot, NULL, ghost->index, g->totalTime, 1.45f, Fade(ghost->color, 0.45f * k),
                        false);
    }
    EndBlendMode();
    // Draw the aggressor last so attacks read on top.
    int order[2] = {0, 1};
    if (g->fighters[0].action != ACTION_IDLE && g->fighters[1].action == ACTION_IDLE)
        order[0] = 1, order[1] = 0;
    if (fx->superTime > 0)
        order[1] = fx->superOwner, order[0] = 1 - fx->superOwner;
    for (int k = 0; k < 2; ++k) {
        int i = order[k];
        const Fighter *f = &g->fighters[i];
        Color accent = FighterColor(g, i);
        if (f->energy >= SUPER_COST && f->health > 0 && !reducedMotion) {
            BeginBlendMode(BLEND_ADDITIVE);
            float pulse = 0.18f + sinf(time * 8) * 0.06f;
            Glow((int)f->position.x, (int)(f->position.y - 110), 150, Fade(accent, pulse), BLANK);
            EndBlendMode();
        }
        DrawFighterPose(a, f, g, i, g->totalTime + i, 1.45f, FighterTint(g, i), false);
        DrawAttackFx(f, g, accent, g->totalTime);
        if (f->blocking || f->blockstun > 0) {
            BeginBlendMode(BLEND_ADDITIVE);
            Vector2 c = {f->position.x + f->facing * 30, f->position.y - 125};
            float r = 86 + (f->blockstun > 0 ? 8 : 0);
            DrawRing(c, r - 4, r, f->facing > 0 ? -70 : 110, f->facing > 0 ? 70 : 250, 24,
                     Fade((Color){150, 214, 255, 255}, f->blockstun > 0 ? 0.9f : 0.45f));
            EndBlendMode();
        }
    }
    for (int i = 0; i < MAX_PROJECTILES; ++i)
        if (g->projectiles[i].active)
            DrawProjectile(&g->projectiles[i], FighterColor(g, g->projectiles[i].owner), g->totalTime);
    FxDrawWorld(fx, true);
    StageDrawFront(g->stage, fx->time, fx->camCenter.x, fx->lightning, reducedMotion);
    if (hitboxes)
        for (int i = 0; i < 2; ++i) {
            DrawRectangleLinesEx(FighterHurtbox(&g->fighters[i]), 2, LIME);
            if (FighterAttackActive(&g->fighters[i]) ||
                (g->fighters[i].action == ACTION_SUPER && g->fighters[i].character == CHAR_REN))
                DrawRectangleLinesEx(FighterAttackbox(&g->fighters[i]), 2, RED);
        }
    EndMode2D();
    BeginMode2D(RenderBaseCamera());
    // Vignette keeps the eye on the fighters.
    DrawRectangleGradientH(0, 0, 160, (int)ARENA_H, Fade(INK, 0.45f), BLANK);
    DrawRectangleGradientH((int)ARENA_W - 160, 0, 160, (int)ARENA_H, BLANK, Fade(INK, 0.45f));
}
