# Spirit Clash / raylib-fighter

This is a C17/raylib 6.x desktop fighting game. Preserve the native C/raylib stack.

- `src/combat.c` contains deterministic game simulation and AI. Keep simulation independent of rendering and audio devices.
- `src/render.c` uses 1280x720 logical coordinates. `main.c` sizes the render texture to physical framebuffer pixels and scales the logical camera; keep input coordinates letterboxed. Preserve the outer camera when drawing effects.
- Attack timing is in seconds. The main loop advances simulation at 120 Hz and buffers input edges across render frames.
- Sprite sheets use four columns and two rows, Ren above Kae. Character artwork is loaded from local `assets/`; the game must work offline.
- Build with `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` and `cmake --build build --parallel 4`.
- Verify combat changes with `ctest --test-dir build --output-on-failure`. Assertions must remain enabled in tests even in Release builds.
- Use `--capture /absolute/path.png --scene menu|fight|pause|results --frames N` for deterministic rendered checks. Fight capture uses an actual seeded AI exhibition. Pause/results options are rendering fixtures.
- Update screenshots for visible changes. Do not commit build directories, executable binaries, or third-party source checkouts.
