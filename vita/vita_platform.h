#pragma once

// Minimal Vita bring-up for the CorsixTH port.
//
// The constructor in vita_platform.cpp creates the data directories before
// main() runs, so no upstream patching is needed for first boot. Everything
// else here is meant to be wired into the frame loop / config code as the
// port matures (see docs/vita-notes.md).

namespace corsix_vita {

// Writable locations on the memory card. The original game data (HOSP,
// levels, music) is expected under data_dir(), copied over by the user.
const char* data_dir(); // "ux0:data/corsixth"
const char* save_dir(); // "ux0:data/corsixth/saves"

// Prevents auto-suspend during long sessions. Call once per frame once the
// main loop is patched. Currently a stub (see .cpp).
void power_tick();

} // namespace corsix_vita
