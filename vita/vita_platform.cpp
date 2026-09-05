#include "vita_platform.h"
#include "vita_build_tag.h"

#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
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
  // stdio fopen() freezes on this build while raw sceIoOpen() works, so map
  // the fault line: heap/malloc first, then POSIX open(), then sceIo reads.
  // (Previous fopen probes hung the boot and were removed.)
  std::fprintf(stderr, "corsixth-vita: probe malloc start\n");
  void* heap_probe = std::malloc(1024);
  std::fprintf(stderr, "corsixth-vita: probe malloc=%p\n", heap_probe);
  std::free(heap_probe);
  std::fprintf(stderr, "corsixth-vita: probe open(app0:) start\n");
  int posix_fd = ::open("app0:/game/CorsixTH.lua", O_RDONLY);
  std::fprintf(stderr, "corsixth-vita: probe open(app0:) fd=%d\n", posix_fd);
  if (posix_fd >= 0) ::close(posix_fd);
  std::fprintf(stderr, "corsixth-vita: probe sceIoOpen(app0:) start\n");
  SceUID fd = sceIoOpen("app0:/game/CorsixTH.lua", SCE_O_RDONLY, 0777);
  std::fprintf(stderr, "corsixth-vita: probe sceIoOpen(app0:) fd=%d\n", (int)fd);
  if (fd >= 0) {
    long size = (long)sceIoLseek(fd, 0, SCE_SEEK_END);
    std::fprintf(stderr, "corsixth-vita: probe sceIo size=%ld\n", size);
    sceIoClose(fd);
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
