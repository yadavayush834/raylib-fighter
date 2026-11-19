# Spirit Clash / raylib-fighter

This is a C17/raylib 6.x desktop fighting game. Preserve the native C/raylib stack.

- `src/combat.c` is the deterministic simulation (moves, cancels, juggles, guard, supers, rounds) and `src/ai.c` the CPU. Keep both independent of rendering and audio devices; randomness only comes from `GameRandom`.
- The simulation publishes `GameEvent`s each step. `src/engine/fx.c` (camera, shake, slow motion, particles, popups) and `src/audio.c` react to them; never feed presentation state back into the sim.
- `src/engine/input.c` owns every keyboard/gamepad query. Use `InputKeyPressed`/`InputKeyDown`, never raylib's directly, so input replay (`--script`) keeps working.
- `src/render.c` draws fighters and the world, `src/stage.c` the arenas, `src/ui.c` the HUD, menus and overlays. All use 1280x720 logical coordinates; `RenderBaseCamera()` maps them to the physical-density canvas. A world draw ends by restoring the base camera.
- Attack timing is in seconds. The main loop advances simulation at 120 Hz (scaled by FX slow motion) and buffers input edges across render frames; attack presses also buffer through hitstop.
- Sprite sheets use four columns and two rows, Ren above Kae; the row is the fighter's `character`. Character artwork is loaded from local `assets/`; the game must work offline.
- Build with `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` and `cmake --build build --parallel 4`.
- Verify combat and AI changes with `ctest --test-dir build --output-on-failure`. Assertions must remain enabled in tests even in Release builds.
- Play-test the whole game with `./build/spirit-clash --script tests/playtest/solo.txt --snap-dir /tmp/snaps` (also `training`, `versus`), or configure with `-DSPIRIT_PLAYTESTS=ON` to run them through CTest.
- Use `--capture /absolute/path.png --scene menu|select|versus|fight|super|hurt|pause|results|moves [--stage storm] --frames N` for deterministic rendered checks. Fight captures use a seeded CPU exhibition.
- Update screenshots in `docs/screenshots/` for visible changes. Do not commit build directories, executable binaries, or third-party source checkouts.
