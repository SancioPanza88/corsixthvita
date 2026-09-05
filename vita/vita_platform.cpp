#include "vita_platform.h"

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
// (config, logs) do not fail.
__attribute__((constructor)) static void vita_bringup() {
  sceIoMkdir("ux0:data", 0777);
  sceIoMkdir(data_dir(), 0777);
  sceIoMkdir(save_dir(), 0777);
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
