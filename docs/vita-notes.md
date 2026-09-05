# Vita port notes

Working notes for the PS Vita bring-up. Kept next to the code so they stay
in sync with it. If something here contradicts the README, the README wins.

## Paths

- Game code and Lua scripts: resolved by upstream relative to its data dir.
- Writable storage: `ux0:data/corsixth/` (created at boot, see
  `vita/vita_platform.cpp`). Saves go to `ux0:data/corsixth/saves/`.
- Original Theme Hospital data (`HOSP`, levels, music): copy the GOG install
  files into `ux0:data/corsixth/` with FTP/VitaShell. Same layout as desktop.

## Build flags (why)

Upstream tag `v0.70.1`, configured with:

- `WITH_MOVIES=OFF` — no FFmpeg on Vita yet. Videos are skipped; the game
  is fully playable without them.
- `WITH_UPDATE_CHECK=OFF` — needs libcurl + internet UX we do not want.
- `WITH_MIDI_DEVICE=OFF` — RtMidi has no Vita backend.
- `SEARCH_LOCAL_DATADIRS=OFF` — avoids WhereamiLib, which has no Vita
  backend either.
- `BUILD_ANIMVIEW/BUILD_TOOLS/ENABLE_UNIT_TESTS=OFF` — not shipped.

First runtime milestone is booting to the setup wizard / main menu. The data
dir baked into upstream `config.h` still points at a desktop-style prefix;
expect a "data files not found" screen on first boot and patch the lookup
towards `ux0:data/corsixth` next (small, contained change).

## Input plan

SDL2 on Vita already exposes the front touchscreen as mouse events, which
covers 90% of this game (it is mouse-driven on desktop). Remaining mapping:

- Left stick: mouse cursor nudge for pixel-precise placement.
- X: left click. O: right click / cancel. Triangle: pause menu.
- L/R: fast scroll / zoom steps once zoom keys are confirmed in-game.

Touchscreen stays the primary pointer; buttons are a fallback.

## Known blockers (runtime, not build)

- `lpeg` / `luafilesystem` are loaded by upstream at runtime via `require`.
  Vita homebrew cannot `dlopen` those the way desktop does. Fix: build both
  as static archives and register them from C before the Lua state boots
  (upstream already supports linked modules under vcpkg; mirror that).
- MIDI music needs FluidSynth or a soundfont path; OGG jukebox tracks work
  through SDL_mixer and come first.

## Bring-up checklist

1. [ ] CI produces `corsixth.vpk` (this repo's Actions tab).
2. [ ] Installs via VitaShell, boots to wizard/menu (no data files yet).
3. [ ] With GOG data in place: new hospital builds, staff hired, patients flow.
4. [ ] Touch controls verified on hardware; button fallback mapped.
5. [ ] Static lpeg/lfs; save/load round-trip; overclock-off performance pass.
