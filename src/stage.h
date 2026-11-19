#ifndef SPIRIT_STAGE_H
#define SPIRIT_STAGE_H
#include "combat.h"

// Stages draw in arena coordinates under the world camera. `camX` is the camera centre and
// drives parallax; `lightning` (0..1) brightens the Storm Terrace sky.
void StageDrawBack(Texture2D backdrop, StageId stage, float time, float camX, float lightning, bool reducedMotion);
void StageDrawFront(StageId stage, float time, float camX, float lightning, bool reducedMotion);
bool StageHasReflections(StageId stage);
#endif
