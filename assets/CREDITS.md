# Art and fonts

- Moonlit Sanctuary arena, Ren/Kae pose sheet, walking sheet, and four-frame hurt reactions were generated specifically for this project using OpenAI image generation. The designs are original and are not taken from an existing anime franchise.
- UI, combat effects, particle animation, sprite playback, and sound effects are implemented in the game code. Sound effects are synthesized at startup.
- The Storm Terrace arena is drawn procedurally in `src/stage.c` (sky, clouds, ridges, watchtower, balustrade, lanterns, rain, lightning, wet-floor reflections).
- Both music loops (the battle theme and the pentatonic menu theme) are original compositions generated note by note in `src/audio.c`; no recorded or third-party audio is used.
- Barlow Condensed Bold and Barlow SemiBold are by Jeremy Tribby and contributors, distributed under the SIL Open Font License. The license texts are included in `fonts/`.
- raylib is by Ramon Santamaria and contributors, under the zlib/libpng license. CMake links an installed raylib or fetches release 6.2.
