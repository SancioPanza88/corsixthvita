#include "vita_platform.h"
#include "vita_build_tag.h"

#include <cstdio>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

namespace corsix_vita {

const char* data_dir() { return "ux0:data/corsixth"; }
const char* save_dir() { return "ux0:data/corsixth/saves"; }

void power_tick() {
  // TODO(port): keep the console awake while a level is running.
  // Candidate is scePowerTick() from <psp2/power.h>; confirm the tick flag
  // against the installed vitaSDK headers before enabling, then call this
  // from the frame loop. Left empty on purpose until then.
}

// Runs before main(). Upstream never learns about ux0: paths at this stage;
// all we guarantee here is that the directories exist so early file writes
// (config, logs) do not fail. stdout/stderr go to ux0:data/corsixth/*.txt
// (unbuffered) so crashes and Lua errors leave a trace readable via
// VitaShell USB/FTP - on Vita there is no console to print to.
__attribute__((constructor)) static void vita_bringup() {
  sceIoMkdir("ux0:data", 0777);
  sceIoMkdir(data_dir(), 0777);
  sceIoMkdir(save_dir(), 0777);
  std::freopen("ux0:data/corsixth/stdout.txt", "w", stdout);
  std::freopen("ux0:data/corsixth/stderr.txt", "w", stderr);
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  std::fprintf(stderr, "corsixth-vita: bringup build=%s, entering main\n", VITA_BUILD_TAG);
  // app0: access matrix: stdio+fopen froze on the previous build without a
  // clean failure, so try every access method and log each result. Whatever
  // works here decides how the interpreter path is addressed.
  std::fprintf(stderr, "corsixth-vita: probe sceIoOpen(app0:) start\n");
  SceUID fd = sceIoOpen("app0:/game/CorsixTH.lua", SCE_O_RDONLY, 0777);
  std::fprintf(stderr, "corsixth-vita: probe sceIoOpen(app0:) fd=%d\n", (int)fd);
  if (fd >= 0) sceIoClose(fd);
  std::fprintf(stderr, "corsixth-vita: probe fopen(relative) start\n");
  if (std::FILE* rel = std::fopen("game/CorsixTH.lua", "rb")) {
    std::fseek(rel, 0, SEEK_END);
    long size = std::ftell(rel);
    std::fclose(rel);
    std::fprintf(stderr, "corsixth-vita: probe fopen(relative) ok, bytes=%ld\n", size);
  } else {
    std::fprintf(stderr, "corsixth-vita: probe fopen(relative) FAILED (errno=%d)\n", errno);
  }
  std::fprintf(stderr, "corsixth-vita: probe fopen(app0:) start\n");
  if (std::FILE* probe = std::fopen("app0:/game/CorsixTH.lua", "rb")) {
    std::fseek(probe, 0, SEEK_END);
    long size = std::ftell(probe);
    std::fclose(probe);
    std::fprintf(stderr, "corsixth-vita: probe fopen(app0:) ok, bytes=%ld\n", size);
  } else {
    std::fprintf(stderr, "corsixth-vita: probe fopen(app0:) FAILED (errno=%d)\n", errno);
  }
  std::fprintf(stderr, "corsixth-vita: probes done\n");
}

} // namespace corsix_vita

// Vita newlib provides no symlink(2)/readlink(2); luafilesystem references
// both (dir locking, make_link, symlinkattributes). The game never uses
// those calls - it needs attributes/dir/mkdir/chdir - so stub them to fail
// cleanly instead of breaking the link.
extern "C" {
int symlink(const char* target, const char* linkpath) {
  (void)target;
  (void)linkpath;
  errno = ENOSYS;
  return -1;
}
ssize_t readlink(const char* path, char* buf, size_t bufsiz) {
  (void)path;
  (void)buf;
  (void)bufsiz;
  errno = ENOSYS;
  return -1;
}
} // extern "C"
