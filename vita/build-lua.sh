#!/usr/bin/env bash
# Build stock Lua 5.4 for the Vita target if vdpm did not provide one.
#
# Skips quietly when lua.h is already in the SDK. Otherwise downloads the
# 5.4.x tarball, cross-builds the static library only (the interpreter
# binary is useless on the build host anyway) and installs the headers +
# liblua.a into $VITASDK/arm-vita-eabi. Idempotent: safe to run on every
# CI invocation.
set -eu

: "${VITASDK:?VITASDK must be exported}"

LUA_VER="5.4.7"
PREFIX="$VITASDK/arm-vita-eabi"

# Vita newlib's <limits.h> only exposes LLONG_MAX when __STDC_VERSION__ is
# defined (C mode). In C++ mode the macro stays undefined, so every C++
# translation unit including lua.h dies in luaconf.h with "Compiler does not
# support 'long long'". The compiler itself is fine (long long works), it is
# purely a header-detection issue. The fix must appear BEFORE luaconf.h's
# `#if defined(LLONG_MAX)` check, i.e. right after `#include <limits.h>`.
# Appending at EOF is too late and does nothing.
patch_luaconf() {
  LUACONF="$PREFIX/include/luaconf.h"
  [ -f "$LUACONF" ] || return 0
  if grep -q VITA_LLONG_EARLY "$LUACONF"; then
    echo "luaconf.h already patched for Vita C++, skipping."
    return 0
  fi
  # Drop the legacy EOF backfill (too late to satisfy the LLONG check) if a
  # previous run left it behind. Harmless if absent.
  if grep -q VITA_LLONG_BACKFILL "$LUACONF"; then
    python3 - "$LUACONF" <<'PY'
import sys
path = sys.argv[1]
text = open(path, encoding="utf-8", errors="replace").read()
idx = text.find("/* VITA_LLONG_BACKFILL")
if idx != -1:
    text = text[:idx].rstrip() + "\n"
    open(path, "w", encoding="utf-8").write(text)
    print("removed legacy EOF VITA_LLONG_BACKFILL block")
PY
  fi
  python3 - "$LUACONF" <<'PY'
import sys
path = sys.argv[1]
text = open(path, encoding="utf-8", errors="replace").read()
if "VITA_LLONG_EARLY" in text:
    print("already patched")
    sys.exit(0)
anchor = "#include <limits.h>"
assert anchor in text, "luaconf.h layout changed: no '#include <limits.h>'"
early = anchor + """

/* VITA_LLONG_EARLY: Vita newlib hides LLONG_MAX/ULLONG_MAX in C++ mode
   (old __STDC_VERSION__-only guard in <limits.h>). long long itself works,
   so backfill the three macros here -- BEFORE luaconf.h's
   `#if defined(LLONG_MAX)` integer-type detection below. */
#if defined(__cplusplus) && !defined(LLONG_MAX)
#define LLONG_MAX 9223372036854775807LL
#define LLONG_MIN (-LLONG_MAX - 1LL)
#define ULLONG_MAX 18446744073709551615ULL
#endif"""
text = text.replace(anchor, early, 1)
open(path, "w", encoding="utf-8").write(text)
print("patched luaconf.h with VITA_LLONG_EARLY block")
PY
}

if [ -f "$PREFIX/include/lua.h" ]; then
  echo "lua headers already present in SDK, ensuring luaconf.h patch."
  patch_luaconf
  exit 0
fi

# Only what the game needs at link time.
CC_BIN="$VITASDK/bin/arm-vita-eabi-gcc"
AR_BIN="$VITASDK/bin/arm-vita-eabi-ar"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cd "$WORK"
curl -L -o lua.tar.gz "https://www.lua.org/ftp/lua-$LUA_VER.tar.gz"
tar xzf lua.tar.gz
cd "lua-$LUA_VER"

# Stock "generic" target: plain static liblua.a with no platform extras
# (no dlopen/readline). Upstream only uses the portable lauxlib subset.
# -Os keeps the text segment small.
make generic \
  CC="$CC_BIN" \
  AR="$AR_BIN rcu" \
  RANLIB="$VITASDK/bin/arm-vita-eabi-ranlib" \
  MYCFLAGS="-Os" \
  -j"$(nproc)"

make install \
  INSTALL_TOP="$PREFIX" \
  INSTALL_MAN="$WORK/man" \
  INSTALL_LMOD="$PREFIX/share/lua/$LUA_VER" \
  INSTALL_CMOD="$PREFIX/lib/lua/$LUA_VER"

# Vita newlib's <limits.h> only exposes LLONG_MAX in C mode, so every C++
# translation unit including lua.h dies in luaconf.h with "Compiler does not
# support 'long long'". The compiler itself is fine (the C build above just
# used long long). Patch via patch_luaconf() above, which inserts the
# backfill right after `#include <limits.h>` -- before the detection check.
patch_luaconf

echo "lua $LUA_VER installed into $PREFIX"
