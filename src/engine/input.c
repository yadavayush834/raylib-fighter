#include "input.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------------------------
// Input replay: a timed script drives a virtual keyboard so the real game can be played
// end-to-end without a display server or a human (used for automated play-tests).
// ---------------------------------------------------------------------------------------------
#define SCRIPT_MAX 2048
typedef struct {
    float time;
    int kind; // 0 down, 1 up, 2 tap, 3 snap, 4 quit
    int key, order;
    char name[48];
} ScriptEvent;
static ScriptEvent script[SCRIPT_MAX];
static int scriptCount, scriptNext;
static bool scripted;
static bool virtualDown[512], virtualPressed[512], releaseNext[512];
static char pendingSnap[48];
static bool quitRequested;

static int KeyFromName(const char *n) {
    if (strlen(n) == 1 && n[0] >= 'A' && n[0] <= 'Z')
        return KEY_A + (n[0] - 'A');
    struct {
        const char *name;
        int key;
    } table[] = {{"SPACE", KEY_SPACE},   {"ENTER", KEY_ENTER},         {"ESCAPE", KEY_ESCAPE}, {"UP", KEY_UP},
                 {"DOWN", KEY_DOWN},     {"LEFT", KEY_LEFT},           {"RIGHT", KEY_RIGHT},   {"COMMA", KEY_COMMA},
                 {"PERIOD", KEY_PERIOD}, {"SLASH", KEY_SLASH},         {"SEMICOLON", KEY_SEMICOLON},
                 {"RSHIFT", KEY_RIGHT_SHIFT}, {"BACKSPACE", KEY_BACKSPACE}};
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
        if (strcmp(n, table[i].name) == 0)
            return table[i].key;
    return -1;
}
static int CompareEvents(const void *a, const void *b) {
    const ScriptEvent *x = a, *y = b;
    if (x->time != y->time)
        return x->time < y->time ? -1 : 1;
    return x->order - y->order; // stable for events that share a timestamp
}
bool InputLoadScript(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    char line[256];
    float clock = 0;
    while (fgets(line, sizeof(line), file) && scriptCount < SCRIPT_MAX) {
        char verb[32] = {0}, arg[48] = {0};
        float t;
        if (line[0] == '#' || sscanf(line, "%f %31s %47s", &t, verb, arg) < 2)
            continue;
        if (line[0] == '+')
            t = clock + t; // "+0.2 tap J" is relative to the previous line
        clock = t;
        ScriptEvent e = {.time = t, .key = -1};
        snprintf(e.name, sizeof(e.name), "%s", arg);
        if (strcmp(verb, "down") == 0)
            e.kind = 0;
        else if (strcmp(verb, "up") == 0)
            e.kind = 1;
        else if (strcmp(verb, "tap") == 0)
            e.kind = 2;
        else if (strcmp(verb, "snap") == 0)
            e.kind = 3;
        else if (strcmp(verb, "quit") == 0)
            e.kind = 4;
        else
            continue;
        if (e.kind <= 2 && (e.key = KeyFromName(arg)) < 0) {
            fprintf(stderr, "script: unknown key %s\n", arg);
            continue;
        }
        e.order = scriptCount;
        script[scriptCount++] = e;
    }
    fclose(file);
    qsort(script, (size_t)scriptCount, sizeof(script[0]), CompareEvents);
    scripted = true;
    return scriptCount > 0;
}
bool InputScripted(void) {
    return scripted;
}
void InputScriptTick(float now) {
    if (!scripted)
        return;
    for (int k = 0; k < 512; ++k) {
        virtualPressed[k] = false;
        if (releaseNext[k])
            virtualDown[k] = releaseNext[k] = false;
    }
    while (scriptNext < scriptCount && script[scriptNext].time <= now) {
        ScriptEvent *e = &script[scriptNext++];
        if (e->kind == 0 || e->kind == 2) {
            virtualPressed[e->key] = !virtualDown[e->key];
            virtualDown[e->key] = true;
            releaseNext[e->key] = e->kind == 2;
        } else if (e->kind == 1)
            virtualDown[e->key] = false;
        else if (e->kind == 3)
            snprintf(pendingSnap, sizeof(pendingSnap), "%s", e->name);
        else
            quitRequested = true;
    }
}
const char *InputScriptSnap(void) {
    static char name[48];
    if (!pendingSnap[0])
        return NULL;
    snprintf(name, sizeof(name), "%s", pendingSnap);
    pendingSnap[0] = 0;
    return name;
}
bool InputScriptDone(void) {
    return quitRequested;
}
bool InputKeyPressed(int key) {
    if (scripted)
        return key >= 0 && key < 512 && virtualPressed[key];
    return IsKeyPressed(key);
}
bool InputKeyDown(int key) {
    if (scripted)
        return key >= 0 && key < 512 && virtualDown[key];
    return IsKeyDown(key);
}

static bool stickWasDown[4][4];
static bool stickDown[4][4];

static float AxisValue(int pad, int axis) {
    if (!IsGamepadAvailable(pad))
        return 0;
    float v = GetGamepadAxisMovement(pad, axis);
    return fabsf(v) < 0.35f ? 0 : v;
}
static bool PadDown(int pad, int button) {
    return IsGamepadAvailable(pad) && IsGamepadButtonDown(pad, button);
}
static bool PadPressed(int pad, int button) {
    return IsGamepadAvailable(pad) && IsGamepadButtonPressed(pad, button);
}

void InputUpdate(void) {
    for (int pad = 0; pad < 4; ++pad) {
        float x = AxisValue(pad, GAMEPAD_AXIS_LEFT_X), y = AxisValue(pad, GAMEPAD_AXIS_LEFT_Y);
        bool now[4] = {y < -0.5f, y > 0.5f, x < -0.5f, x > 0.5f};
        for (int d = 0; d < 4; ++d) {
            stickWasDown[pad][d] = stickDown[pad][d];
            stickDown[pad][d] = now[d];
        }
    }
}
static bool StickPressed(int pad, int direction) {
    return stickDown[pad][direction] && !stickWasDown[pad][direction];
}

bool InputGamepad(int player) {
    return IsGamepadAvailable(player);
}

FighterInput InputFighter(int player) {
    FighterInput in;
    if (player == 0)
        in = (FighterInput){
            .move = (float)InputKeyDown(KEY_D) - (float)InputKeyDown(KEY_A),
            .jump = InputKeyPressed(KEY_W),
            .light = InputKeyPressed(KEY_J),
            .heavy = InputKeyPressed(KEY_K),
            .special = InputKeyPressed(KEY_L),
            .super = InputKeyPressed(KEY_I),
            .block = InputKeyDown(KEY_S),
            .dash = InputKeyPressed(KEY_SPACE),
        };
    else
        in = (FighterInput){
            .move = (float)InputKeyDown(KEY_RIGHT) - (float)InputKeyDown(KEY_LEFT),
            .jump = InputKeyPressed(KEY_UP),
            .light = InputKeyPressed(KEY_COMMA) || InputKeyPressed(KEY_KP_1),
            .heavy = InputKeyPressed(KEY_PERIOD) || InputKeyPressed(KEY_KP_2),
            .special = InputKeyPressed(KEY_SLASH) || InputKeyPressed(KEY_KP_3),
            .super = InputKeyPressed(KEY_SEMICOLON) || InputKeyPressed(KEY_KP_4),
            .block = InputKeyDown(KEY_DOWN),
            .dash = InputKeyPressed(KEY_RIGHT_SHIFT) || InputKeyPressed(KEY_KP_0),
        };
    // Gamepad N drives player N. Face buttons follow a classic arcade layout.
    int pad = player;
    if (IsGamepadAvailable(pad)) {
        float x = AxisValue(pad, GAMEPAD_AXIS_LEFT_X);
        float y = AxisValue(pad, GAMEPAD_AXIS_LEFT_Y);
        float dpad = (float)PadDown(pad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) - (float)PadDown(pad, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        if (in.move == 0)
            in.move = dpad != 0 ? dpad : (x > 0.4f ? 1.0f : x < -0.4f ? -1.0f : 0.0f);
        in.jump |= PadPressed(pad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || PadPressed(pad, GAMEPAD_BUTTON_LEFT_FACE_UP) ||
                   StickPressed(pad, 0);
        in.light |= PadPressed(pad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        in.heavy |= PadPressed(pad, GAMEPAD_BUTTON_RIGHT_FACE_UP);
        in.special |= PadPressed(pad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        in.dash |= PadPressed(pad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
        in.super |= PadPressed(pad, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
        in.block |= PadDown(pad, GAMEPAD_BUTTON_LEFT_TRIGGER_1) || PadDown(pad, GAMEPAD_BUTTON_LEFT_FACE_DOWN) ||
                    y > 0.6f;
    }
    // Heavy + special together also triggers the Spirit Art.
    if (in.heavy && in.special) {
        in.super = true;
        in.heavy = in.special = false;
    }
    return in;
}

MenuInput InputMenu(int player) {
    MenuInput m = {0};
    bool p1 = player <= 0, p2 = player < 0 || player == 1;
    if (p1) {
        m.up |= InputKeyPressed(KEY_W);
        m.down |= InputKeyPressed(KEY_S);
        m.left |= InputKeyPressed(KEY_A);
        m.right |= InputKeyPressed(KEY_D);
        m.accept |= InputKeyPressed(KEY_J) || InputKeyPressed(KEY_SPACE);
        m.back |= InputKeyPressed(KEY_K);
    }
    if (p2) {
        m.up |= InputKeyPressed(KEY_UP);
        m.down |= InputKeyPressed(KEY_DOWN);
        m.left |= InputKeyPressed(KEY_LEFT);
        m.right |= InputKeyPressed(KEY_RIGHT);
        m.accept |= InputKeyPressed(KEY_COMMA) || InputKeyPressed(KEY_KP_1);
        m.back |= InputKeyPressed(KEY_PERIOD) || InputKeyPressed(KEY_KP_2);
    }
    if (player < 0) {
        m.accept |= InputKeyPressed(KEY_ENTER) || InputKeyPressed(KEY_KP_ENTER);
        m.back |= InputKeyPressed(KEY_ESCAPE) || InputKeyPressed(KEY_BACKSPACE);
    } else if (player == 0) {
        m.accept |= InputKeyPressed(KEY_ENTER);
        m.back |= InputKeyPressed(KEY_ESCAPE);
    }
    m.pause = InputKeyPressed(KEY_ESCAPE);
    for (int pad = 0; pad < 4; ++pad) {
        if ((player >= 0 && pad != player) || !IsGamepadAvailable(pad))
            continue;
        m.up |= PadPressed(pad, GAMEPAD_BUTTON_LEFT_FACE_UP) || StickPressed(pad, 0);
        m.down |= PadPressed(pad, GAMEPAD_BUTTON_LEFT_FACE_DOWN) || StickPressed(pad, 1);
        m.left |= PadPressed(pad, GAMEPAD_BUTTON_LEFT_FACE_LEFT) || StickPressed(pad, 2);
        m.right |= PadPressed(pad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || StickPressed(pad, 3);
        m.accept |= PadPressed(pad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || PadPressed(pad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        m.back |= PadPressed(pad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        m.pause |= PadPressed(pad, GAMEPAD_BUTTON_MIDDLE_RIGHT);
    }
    m.any = m.up || m.down || m.left || m.right || m.accept || m.back || m.pause;
    return m;
}

void InputMerge(FighterInput *pending, FighterInput input) {
    pending->move = input.move;
    pending->block = input.block;
    pending->jump |= input.jump;
    pending->light |= input.light;
    pending->heavy |= input.heavy;
    pending->special |= input.special;
    pending->dash |= input.dash;
    pending->super |= input.super;
}
void InputConsume(FighterInput *input) {
    input->jump = input->light = input->heavy = input->special = input->dash = input->super = false;
}

void InputHistoryPush(InputHistory *h, FighterInput in, int facing, float dt) {
    (void)facing;
    unsigned bits = 0;
    if (in.move < 0)
        bits |= IN_LEFT;
    if (in.move > 0)
        bits |= IN_RIGHT;
    if (in.jump)
        bits |= IN_UP;
    if (in.block)
        bits |= IN_GUARD;
    if (in.light)
        bits |= IN_LIGHT;
    if (in.heavy)
        bits |= IN_HEAVY;
    if (in.special)
        bits |= IN_SPECIAL;
    if (in.dash)
        bits |= IN_DASH;
    if (in.super)
        bits |= IN_SUPER;
    if (h->count > 0 && h->bits[0] == bits) {
        h->hold[0] += dt;
        return;
    }
    memmove(&h->bits[1], &h->bits[0], sizeof(h->bits[0]) * (INPUT_HISTORY - 1));
    memmove(&h->hold[1], &h->hold[0], sizeof(h->hold[0]) * (INPUT_HISTORY - 1));
    h->bits[0] = bits;
    h->hold[0] = 0;
    if (h->count < INPUT_HISTORY)
        h->count++;
}
