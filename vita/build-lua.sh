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

# Plain ANSI target; upstream CorsixTH wants Lua 5.4.x and only uses the
# portable subset through lauxlib. -Os keeps the text segment small.
make a \
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

echo "lua $LUA_VER installed into $PREFIX"
