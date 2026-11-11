#ifndef SPIRIT_RENDER_H
#define SPIRIT_RENDER_H
#include "combat.h"
typedef struct {
    Texture2D backdrop, fighters, walks;
    Font display, body;
} Art;
void ArtLoad(Art *art);
void ArtUnload(Art *art);
void DrawArena(const Art *art, float time, bool darken);
void DrawFighter(const Art *art, const Fighter *fighter, int index, float time, float scale);
void DrawMatch(const Art *art, const Game *game, bool hitboxes, bool reducedMotion);
void DrawMenu(const Art *art, float time, int selected, int difficulty, bool muted);
void DrawPause(const Art *art, int selected, bool muted, bool reducedMotion);
void DrawResults(const Art *art, const Game *game, int selected);
void TextLabel(Font font, const char *text, Vector2 position, float size, Color color);
void CenterLabel(Font font, const char *text, float y, float size, Color color);
#endif
