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

if [ -f "$PREFIX/include/lua.h" ]; then
  echo "lua headers already present in SDK, skipping."
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
# used long long), so backfill the three macros under a guard. Skipped
# automatically if a future SDK header provides them.
if ! grep -q VITA_LLONG_BACKFILL "$PREFIX/include/luaconf.h"; then
  cat >> "$PREFIX/include/luaconf.h" <<'EOF'

/* VITA_LLONG_BACKFILL: newlib omits these in C++ mode; long long works. */
#if defined(__cplusplus) && !defined(LLONG_MAX)
#define LLONG_MAX 9223372036854775807LL
#define LLONG_MIN (-LLONG_MAX - 1LL)
#define ULLONG_MAX 18446744073709551615ULL
#endif
EOF
fi

echo "lua $LUA_VER installed into $PREFIX"
