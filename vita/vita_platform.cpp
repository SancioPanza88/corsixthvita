#include "vita_platform.h"
#include "vita_build_tag.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

struct _reent;

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
  // Absolute-device paths work but cwd-dependent calls (fopen, status) freeze,
  // which smells like an unset working directory. Pin it to app0:/ and log it;
  // if this line never appears, chdir itself is the hang.
  std::fprintf(stderr, "corsixth-vita: chdir(app0:/) start\n");
  int chdir_rc = chdir("app0:/");
  std::fprintf(stderr, "corsixth-vita: chdir(app0:/) rc=%d errno=%d\n", chdir_rc, chdir_rc == 0 ? 0 : errno);
  std::fprintf(stderr, "corsixth-vita: probes done\n");
}

} // namespace corsix_vita

// Vita newlib's _stat_r/_fstat_r hang on app0: paths (verified on hardware:
// fopen() and std::filesystem::status() freeze with no error while open(),
// sceIoOpen() and sceIoLseek() work). Everything file-shaped - Lua loadfile,
// persist, lfs, SDL, freetype - is built on those two, so override them with
// implementations using only the proven primitives. Static linking prefers
// these definitions over libc's. st_mode/st_size are accurate for stat();
// fstat() gets a plausible regular-file answer, which is all fopen needs.
extern "C" {
int _stat_r(struct _reent* reent, const char* path, struct stat* buf) {
  (void)reent;
  if (!path || !buf) {
    errno = EINVAL;
    return -1;
  }
  SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0777);
  if (fd >= 0) {
    SceOff end = sceIoLseek(fd, 0, SCE_SEEK_END);
    sceIoClose(fd);
    if (end < 0) {
      errno = EIO;
      return -1;
    }
    std::memset(buf, 0, sizeof(*buf));
    buf->st_mode = S_IFREG | 0444;
    buf->st_nlink = 1;
    buf->st_size = (off_t)end;
    buf->st_blksize = 4096;
    return 0;
  }
  SceUID dir = sceIoDopen(path);
  if (dir >= 0) {
    sceIoDclose(dir);
    std::memset(buf, 0, sizeof(*buf));
    buf->st_mode = S_IFDIR | 0555;
    buf->st_nlink = 1;
    return 0;
  }
  errno = ENOENT;
  return -1;
}
int _fstat_r(struct _reent* reent, int fd, struct stat* buf) {
  (void)reent;
  (void)fd;
  if (!buf) {
    errno = EINVAL;
    return -1;
  }
  std::memset(buf, 0, sizeof(*buf));
  buf->st_mode = S_IFREG | 0644;
  buf->st_nlink = 1;
  buf->st_size = 0;
  buf->st_blksize = 4096;
  return 0;
}
} // extern "C"

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
