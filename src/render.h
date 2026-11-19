#ifndef SPIRIT_RENDER_H
#define SPIRIT_RENDER_H
#include "combat.h"
#include "engine/fx.h"

typedef struct {
    Texture2D backdrop, fighters, walks, hurt;
    Font display, body;
} Art;

static const Color INK = {15, 15, 39, 255};
static const Color PAPER = {244, 241, 255, 255};
static const Color MUTED = {172, 175, 206, 255};
static const Color CYAN = {117, 229, 255, 255};
static const Color ROSE = {255, 153, 172, 255};
static const Color SPIRIT_GOLD = {255, 221, 157, 255};
static const Color DANGER = {255, 84, 104, 255};

// Soft radial light; kept on integer-free arena coordinates.
static inline void Glow(float x, float y, float radius, Color inner, Color outer) {
    DrawCircleGradient((Vector2){x, y}, radius, inner, outer);
}

void ArtLoad(Art *art);
void ArtUnload(Art *art);

// The canvas is rasterised at physical pixel density; every camera multiplies by it.
void RenderSetDensity(float density);
float RenderDensity(void);
Camera2D RenderBaseCamera(void);

void DrawFighterPose(const Art *art, const Fighter *fighter, const Game *game, int index, float time, float scale,
                     Color tint, bool mirrored);
void DrawPortrait(const Art *art, int character, Rectangle dest, bool flip, Color tint);
void DrawCharacterShowcase(const Art *art, int character, Vector2 feet, float scale, int facing, int pose, Color tint);
void DrawStagePreview(const Art *art, StageId stage, Rectangle dest, float time);
void DrawWorld(const Art *art, const Game *game, const Fx *fx, bool hitboxes, bool reducedMotion);

void TextLabel(Font font, const char *text, Vector2 position, float size, Color color);
void CenterLabel(Font font, const char *text, float y, float size, Color color);
void CenterLabelAt(Font font, const char *text, float x, float y, float size, Color color);
void RightLabel(Font font, const char *text, float right, float y, float size, Color color);
Color Shade(Color color, float factor);
Color MixColor(Color a, Color b, float t);
#endif
