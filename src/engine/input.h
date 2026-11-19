#ifndef SPIRIT_INPUT_H
#define SPIRIT_INPUT_H
#include "combat.h"

// Device layer: keyboard and gamepads feed fighters and menus through one interface.
typedef struct {
    bool up, down, left, right, accept, back, pause, any;
} MenuInput;

#define INPUT_HISTORY 14
typedef struct {
    unsigned bits[INPUT_HISTORY];
    float hold[INPUT_HISTORY];
    int count;
} InputHistory;

enum {
    IN_LEFT = 1 << 0,
    IN_RIGHT = 1 << 1,
    IN_UP = 1 << 2,
    IN_GUARD = 1 << 3,
    IN_LIGHT = 1 << 4,
    IN_HEAVY = 1 << 5,
    IN_SPECIAL = 1 << 6,
    IN_DASH = 1 << 7,
    IN_SUPER = 1 << 8,
};

void InputUpdate(void);
bool InputKeyPressed(int key);
bool InputKeyDown(int key);
bool InputLoadScript(const char *path);
bool InputScripted(void);
void InputScriptTick(float now);
const char *InputScriptSnap(void);
bool InputScriptDone(void);
FighterInput InputFighter(int player);
MenuInput InputMenu(int player); // player < 0 reads every device
bool InputGamepad(int player);
void InputMerge(FighterInput *pending, FighterInput input);
void InputConsume(FighterInput *input);
void InputHistoryPush(InputHistory *history, FighterInput input, int facing, float dt);
#endif
