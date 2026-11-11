#include "combat.h"
#include "render.h"
#include "audio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void MergeInput(FighterInput *pending, FighterInput input) {
    pending->move = input.move;
    pending->block = input.block;
    pending->jump |= input.jump;
    pending->light |= input.light;
    pending->heavy |= input.heavy;
    pending->special |= input.special;
    pending->dash |= input.dash;
}
static void ConsumeButtons(FighterInput *input) {
    input->jump = input->light = input->heavy = input->special = input->dash = false;
}
static FighterInput ReadPlayer(int player) {
    if (player == 0)
        return (FighterInput){
            .move = (float)IsKeyDown(KEY_D) - (float)IsKeyDown(KEY_A),
            .jump = IsKeyPressed(KEY_W),
            .light = IsKeyPressed(KEY_J),
            .heavy = IsKeyPressed(KEY_K),
            .special = IsKeyPressed(KEY_L),
            .block = IsKeyDown(KEY_S),
            .dash = IsKeyPressed(KEY_SPACE),
        };
    return (FighterInput){
        .move = (float)IsKeyDown(KEY_RIGHT) - (float)IsKeyDown(KEY_LEFT),
        .jump = IsKeyPressed(KEY_UP),
        .light = IsKeyPressed(KEY_COMMA) || IsKeyPressed(KEY_KP_1),
        .heavy = IsKeyPressed(KEY_PERIOD) || IsKeyPressed(KEY_KP_2),
        .special = IsKeyPressed(KEY_SLASH) || IsKeyPressed(KEY_KP_3),
        .block = IsKeyDown(KEY_DOWN),
        .dash = IsKeyPressed(KEY_RIGHT_SHIFT) || IsKeyPressed(KEY_KP_0),
    };
}
static int Navigate(int selected, int total) {
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        selected = (selected + 1) % total;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
        selected = (selected + total - 1) % total;
    return selected;
}
static bool Clicked(Vector2 mouse, Rectangle rect) {
    return CheckCollisionPointRec(mouse, rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
static bool Screenshot(RenderTexture2D target, const char *path) {
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    bool saved = ExportImage(image, path);
    UnloadImage(image);
    return saved;
}

int main(int argc, char **argv) {
    const char *capture = NULL;
    const char *scene = "menu";
    int frames = 1;
    bool demo = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc)
            capture = argv[++i];
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc)
            scene = argv[++i];
        else if (strcmp(argv[i], "--demo") == 0)
            demo = true;
        else if (strcmp(argv[i], "--help") == 0) {
            puts("Spirit Clash | --demo | --capture FILE [--scene menu|fight|pause|results] [--frames N]");
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
    if (strcmp(scene, "menu") && strcmp(scene, "fight") && strcmp(scene, "pause") &&
        strcmp(scene, "results")) {
        fputs("Unknown scene. Use menu, fight, pause or results.\n", stderr);
        return 1;
    }
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | (capture ? FLAG_WINDOW_HIDDEN : 0));
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(capture ? 1280 : 1120, capture ? 720 : 630, "Spirit Clash | Raylib Fighter");
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
    GameAudio audio = {0};
    if (!capture)
        GameAudioInit(&audio);
    Game game;
    GameStart(&game, demo || strcmp(scene, "menu") ? MODE_DEMO : MODE_SOLO, 1, 80377);
    bool title = !demo && strcmp(scene, "menu") == 0;
    bool quit = false, debug = false, reducedMotion = false;
    int exitStatus = 0;
    int selected = 0, pauseSelection = 0, resultSelection = 0, difficulty = 1, frame = 0;
    float accumulator = 0, menuTime = 0, demoEnd = 0;
    FighterInput pending[2] = {0};
    while (!WindowShouldClose() && !quit) {
        float dt = capture ? 1.0f / 60 : fminf(GetFrameTime(), .1f);
        menuTime += dt;
        float scale = fminf((float)GetScreenWidth() / ARENA_W, (float)GetScreenHeight() / ARENA_H);
        Vector2 offset = {(GetScreenWidth() - ARENA_W * scale) / 2,
                          (GetScreenHeight() - ARENA_H * scale) / 2};
        Vector2 mouse = {(GetMouseX() - offset.x) / scale, (GetMouseY() - offset.y) / scale};
        if (IsKeyPressed(KEY_M))
            GameAudioMute(&audio);
        if (IsKeyPressed(KEY_F10))
            ToggleBorderlessWindowed();
        if (IsKeyPressed(KEY_V))
            reducedMotion = !reducedMotion;
        if (IsKeyPressed(KEY_H))
            debug = !debug;
        if (title) {
            selected = Navigate(selected, 3);
            if (IsKeyPressed(KEY_LEFT))
                difficulty = (difficulty + 2) % 3;
            if (IsKeyPressed(KEY_RIGHT))
                difficulty = (difficulty + 1) % 3;
            if (Clicked(mouse, (Rectangle){76, 650, 410, 30}))
                difficulty = (difficulty + 1) % 3;
            bool start = IsKeyPressed(KEY_ENTER);
            for (int i = 0; i < 3; ++i)
                if (Clicked(mouse, (Rectangle){76, 414 + i * 76, 410, 64})) {
                    selected = i;
                    start = true;
                }
            if (start) {
                GameStart(&game,
                          selected == 0   ? MODE_SOLO
                          : selected == 1 ? MODE_LOCAL
                                          : MODE_TRAINING,
                          difficulty, 80377 + (unsigned)frame);
                title = false;
                accumulator = 0;
                memset(pending, 0, sizeof(pending));
            }
            if (IsKeyPressed(KEY_Q))
                quit = true;
        } else {
            if (!capture && game.phase != MATCH_END &&
                (IsKeyPressed(KEY_ESCAPE) || (!IsWindowFocused() && !game.paused))) {
                game.paused = !game.paused;
                pauseSelection = 0;
                accumulator = 0;
                memset(pending, 0, sizeof(pending));
            }
            if (game.paused) {
                pauseSelection = Navigate(pauseSelection, 3);
                bool choose = IsKeyPressed(KEY_ENTER);
                for (int i = 0; i < 3; ++i)
                    if (Clicked(mouse, (Rectangle){449, 300 + i * 56, 382, 45})) {
                        pauseSelection = i;
                        choose = true;
                    }
                if (choose) {
                    if (pauseSelection == 0)
                        game.paused = false;
                    else if (pauseSelection == 1)
                        GameStart(&game, game.mode, difficulty, 80377);
                    else {
                        title = true;
                        selected = 0;
                    }
                }
            } else if (game.phase == MATCH_END) {
                resultSelection = Navigate(resultSelection, 2);
                bool choose = IsKeyPressed(KEY_ENTER);
                for (int i = 0; i < 2; ++i)
                    if (Clicked(mouse, (Rectangle){449, 381 + i * 62, 382, 49})) {
                        resultSelection = i;
                        choose = true;
                    }
                if (choose) {
                    if (resultSelection == 0)
                        GameStart(&game, game.mode, difficulty, 80377 + (unsigned)frame);
                    else {
                        title = true;
                        selected = 0;
                    }
                }
                if (demo) {
                    demoEnd += dt;
                    if (demoEnd > 3) {
                        GameStart(&game, MODE_DEMO, difficulty, 80377 + (unsigned)frame);
                        demoEnd = 0;
                    }
                }
            } else {
                MergeInput(&pending[0], ReadPlayer(0));
                MergeInput(&pending[1], ReadPlayer(1));
                accumulator += dt;
                unsigned events = 0;
                while (accumulator >= FIXED_DT) {
                    GameUpdate(&game, pending[0], pending[1], FIXED_DT);
                    events |= game.soundEvents;
                    ConsumeButtons(&pending[0]);
                    ConsumeButtons(&pending[1]);
                    accumulator -= FIXED_DT;
                }
                GameAudioPlay(&audio, events);
            }
        }
        if (capture && strcmp(scene, "pause") == 0 && frame >= frames - 1)
            game.paused = true;
        if (capture && strcmp(scene, "results") == 0 && frame >= frames - 1) {
            game.phase = MATCH_END;
            game.winner = 0;
            game.fighters[0].wins = 2;
            game.fighters[1].wins = 1;
        }
        BeginTextureMode(canvas);
        ClearBackground((Color){15, 15, 39, 255});
        if (title)
            DrawMenu(&art, reducedMotion ? 0 : menuTime, selected, difficulty, audio.muted);
        else {
            DrawMatch(&art, &game, debug, reducedMotion);
            if (game.paused)
                DrawPause(&art, pauseSelection, audio.muted, reducedMotion);
            if (game.phase == MATCH_END)
                DrawResults(&art, &game, resultSelection);
        }
        EndTextureMode();
        if (!capture && IsKeyPressed(KEY_F12)) {
            char folder[2048], filename[2304], stamp[64];
            time_t now = time(NULL);
            strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", localtime(&now));
            snprintf(folder, sizeof(folder), "%sscreenshots", GetApplicationDirectory());
            if (!DirectoryExists(folder))
                MakeDirectory(folder);
            snprintf(filename, sizeof(filename), "%s/spirit-%s-%i.png", folder, stamp, frame);
            Screenshot(canvas, filename);
        }
        BeginDrawing();
        ClearBackground((Color){9, 10, 20, 255});
        DrawTexturePro(canvas.texture, (Rectangle){0, 0, ARENA_W, -ARENA_H},
                       (Rectangle){offset.x, offset.y, ARENA_W * scale, ARENA_H * scale}, (Vector2){0}, 0,
                       WHITE);
        EndDrawing();
        frame++;
        if (capture && frame >= frames) {
            exitStatus = Screenshot(canvas, capture) ? 0 : 1;
            break;
        }
    }
    GameAudioClose(&audio);
    UnloadRenderTexture(canvas);
    ArtUnload(&art);
    CloseWindow();
    return exitStatus;
}
