#ifndef SPIRIT_UI_H
#define SPIRIT_UI_H
#include "engine/input.h"
#include "render.h"

typedef enum { TITLE_SOLO, TITLE_VERSUS, TITLE_TRAINING, TITLE_MOVES, TITLE_QUIT, TITLE_COUNT } TitleItem;
typedef enum { PAUSE_RESUME, PAUSE_MOVES, PAUSE_RESTART, PAUSE_SELECT, PAUSE_TITLE, PAUSE_COUNT } PauseItem;
typedef enum { RESULT_REMATCH, RESULT_SELECT, RESULT_TITLE, RESULT_COUNT } ResultItem;

typedef struct {
    MatchMode mode;
    int cursor[2];
    bool locked[2];
    float lockTime[2];
    int stage;
    int step; // 0 fighters, 1 stage
    bool cpuSide; // solo/training: player one is now choosing the opponent
} SelectState;

Rectangle TitleItemRect(int item);
Rectangle PauseItemRect(int item);
Rectangle ResultItemRect(int item);

void DrawTitle(const Art *art, float time, int selected, float cursor, int difficulty, bool muted);
void DrawSelect(const Art *art, const SelectState *select, float time, int difficulty);
void DrawVersus(const Art *art, const Game *game, float t);
void DrawHud(const Art *art, const Game *game, const Fx *fx, const InputHistory *history, bool showHistory);
void DrawAnnouncer(const Art *art, const Game *game, const Fx *fx);
void DrawPopups(const Art *art, const Fx *fx, bool reducedMotion);
void DrawPause(const Art *art, const Game *game, int selected, bool muted, bool reducedMotion);
void DrawMoveList(const Art *art, float time);
void DrawResults(const Art *art, const Game *game, int selected, float t);
#endif
