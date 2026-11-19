#include "audio.h"
#include "combat.h"
#include "engine/fx.h"
#include "engine/input.h"
#include "render.h"
#include "rlgl.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { SCENE_TITLE, SCENE_SELECT, SCENE_VERSUS, SCENE_FIGHT, SCENE_MOVES } Scene;

typedef struct {
    Scene scene, movesReturn;
    Game game;
    Fx fx;
    SelectState select;
    InputHistory history[2];
    FighterInput pending[2];
    GameAudio audio;
    int titleSelected, pauseSelection, resultSelection, difficulty;
    float titleCursor, accumulator, versusTime, resultTime, sceneTime;
    bool quit, debug, reducedMotion, showHistory;
    unsigned frame;
} App;

static bool Clicked(Vector2 mouse, Rectangle rect) {
    return CheckCollisionPointRec(mouse, rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
static int Navigate(int selected, int total, MenuInput m, GameAudio *audio) {
    int before = selected;
    if (m.down)
        selected = (selected + 1) % total;
    if (m.up)
        selected = (selected + total - 1) % total;
    if (selected != before)
        GameAudioSfx(audio, SFX_UI_MOVE);
    return selected;
}
static bool Screenshot(RenderTexture2D target, const char *path) {
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    // Blending leaves partial alpha in the canvas; the colour channels are already composited.
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    bool saved = ExportImage(image, path);
    UnloadImage(image);
    return saved;
}

static void Go(App *app, Scene scene) {
    app->scene = scene;
    app->sceneTime = 0;
}
static void OpenSelect(App *app, MatchMode mode) {
    SelectState *s = &app->select;
    s->mode = mode;
    s->locked[0] = s->locked[1] = false;
    s->cpuSide = false;
    s->step = 0;
    if (s->cursor[0] == s->cursor[1] && mode != MODE_LOCAL)
        s->cursor[1] = (s->cursor[0] + 1) % CHAR_COUNT;
    Go(app, SCENE_SELECT);
}
static void BeginMatch(App *app, MatchMode mode, uint32_t seed) {
    SelectState *s = &app->select;
    GameStartEx(&app->game, mode, app->difficulty, seed, s->cursor[0], s->cursor[1], (StageId)s->stage);
    FxReset(&app->fx);
    memset(app->pending, 0, sizeof(app->pending));
    memset(app->history, 0, sizeof(app->history));
    app->accumulator = 0;
    app->pauseSelection = app->resultSelection = 0;
    app->resultTime = 0;
}

// ---------------------------------------------------------------------------------------------
// Scene updates
// ---------------------------------------------------------------------------------------------
static void UpdateTitle(App *app, Vector2 mouse) {
    MenuInput m = InputMenu(-1);
    app->titleSelected = Navigate(app->titleSelected, TITLE_COUNT, m, &app->audio);
    if (m.left || m.right) {
        app->difficulty = (app->difficulty + (m.right ? 1 : 2)) % 3;
        GameAudioSfx(&app->audio, SFX_UI_MOVE);
    }
    bool accept = m.accept;
    for (int i = 0; i < TITLE_COUNT; ++i)
        if (Clicked(mouse, TitleItemRect(i))) {
            app->titleSelected = i;
            accept = true;
        }
    if (Clicked(mouse, (Rectangle){70, 650, 330, 40}))
        app->difficulty = (app->difficulty + 1) % 3;
    if (InputKeyPressed(KEY_Q))
        app->quit = true;
    if (!accept)
        return;
    GameAudioSfx(&app->audio, SFX_UI_SELECT);
    switch (app->titleSelected) {
    case TITLE_SOLO:
        OpenSelect(app, MODE_SOLO);
        break;
    case TITLE_VERSUS:
        OpenSelect(app, MODE_LOCAL);
        break;
    case TITLE_TRAINING:
        OpenSelect(app, MODE_TRAINING);
        break;
    case TITLE_MOVES:
        app->movesReturn = SCENE_TITLE;
        Go(app, SCENE_MOVES);
        break;
    default:
        app->quit = true;
    }
}

static void UpdateSelect(App *app) {
    SelectState *s = &app->select;
    float now = (float)GetTime();
    if (s->step == 1) {
        MenuInput m = InputMenu(-1);
        if (m.left || m.right) {
            s->stage = (s->stage + (m.right ? 1 : STAGE_COUNT - 1)) % STAGE_COUNT;
            GameAudioSfx(&app->audio, SFX_UI_MOVE);
        }
        if (m.accept) {
            GameAudioSfx(&app->audio, SFX_FIGHT);
            BeginMatch(app, s->mode, 80377u + app->frame);
            app->versusTime = 0;
            Go(app, SCENE_VERSUS);
            GameAudioMusic(&app->audio, MUSIC_BATTLE);
        } else if (m.back) {
            GameAudioSfx(&app->audio, SFX_UI_BACK);
            s->step = 0;
            s->locked[s->mode == MODE_LOCAL ? 1 : (s->cpuSide ? 1 : 0)] = false;
        }
        return;
    }
    if (s->mode == MODE_LOCAL) {
        for (int side = 0; side < 2; ++side) {
            MenuInput m = InputMenu(side);
            if (side == 0) {
                // Player one may also use Enter and Escape.
                m.accept |= InputKeyPressed(KEY_ENTER);
                m.back |= InputKeyPressed(KEY_ESCAPE);
            }
            if (!s->locked[side] && (m.left || m.right)) {
                s->cursor[side] = (s->cursor[side] + (m.right ? 1 : CHAR_COUNT - 1)) % CHAR_COUNT;
                GameAudioSfx(&app->audio, SFX_UI_MOVE);
            }
            if (m.accept && !s->locked[side]) {
                s->locked[side] = true;
                s->lockTime[side] = now;
                GameAudioSfx(&app->audio, SFX_UI_SELECT);
            } else if (m.back) {
                GameAudioSfx(&app->audio, SFX_UI_BACK);
                if (s->locked[side])
                    s->locked[side] = false;
                else if (side == 0)
                    Go(app, SCENE_TITLE);
            }
        }
        if (s->locked[0] && s->locked[1])
            s->step = 1;
        return;
    }
    int side = s->cpuSide ? 1 : 0;
    MenuInput m = InputMenu(-1);
    if (m.left || m.right) {
        s->cursor[side] = (s->cursor[side] + (m.right ? 1 : CHAR_COUNT - 1)) % CHAR_COUNT;
        GameAudioSfx(&app->audio, SFX_UI_MOVE);
    }
    if (m.accept) {
        s->locked[side] = true;
        s->lockTime[side] = now;
        GameAudioSfx(&app->audio, SFX_UI_SELECT);
        if (side == 0)
            s->cpuSide = true;
        else
            s->step = 1;
    } else if (m.back) {
        GameAudioSfx(&app->audio, SFX_UI_BACK);
        if (s->cpuSide) {
            s->cpuSide = false;
            s->locked[0] = false;
        } else
            Go(app, SCENE_TITLE);
    }
}

static void StepMatch(App *app, float dt) {
    Game *g = &app->game;
    InputMerge(&app->pending[0], InputFighter(0));
    InputMerge(&app->pending[1], InputFighter(1));
    InputHistoryPush(&app->history[0], app->pending[0], g->fighters[0].facing, dt);
    app->accumulator += dt * FxTimeScale(&app->fx);
    while (app->accumulator >= FIXED_DT) {
        GameUpdate(g, app->pending[0], app->pending[1], FIXED_DT);
        FxEvents(&app->fx, g, g->events, g->eventCount);
        GameAudioPlay(&app->audio, g->events, g->eventCount);
        InputConsume(&app->pending[0]);
        InputConsume(&app->pending[1]);
        app->accumulator -= FIXED_DT;
    }
}

static void UpdateFight(App *app, Vector2 mouse, float dt, bool capture, bool demo) {
    Game *g = &app->game;
    MenuInput m = InputMenu(-1);
    if (!capture && !demo && g->phase != MATCH_END &&
        (m.pause || (!IsWindowFocused() && !g->paused && !InputScripted()) || (g->paused && m.back && !m.pause))) {
        g->paused = !g->paused;
        app->pauseSelection = 0;
        app->accumulator = 0;
        memset(app->pending, 0, sizeof(app->pending));
        GameAudioSfx(&app->audio, g->paused ? SFX_UI_SELECT : SFX_UI_BACK);
        return;
    }
    if (g->paused) {
        app->pauseSelection = Navigate(app->pauseSelection, PAUSE_COUNT, m, &app->audio);
        bool choose = m.accept;
        for (int i = 0; i < PAUSE_COUNT; ++i)
            if (Clicked(mouse, PauseItemRect(i))) {
                app->pauseSelection = i;
                choose = true;
            }
        if (!choose)
            return;
        GameAudioSfx(&app->audio, SFX_UI_SELECT);
        switch (app->pauseSelection) {
        case PAUSE_RESUME:
            g->paused = false;
            break;
        case PAUSE_MOVES:
            app->movesReturn = SCENE_FIGHT;
            Go(app, SCENE_MOVES);
            break;
        case PAUSE_RESTART:
            BeginMatch(app, g->mode, 80377u + app->frame);
            break;
        case PAUSE_SELECT:
            OpenSelect(app, g->mode);
            GameAudioMusic(&app->audio, MUSIC_MENU);
            break;
        default:
            Go(app, SCENE_TITLE);
            GameAudioMusic(&app->audio, MUSIC_MENU);
        }
        return;
    }
    if (g->mode == MODE_TRAINING) {
        if (InputKeyPressed(KEY_T))
            g->dummy = (DummyMode)((g->dummy + 1) % DUMMY_COUNT);
        if (InputKeyPressed(KEY_R) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_LEFT))) {
            GameTrainingReset(g);
            FxReset(&app->fx);
        }
    }
    if (g->phase == MATCH_END) {
        app->resultTime += dt;
        FxUpdate(&app->fx, g, dt, app->reducedMotion);
        if (demo) {
            if (app->resultTime > 4)
                BeginMatch(app, MODE_DEMO, 80377u + app->frame);
            return;
        }
        if (app->resultTime < 0.6f)
            return;
        app->resultSelection = Navigate(app->resultSelection, RESULT_COUNT, m, &app->audio);
        bool choose = m.accept;
        for (int i = 0; i < RESULT_COUNT; ++i)
            if (Clicked(mouse, ResultItemRect(i))) {
                app->resultSelection = i;
                choose = true;
            }
        if (!choose)
            return;
        GameAudioSfx(&app->audio, SFX_UI_SELECT);
        if (app->resultSelection == RESULT_REMATCH) {
            BeginMatch(app, g->mode, 80377u + app->frame);
            app->versusTime = 0;
            Go(app, SCENE_VERSUS);
        } else if (app->resultSelection == RESULT_SELECT) {
            OpenSelect(app, g->mode);
            GameAudioMusic(&app->audio, MUSIC_MENU);
        } else {
            Go(app, SCENE_TITLE);
            GameAudioMusic(&app->audio, MUSIC_MENU);
        }
        return;
    }
    StepMatch(app, dt);
    FxUpdate(&app->fx, g, dt, app->reducedMotion);
    if (app->fx.thunder)
        GameAudioSfx(&app->audio, SFX_THUNDER);
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------
static void DrawMatchScene(App *app, const Art *art) {
    Game *g = &app->game;
    DrawWorld(art, g, &app->fx, app->debug, app->reducedMotion);
    FxDrawScreen(&app->fx, g, app->reducedMotion);
    DrawHud(art, g, &app->fx, app->history, app->showHistory);
    DrawPopups(art, &app->fx, app->reducedMotion);
    DrawAnnouncer(art, g, &app->fx);
    if (g->paused && app->scene == SCENE_FIGHT)
        DrawPause(art, g, app->pauseSelection, app->audio.muted, app->reducedMotion);
    if (g->phase == MATCH_END)
        DrawResults(art, g, app->resultSelection, app->resultTime);
}
static void DrawScene(App *app, const Art *art) {
    switch (app->scene) {
    case SCENE_TITLE:
        DrawTitle(art, app->reducedMotion ? 0 : app->sceneTime + 3, app->titleSelected, app->titleCursor, app->difficulty,
                  app->audio.muted);
        break;
    case SCENE_SELECT:
        DrawSelect(art, &app->select, (float)GetTime(), app->difficulty);
        break;
    case SCENE_VERSUS:
        DrawVersus(art, &app->game, app->versusTime);
        break;
    case SCENE_FIGHT:
        DrawMatchScene(app, art);
        break;
    case SCENE_MOVES:
        if (app->movesReturn == SCENE_FIGHT)
            DrawMatchScene(app, art);
        else
            DrawTitle(art, app->sceneTime, app->titleSelected, app->titleCursor, app->difficulty, app->audio.muted);
        DrawMoveList(art, app->sceneTime);
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Deterministic capture fixtures for screenshots and visual checks.
// ---------------------------------------------------------------------------------------------
static void CaptureFixture(App *app, const char *scene, int frame, int frames) {
    Game *g = &app->game;
    if (strcmp(scene, "super") == 0 && frame == 600) {
        g->fighters[0].energy = MAX_ENERGY;
        g->fighters[0].action = ACTION_IDLE;
        g->fighters[0].stun = 0;
        g->fighters[0].launched = false;
        app->pending[0].super = true;
    }
    if (frame < frames - 1)
        return;
    if (strcmp(scene, "pause") == 0)
        g->paused = true;
    if (strcmp(scene, "results") == 0) {
        g->phase = MATCH_END;
        g->winner = 0;
        g->fighters[0].wins = 2;
        g->fighters[1].wins = 1;
        app->resultTime = 2;
    }
    if (strcmp(scene, "hurt") == 0) {
        g->phase = ROUND_FIGHT;
        for (int i = 0; i < 2; ++i) {
            Fighter *f = &g->fighters[i];
            f->position = (Vector2){i == 0 ? 430.0f : 850.0f, GROUND_Y};
            f->facing = i == 0 ? 1 : -1;
            f->grounded = true;
            f->blocking = false;
            f->launched = false;
            f->knockdown = 0;
            f->action = ACTION_IDLE;
            f->hurtDuration = .3f;
            f->stun = i == 0 ? .22f : .1f;
            f->hurtDirection = -f->facing;
        }
    }
}

int main(int argc, char **argv) {
    const char *capture = NULL;
    const char *scene = "menu";
    int frames = 1;
    bool demo = false;
    int stage = STAGE_SANCTUARY;
    const char *script = NULL, *snapDir = ".";
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc)
            capture = argv[++i];
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc)
            scene = argv[++i];
        else if (strcmp(argv[i], "--stage") == 0 && i + 1 < argc)
            stage = strcmp(argv[++i], "storm") == 0 ? STAGE_STORM_TERRACE : STAGE_SANCTUARY;
        else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc)
            script = argv[++i];
        else if (strcmp(argv[i], "--snap-dir") == 0 && i + 1 < argc)
            snapDir = argv[++i];
        else if (strcmp(argv[i], "--demo") == 0)
            demo = true;
        else if (strcmp(argv[i], "--help") == 0) {
            puts("Spirit Clash | --demo [--stage sanctuary|storm] | --capture FILE [--scene "
                 "menu|select|versus|fight|super|hurt|pause|results|moves] [--stage sanctuary|storm] [--frames N]");
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }
    if (frames < 1 || frames > 36000) {
        fputs("Frames must be between 1 and 36000.\n", stderr);
        return 1;
    }
    const char *scenes[] = {"menu", "select", "versus", "fight", "super", "hurt", "pause", "results", "moves"};
    bool known = false;
    for (int i = 0; i < 9; ++i)
        known |= strcmp(scene, scenes[i]) == 0;
    if (!known) {
        fputs("Unknown scene. Use menu, select, versus, fight, super, hurt, pause, results or moves.\n", stderr);
        return 1;
    }
    if (script && !InputLoadScript(script)) {
        fprintf(stderr, "Could not read script %s\n", script);
        return 1;
    }
    bool headless = capture || script;
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | (headless ? FLAG_WINDOW_HIDDEN : 0));
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(headless ? 1280 : 1120, headless ? 720 : 630, "Spirit Clash | Raylib Fighter");
    if (!IsWindowReady())
        return 1;
    SetWindowMinSize(768, 432);
    SetExitKey(KEY_NULL);
    if (!capture)
        SetTargetFPS(120);
    Art art;
    ArtLoad(&art);
    RenderTexture2D canvas = LoadRenderTexture((int)ARENA_W, (int)ARENA_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    static App app;
    memset(&app, 0, sizeof(app));
    app.difficulty = 1;
    app.showHistory = true;
    app.select.cursor[1] = CHAR_KAE;
    app.select.stage = stage;
    app.fx.seed = 99;
    if (!capture) {
        GameAudioInit(&app.audio);
        if (script)
            GameAudioMute(&app.audio); // play-tests exercise the audio engine silently
    }
    // Demo and gameplay captures run a real seeded match.
    bool matchScene = strcmp(scene, "menu") && strcmp(scene, "select") && strcmp(scene, "moves");
    if (demo || matchScene) {
        MatchMode mode = strcmp(scene, "super") == 0 ? MODE_SOLO : MODE_DEMO;
        BeginMatch(&app, mode, 80377);
        app.game.difficulty = 2;
        Go(&app, strcmp(scene, "versus") == 0 ? SCENE_VERSUS : SCENE_FIGHT);
        GameAudioMusic(&app.audio, MUSIC_BATTLE);
    } else {
        Go(&app, strcmp(scene, "moves") == 0 ? SCENE_MOVES : strcmp(scene, "select") == 0 ? SCENE_SELECT : SCENE_TITLE);
        if (app.scene == SCENE_SELECT) {
            app.select.mode = MODE_SOLO;
            app.select.locked[0] = true;
            app.select.cpuSide = true;
            app.select.lockTime[0] = -10;
        }
        GameAudioMusic(&app.audio, MUSIC_MENU);
    }
    int exitStatus = 0;
    while (!WindowShouldClose() && !app.quit) {
        float dt = headless ? 1.0f / 60 : fminf(GetFrameTime(), .1f);
        app.sceneTime += dt;
        InputScriptTick(app.frame / 60.0f);
        InputUpdate();
        if (InputScriptDone())
            app.quit = true;
        float scale = fminf((float)GetScreenWidth() / ARENA_W, (float)GetScreenHeight() / ARENA_H);
        Vector2 offset = {(GetScreenWidth() - ARENA_W * scale) / 2, (GetScreenHeight() - ARENA_H * scale) / 2};
        Vector2 mouse = {(GetMouseX() - offset.x) / scale, (GetMouseY() - offset.y) / scale};
        if (InputKeyPressed(KEY_M))
            GameAudioMute(&app.audio);
        if (InputKeyPressed(KEY_F10))
            ToggleBorderlessWindowed();
        if (InputKeyPressed(KEY_V))
            app.reducedMotion = !app.reducedMotion;
        if (InputKeyPressed(KEY_H))
            app.debug = !app.debug;
        if (capture)
            CaptureFixture(&app, scene, (int)app.frame, frames);
        switch (app.scene) {
        case SCENE_TITLE:
            if (!capture)
                UpdateTitle(&app, mouse);
            app.titleCursor += (app.titleSelected - app.titleCursor) * (1 - expf(-dt * 18));
            break;
        case SCENE_SELECT:
            if (!capture)
                UpdateSelect(&app);
            break;
        case SCENE_VERSUS:
            app.versusTime += dt;
            if (capture && app.versusTime > 1.4f)
                app.versusTime = 1.4f;
            if (!capture && (app.versusTime > 2.6f || (app.versusTime > 0.6f && InputMenu(-1).accept)))
                Go(&app, SCENE_FIGHT);
            break;
        case SCENE_FIGHT:
            UpdateFight(&app, mouse, dt, capture != NULL, demo || app.game.mode == MODE_DEMO);
            break;
        case SCENE_MOVES: {
            MenuInput m = InputMenu(-1);
            if (!capture && (m.back || m.accept || m.pause)) {
                GameAudioSfx(&app.audio, SFX_UI_BACK);
                Go(&app, app.movesReturn);
            }
            break;
        }
        }
        // Music ducks under pauses, results and Spirit Art freezes.
        float duck = 1;
        if (app.scene == SCENE_FIGHT || app.scene == SCENE_MOVES) {
            if (app.game.paused || app.scene == SCENE_MOVES)
                duck = 0.35f;
            else if (app.game.phase == MATCH_END)
                duck = 0.5f;
            else if (app.game.superFreeze > 0 || app.fx.slowmo > 0)
                duck = 0.3f;
        }
        GameAudioDuck(&app.audio, duck);

        // Keep simulation and input in logical coordinates, but rasterize at physical display pixels.
        float density = capture ? 1.0f : fminf((float)GetRenderWidth() / ARENA_W, (float)GetRenderHeight() / ARENA_H);
        int targetWidth = (int)fmaxf(1, roundf(ARENA_W * density));
        int targetHeight = (int)fmaxf(1, roundf(ARENA_H * density));
        if (targetWidth != canvas.texture.width || targetHeight != canvas.texture.height) {
            RenderTexture2D resized = LoadRenderTexture(targetWidth, targetHeight);
            if (resized.id) {
                UnloadRenderTexture(canvas);
                canvas = resized;
                SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
            }
        }
        RenderSetDensity((float)canvas.texture.width / ARENA_W);
        if (getenv("SPIRIT_RENDER_STATS") && (app.frame == 0 || IsWindowResized())) {
            printf("render: window=%dx%d framebuffer=%dx%d canvas=%dx%d\n", GetScreenWidth(), GetScreenHeight(),
                   GetRenderWidth(), GetRenderHeight(), canvas.texture.width, canvas.texture.height);
            fflush(stdout);
        }
        BeginTextureMode(canvas);
        ClearBackground((Color){15, 15, 39, 255});
        BeginMode2D(RenderBaseCamera());
        DrawScene(&app, &art);
        EndMode2D();
        EndTextureMode();
        const char *snap = InputScriptSnap();
        if (snap) {
            char path[2048];
            snprintf(path, sizeof(path), "%s/%s.png", snapDir, snap);
            printf("snap %s scene=%d phase=%d p1=%.0f p2=%.0f combo=%d/%d\n", path, app.scene, app.game.phase,
                   app.game.fighters[0].health, app.game.fighters[1].health, app.game.fighters[0].maxCombo,
                   app.game.fighters[1].maxCombo);
            fflush(stdout);
            Screenshot(canvas, path);
        }
        if (!capture && InputKeyPressed(KEY_F9)) {
            char folder[2048], filename[2304], stamp[64];
            time_t now = time(NULL);
            strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", localtime(&now));
            snprintf(folder, sizeof(folder), "%sscreenshots", GetApplicationDirectory());
            if (!DirectoryExists(folder))
                MakeDirectory(folder);
            snprintf(filename, sizeof(filename), "%s/spirit-%s-%u.png", folder, stamp, app.frame);
            Screenshot(canvas, filename);
        }
        BeginDrawing();
        ClearBackground((Color){9, 10, 20, 255});
        rlDrawRenderBatchActive();
        rlDisableColorBlend(); // present the composited canvas as opaque pixels
        DrawTexturePro(canvas.texture, (Rectangle){0, 0, (float)canvas.texture.width, -(float)canvas.texture.height},
                       (Rectangle){offset.x, offset.y, ARENA_W * scale, ARENA_H * scale}, (Vector2){0}, 0, WHITE);
        rlDrawRenderBatchActive();
        rlEnableColorBlend();
        EndDrawing();
        app.frame++;
        if (capture && (int)app.frame >= frames) {
            exitStatus = Screenshot(canvas, capture) ? 0 : 1;
            break;
        }
    }
    GameAudioClose(&app.audio);
    UnloadRenderTexture(canvas);
    ArtUnload(&art);
    CloseWindow();
    return exitStatus;
}
