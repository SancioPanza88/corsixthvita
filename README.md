# CorsixTH for PS Vita (unofficial port)

A PlayStation Vita port of [CorsixTH](https://github.com/CorsixTH/CorsixTH),
the open-source reimplementation of Bullfrog's 1997 classic Theme Hospital.
This repository holds the Vita build tree only; upstream sources are fetched
at configure time (pinned tag, see below) and are not vendored here.

**Status: work in progress — first bring-up.** CI builds a VPK; on-console
testing against real game data is still ahead. See `docs/vita-notes.md` for
the checklist.

## What this is

CorsixTH faithfully recreates the original management sim and adds modern
OS support, high resolutions, zooming and UI scaling. It fits the Vita well:
2D isometric rendering through SDL2, modest CPU needs (there is even a Wii
port upstream), and mouse-driven gameplay that maps naturally to the Vita's
front touchscreen.

Pinned upstream: **v0.70.1**. Newer upstream is migrating to SDL3, which has
no Vita backend yet, so the pin moves only after that situation changes.

## You will need

- A modded PS Vita / PS TV (HENkaku/Enso, firmware 3.60–3.65 recommended)
  with VitaShell installed.
- `libshacccg.suprx` in place (standard for SDL2/vitaGL homebrew; extract it
  with ShaRKBR33D if missing).
- A legal copy of the original Theme Hospital data files (GOG release works).
  Copy them to `ux0:data/corsixth/` after installing the VPK.

This port contains no game data and no upstream code — only the build glue
and a thin platform layer in `vita/`.

## Controls (planned)

The front touchscreen acts as the mouse (handled by SDL2 on Vita). Button
fallback, to be confirmed on hardware:

| Vita input | Action |
|---|---|
| Touchscreen | Mouse pointer, click |
| Left stick | Nudge cursor |
| X | Left click |
| O | Right click / cancel |
| Triangle | Pause menu |
| L / R | Zoom step |

## Building

### GitHub Actions (recommended)

Every push builds `corsixth.vpk` in the Actions tab (`corsixth-vita-vpk`
artifact). No local toolchain needed; download the artifact (GitHub login
required) and install it with VitaShell.

### Locally

Requirements: vitaSDK with `$VITASDK` exported, plus vdpm packages. The
workflow installs `sdl2`, `sdl2_mixer`, `freetype`, `libpng`, `zlib` and
builds stock Lua 5.4 via `vita/build-lua.sh` when vdpm does not provide it.

```sh
export VITASDK=/usr/local/vitasdk
export PATH=$VITASDK/bin:$PATH
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

The VPK lands at `build/corsixth.vpk`. Vita-safe configure choices
(movies/update-check/MIDI off, tools/tests off) are already forced in the
top-level `CMakeLists.txt`; background in `docs/vita-notes.md`.

## Project layout

```
CMakeLists.txt            fetch + configure upstream, link platform lib, package VPK
vita/vita_platform.*      data-dir bring-up, power hooks (thin on purpose)
vita/package.cmake        eboot.bin + VPK from the linked ELF (explicit step)
vita/build-lua.sh         fallback Lua 5.4 cross-build for the SDK
vita/gen_livearea.py      placeholder LiveArea art (stdlib PNG writer)
vita/sce_sys/...          LiveArea template (final art still TODO)
docs/vita-notes.md        paths, flags, input plan, bring-up checklist
.github/workflows/build.yml  vitasdk container build, uploads the VPK
```

## Known issues

- First boot will likely stop at the data-files screen until the data-dir
  lookup is redirected to `ux0:data/corsixth` (tracked in vita-notes).
- `lpeg` / `luafilesystem` load at runtime via `require` on desktop; on Vita
  they must be statically linked and pre-registered (next patch).
- In-game movies are disabled (no FFmpeg); MIDI music needs a FluidSynth
  path — OGG jukebox via SDL_mixer comes first.
- LiveArea art is placeholder.

## Contributing

Issues and pull requests welcome. Please build through the same pinned tag
and keep Vita-specific code inside `vita/` unless upstream accepts the
change (small, upstreamable patches preferred over forks).

## Credits

- CorsixTH team and contributors for the engine reimplementation.
- vitaSDK, SDL2-Vita, vitaGL and FalsoJNI/so-loader authors for the
  homebrew groundwork this port stands on.
- Bullfrog / EA for the original game. Buy it if you do not own it.

## License / disclaimer

Port glue in this repo: MIT (see `LICENSE.md`). Upstream CorsixTH: MIT,
(c) CorsixTH contributors. Unofficial fan port, not affiliated with or
endorsed by EA, Bullfrog or the CorsixTH team.
