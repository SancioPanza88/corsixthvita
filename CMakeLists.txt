# PS Vita port of CorsixTH.
#
# Upstream (MIT): https://github.com/CorsixTH/CorsixTH
# This tree does not vendor upstream. It fetches a pinned tag at configure
# time and builds it with Vita-safe options, then packages a VPK.
#
# Local build requirements: vitaSDK with $VITASDK exported, plus vdpm
# packages (see README). CI uses the vitasdk/vitasdk container instead.

cmake_minimum_required(VERSION 3.14)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Vita toolchain file")
  else()
    message(FATAL_ERROR "VITASDK is not set. Install vitaSDK and export VITASDK first (see README).")
  endif()
endif()

project(CorsixTH-Vita C CXX)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "CorsixTH")
set(VITA_TITLEID "CRSXTH001")
set(VITA_VERSION "00.90")

# Pinned upstream. Bump deliberately, after checking the changelog for
# dependency changes (notably the SDL2 -> SDL3 migration past 0.70).
set(CORSIXTH_TAG "v0.70.1" CACHE STRING "Upstream CorsixTH tag to build")

include(FetchContent)
FetchContent_Declare(
  corsixth_upstream
  URL "https://github.com/CorsixTH/CorsixTH/archive/refs/tags/${CORSIXTH_TAG}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Vita-safe defaults. Applied before the upstream tree is configured.
# Movies, update checks and MIDI hardware are desktop extras the handheld
# build does not need; AnimView/tools/tests are not shipped.
set(BUILD_CORSIXTH ON CACHE BOOL "" FORCE)
set(WITH_MOVIES OFF CACHE BOOL "" FORCE)
set(WITH_UPDATE_CHECK OFF CACHE BOOL "" FORCE)
set(WITH_MIDI_DEVICE OFF CACHE BOOL "" FORCE)
set(BUILD_ANIMVIEW OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ENABLE_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_TRACY OFF CACHE BOOL "" FORCE)
set(FETCH_SOUNDFONT OFF CACHE BOOL "" FORCE)
set(FETCH_UNICODE_FONT OFF CACHE BOOL "" FORCE)
# Keep off: WhereamiLib (pulled in by local-datadir search) has no Vita
# backend. Data-dir redirection to ux0: is handled in docs/vita-notes.md
# and will need a small upstream patch once the binary boots.
set(SEARCH_LOCAL_DATADIRS OFF CACHE BOOL "" FORCE)

# Vita newlib workaround: its <limits.h> only defines LLONG_MAX/ULLONG_MAX
# when __STDC_VERSION__ is set (C mode). In C++ mode the macros stay
# undefined, so Lua's luaconf.h hits `#error "Compiler does not support
# 'long long'"` even though the compiler itself handles long long fine.
# vita/build-lua.sh patches luaconf.h early (right after its
# `#include <limits.h>`); this is belt-and-braces so an unpatched header
# (e.g. a future vdpm lua package) still configures. Newer newlib guards
# with `#undef` before redefining, so predefining here is safe.
add_compile_options(
  $<$<COMPILE_LANGUAGE:CXX>:-DLLONG_MAX=9223372036854775807LL>
  $<$<COMPILE_LANGUAGE:CXX>:-DLLONG_MIN=(-9223372036854775807LL-1LL)>
  $<$<COMPILE_LANGUAGE:CXX>:-DULLONG_MAX=18446744073709551615ULL>
)

FetchContent_GetProperties(corsixth_upstream)
if(NOT corsixth_upstream_POPULATED)
  FetchContent_Populate(corsixth_upstream)
  # Upstream's CorsixTH/Src/CMakeLists.txt addresses config.h.in through
  # CMAKE_SOURCE_DIR, i.e. it assumes a top-level checkout. Rewrite that one
  # line to the list-dir-relative form so the tree also configures as a
  # FetchContent subdir. Revisit on tag bump: the guard below fails loudly
  # if upstream moves the line.
  set(_cfg_lists "${corsixth_upstream_SOURCE_DIR}/CorsixTH/Src/CMakeLists.txt")
  file(READ "${_cfg_lists}" _cfg_text)
  # Bracket arguments: no variable expansion, the ${...} below is matched
  # and written literally.
  string(REPLACE
    [==[configure_file(${CMAKE_SOURCE_DIR}/CorsixTH/Src/config.h.in]==]
    [==[configure_file(${CMAKE_CURRENT_LIST_DIR}/config.h.in]==]
    _cfg_text "${_cfg_text}")
  if(NOT _cfg_text MATCHES "CMAKE_CURRENT_LIST_DIR}/config.h.in")
    message(FATAL_ERROR "Upstream Src/CMakeLists layout changed - revisit the config.h.in path fix.")
  endif()
  file(WRITE "${_cfg_lists}" "${_cfg_text}")
endif()
add_subdirectory("${corsixth_upstream_SOURCE_DIR}" "${corsixth_upstream_BINARY_DIR}")

# Small static lib with Vita bring-up (data dirs, power policy hooks).
# Linked into the upstream executable below; the init itself runs from a
# constructor so no upstream patching is needed for the first boot.
add_library(vita_platform STATIC
  vita/vita_platform.cpp
)
target_include_directories(vita_platform PUBLIC vita)
target_link_libraries(vita_platform PUBLIC
  SceIofilemgr_stub
  ScePower_stub
)

# Upstream declares add_executable(CorsixTH ...) in CorsixTH/CMakeLists.txt.
if(NOT TARGET CorsixTH)
  message(FATAL_ERROR "Upstream target 'CorsixTH' not found - check CORSIXTH_TAG layout.")
endif()
target_link_libraries(CorsixTH PRIVATE vita_platform)

# Placeholder LiveArea art is generated at configure time (see
# vita/gen_livearea.py) so the repo stays text-only. Real art lands later.
# Pairs are collected as "src=dst" lines for vita/package.cmake; when python3
# is missing the game still packages, just with default LiveArea art.
set(VITA_LIVEAREA_DIR "${CMAKE_BINARY_DIR}/sce_sys")
find_program(VITA_PYTHON python3)
set(VITA_ART_PAIRS "")
if(VITA_PYTHON)
  execute_process(
    COMMAND "${VITA_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/vita/gen_livearea.py" "${VITA_LIVEAREA_DIR}"
    RESULT_VARIABLE _livearea_rc
  )
endif()
if(VITA_PYTHON AND _livearea_rc EQUAL 0)
  list(APPEND VITA_ART_PAIRS
    "${VITA_LIVEAREA_DIR}/icon0.png=sce_sys/icon0.png"
    "${VITA_LIVEAREA_DIR}/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png"
    "${VITA_LIVEAREA_DIR}/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/vita/sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml"
  )
else()
  message(WARNING "LiveArea art skipped (no python3 or generator failed) - packaging with defaults.")
endif()
string(REPLACE ";" "\n" _art_text "${VITA_ART_PAIRS}")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/vita_art.txt" CONTENT "${_art_text}\n")

# libSDL2.a / libSDL2_mixer.a come in by plain path (upstream uses legacy
# find modules), so their Sce dependencies do not propagate as INTERFACE
# usage requirements the way SDL2::SDL2 would. List the set SDL itself
# needs on Vita explicitly (mirrors SDL's EXTRA_LIBS for the VITA target).
# No libm: newlib keeps math in libc and SDL deliberately skips it on Vita.
target_link_libraries(CorsixTH PRIVATE
  SceGxm_stub
  SceDisplay_stub
  SceCtrl_stub
  SceAppMgr_stub
  SceAppUtil_stub
  SceAudio_stub
  SceAudioIn_stub
  SceSysmodule_stub
  SceIofilemgr_stub
  SceCommonDialog_stub
  SceTouch_stub
  SceHid_stub
  SceMotion_stub
  ScePower_stub
  SceProcessmgr_stub
)

# Packaging runs as an explicit second build step (see workflow), strictly
# after the executable is linked. Deliberately NOT done with the
# vita_create_self/vpk ALL-target macros: those race the link under
# parallel make on large trees (velf conversion firing on a missing ELF).
add_custom_target(package-vpk
  COMMAND "${CMAKE_COMMAND}"
    "-DELF_FILE=$<TARGET_FILE:CorsixTH>"
    "-DOUT_DIR=${CMAKE_BINARY_DIR}"
    "-DTITLEID=${VITA_TITLEID}"
    "-DVERSION=${VITA_VERSION}"
    "-DAPP_NAME=${VITA_APP_NAME}"
    "-DART_LIST=${CMAKE_BINARY_DIR}/vita_art.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/vita/package.cmake"
  COMMENT "Packaging corsixth.vpk"
  VERBATIM
)
add_dependencies(package-vpk CorsixTH)
