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

# Build tag baked into the boot log so a stderr.txt always tells us exactly
# which binary produced it (ends "which VPK is installed" disputes).
if(DEFINED ENV{GITHUB_SHA})
  string(SUBSTRING "$ENV{GITHUB_SHA}" 0 7 VITA_BUILD_TAG)
endif()
if(NOT VITA_BUILD_TAG)
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE VITA_BUILD_TAG
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
  endif()
endif()
if(NOT VITA_BUILD_TAG)
  set(VITA_BUILD_TAG "unknown")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/vita_build_tag.h"
  "#pragma once\n#define VITA_BUILD_TAG \"${VITA_BUILD_TAG}\"\n")

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
  # Upstream bakes CORSIX_TH_INTERPRETER_PATH from the desktop install prefix
  # (/usr/local/share/...), which does not exist on Vita, so the game would
  # exit(1) right after the LiveArea splash. Point it at the bundled copy
  # instead (packed under game/ by the rules further below).
  # Revisit on tag bump: the guard below fails loudly if upstream moves it.
  set(_vita_lists "${corsixth_upstream_SOURCE_DIR}/CorsixTH/CMakeLists.txt")
  file(READ "${_vita_lists}" _vita_text)
  string(REPLACE
    [==[set(CORSIX_TH_DATADIR ${CMAKE_INSTALL_DATADIR}/corsix-th)]==]
    [==[set(CORSIX_TH_DATADIR "app0:/game") # VITA_PATCHED_DATADIR]==]
    _vita_text "${_vita_text}")
  string(REPLACE
    [==[set(CORSIX_TH_INTERPRETER_PATH ${CMAKE_INSTALL_FULL_DATADIR}/corsix-th/${CORSIX_TH_INTERPRETER_NAME})]==]
    [==[set(CORSIX_TH_INTERPRETER_PATH "app0:/game/${CORSIX_TH_INTERPRETER_NAME}") # VITA_PATCHED_INTERPRETER]==]
    _vita_text "${_vita_text}")
  if(NOT _vita_text MATCHES "VITA_PATCHED_INTERPRETER")
    message(FATAL_ERROR "Upstream CorsixTH/CMakeLists layout changed - revisit the app0:/game datadir fix.")
  endif()
  file(WRITE "${_vita_lists}" "${_vita_text}")
  # Temporary bring-up tracing: stderr goes to ux0:data/corsixth/stderr.txt
  # (see vita_platform.cpp), so a few markers here tell us exactly where a
  # silent hang happens. Remove once the game reaches the menu.
  # Revisit on tag bump: the guard below fails loudly if upstream moves it.
  set(_vita_main "${corsixth_upstream_SOURCE_DIR}/CorsixTH/SrcUnshared/main.cpp")
  file(READ "${_vita_main}" _vita_main_text)
  string(REPLACE
    [==[lua_atpanic(L.get(), lua_panic);]==]
    [==[lua_atpanic(L.get(), lua_panic);
    std::fprintf(stderr, "vita: lua state opened\n"); // VITA_TRACE_STATE]==]
    _vita_main_text "${_vita_main_text}")
  string(REPLACE
    [==[if (lua_pcall(L.get(), argc, 0, 1) != 0) {]==]
    [==[std::fprintf(stderr, "vita: calling lua_init\n"); // VITA_TRACE_INIT
    if (lua_pcall(L.get(), argc, 0, 1) != 0) {]==]
    _vita_main_text "${_vita_main_text}")
  string(REPLACE
    [==[mainloop(L.get());]==]
    [==[std::fprintf(stderr, "vita: entering mainloop\n"); // VITA_TRACE_LOOP
    mainloop(L.get());
    std::fprintf(stderr, "vita: mainloop exited\n");]==]
    _vita_main_text "${_vita_main_text}")
  string(REPLACE
    [==[auto scriptFilePath = search_script_file(L);]==]
    [==[auto scriptFilePath = search_script_file(L);
  std::fprintf(stderr, "vita: interpreter path='%s'\n", scriptFilePath.c_str()); // VITA_TRACE_PATH]==]
    _vita_main_text "${_vita_main_text}")
  string(REPLACE
    [==[lua_call(L, 1, 2);
  lua_call(L, 2, 1);]==]
    [==[lua_call(L, 1, 2);
  std::fprintf(stderr, "vita: loadfile done\n"); // VITA_TRACE_LOAD
  lua_call(L, 2, 1);
  std::fprintf(stderr, "vita: interpreter chunk returned\n");]==]
    _vita_main_text "${_vita_main_text}")
  if(NOT _vita_main_text MATCHES "VITA_TRACE_LOOP")
    message(FATAL_ERROR "Upstream SrcUnshared/main.cpp layout changed - revisit the vita trace markers.")
  endif()
  file(WRITE "${_vita_main}" "${_vita_main_text}")
  # Fine-grained markers inside lua_init_no_eval: the hang sits between
  # "calling lua_init" and the interpreter-path print, and this span holds
  # the version check, luaopen_random, the preload table, require"debug"
  # and search_script_file - exactly one marker per step. Temporary.
  # Revisit on tag bump: the guard below fails loudly if upstream moves it.
  set(_vita_src_main "${corsixth_upstream_SOURCE_DIR}/CorsixTH/Src/main.cpp")
  file(READ "${_vita_src_main}" _vita_noeval_text)
  string(REPLACE
    [==[lua_pop(L, 1);]==]
    [==[lua_pop(L, 1);
  std::fprintf(stderr, "vita: lua version ok\n"); // VITA_TRACE_NOEVAL]==]
    _vita_noeval_text "${_vita_noeval_text}")
  string(REPLACE
    [==[luaT_cpcall(L, luaopen_random, nullptr);]==]
    [==[luaT_cpcall(L, luaopen_random, nullptr);
  std::fprintf(stderr, "vita: random opened\n"); // VITA_TRACE_RANDOM]==]
    _vita_noeval_text "${_vita_noeval_text}")
  string(REPLACE
    [==[luaT_execute(L, "require \"debug\"");]==]
    [==[luaT_execute(L, "require \"debug\"");
  std::fprintf(stderr, "vita: debug required\n"); // VITA_TRACE_DEBUG]==]
    _vita_noeval_text "${_vita_noeval_text}")
  string(REPLACE
    [==[auto scriptFilePath = search_script_file(L);]==]
    [==[std::fprintf(stderr, "vita: entering search\n"); // VITA_TRACE_SEARCH
  auto scriptFilePath = search_script_file(L);]==]
    _vita_noeval_text "${_vita_noeval_text}")
  if(NOT _vita_noeval_text MATCHES "VITA_TRACE_SEARCH")
    message(FATAL_ERROR "Upstream Src/main.cpp layout changed - revisit the vita noeval trace markers.")
  endif()
  file(WRITE "${_vita_src_main}" "${_vita_noeval_text}")
  # std::filesystem::status() hangs/throws on Vita device paths (app0:/ux0:),
  # which freezes the interpreter search with zero output. Probe readability
  # with a raw POSIX open() instead - proven working on hardware. The only
  # caller checks the bundled interpreter file, so treating readable dirs as
  # existing is harmless.
  # Revisit on tag bump: the guard below fails loudly if upstream moves it.
  file(READ "${_vita_src_main}" _vita_src_main_text)
  string(REPLACE
    [==[#include <filesystem>]==]
    [==[#include <fcntl.h> // VITA_POSIX_IO (<filesystem> removed, unusable on Vita)
#include <unistd.h>]==]
    _vita_src_main_text "${_vita_src_main_text}")
  string(REPLACE
    [==[bool file_exists(const std::filesystem::path& f) {
  const std::filesystem::file_status st = std::filesystem::status(f);
  return std::filesystem::exists(st) && !std::filesystem::is_directory(st);
}]==]
    [==[bool file_exists(const char* f) {
  // VITA_FILE_EXISTS_PATCH: the C++ filesystem layer is unusable on Vita
  // device paths (audited: this was its only user in the whole game), so a
  // raw POSIX open() is all the existence check needs. Plain C types only.
  std::fprintf(stderr, "vita: file_exists('%s')\n", f);
  const int fd = ::open(f, O_RDONLY);
  std::fprintf(stderr, "vita: file_exists open fd=%d\n", fd);
  if (fd < 0) return false;
  ::close(fd);
  return true;
}]==]
    _vita_src_main_text "${_vita_src_main_text}")
  if(NOT _vita_src_main_text MATCHES "VITA_FILE_EXISTS_PATCH")
    message(FATAL_ERROR "Upstream Src/main.cpp layout changed - revisit the vita file_exists fix.")
  endif()
  file(WRITE "${_vita_src_main}" "${_vita_src_main_text}")
endif()
add_subdirectory("${corsixth_upstream_SOURCE_DIR}" "${corsixth_upstream_BINARY_DIR}")

# Small static lib with Vita bring-up (data dirs, power policy hooks).
# Linked into the upstream executable below; the init itself runs from a
# constructor so no upstream patching is needed for the first boot.
add_library(vita_platform STATIC
  vita/vita_platform.cpp
)
target_include_directories(vita_platform PUBLIC vita)
target_include_directories(vita_platform PRIVATE "${CMAKE_BINARY_DIR}")
target_link_libraries(vita_platform PUBLIC
  SceIofilemgr_stub
  ScePower_stub
)

# Upstream declares add_executable(CorsixTH ...) in CorsixTH/CMakeLists.txt.
if(NOT TARGET CorsixTH)
  message(FATAL_ERROR "Upstream target 'CorsixTH' not found - check CORSIXTH_TAG layout.")
endif()
target_link_libraries(CorsixTH PRIVATE vita_platform)

# Upstream only preloads lfs/lpeg when CORSIX_TH_LINK_LUA_MODULES is set,
# which it limits to vcpkg builds - everywhere else they load via require at
# runtime (.so), which cannot work on Vita. Build both from source against
# our Lua 5.4 headers and flip the switch: config.h emits it as a commented
# `#undef` when OFF, so this -D wins without touching upstream sources.
FetchContent_Declare(
  vita_lfs
  URL "https://github.com/lunarmodules/luafilesystem/archive/refs/tags/v1_8_0.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
  vita_lpeg
  URL "http://www.inf.puc-rio.br/~roberto/lpeg/lpeg-1.1.0.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(vita_lfs)
if(NOT vita_lfs_POPULATED)
  FetchContent_Populate(vita_lfs)
endif()
FetchContent_GetProperties(vita_lpeg)
if(NOT vita_lpeg_POPULATED)
  FetchContent_Populate(vita_lpeg)
endif()
# lpeg 1.1.0 ships split sources (lpcode.c, lpcap.c, ...), older releases a
# single lpeg.c; the tarball may also nest them one level down. Glob it all.
file(GLOB _vita_lpeg_src
  "${vita_lpeg_SOURCE_DIR}/lp*.c"
  "${vita_lpeg_SOURCE_DIR}/*/lp*.c"
  "${vita_lpeg_SOURCE_DIR}/lpeg.c"
  "${vita_lpeg_SOURCE_DIR}/*/lpeg.c"
)
list(LENGTH _vita_lpeg_src _vita_lpeg_n)
if(NOT _vita_lpeg_n GREATER 0)
  message(FATAL_ERROR "No lpeg sources found under ${vita_lpeg_SOURCE_DIR}")
endif()
add_library(vita_luamods STATIC
  "${vita_lfs_SOURCE_DIR}/src/lfs.c"
  ${_vita_lpeg_src}
)
target_include_directories(vita_luamods PRIVATE "$ENV{VITASDK}/arm-vita-eabi/include")
target_compile_definitions(CorsixTH PRIVATE CORSIX_TH_LINK_LUA_MODULES)
# NOTE: vita_platform linked again after luamods - it provides the
# symlink/readlink stubs luafilesystem needs, and static linking is
# one-pass, so the provider must come after the user.
target_link_libraries(CorsixTH PRIVATE vita_luamods vita_platform)

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
    "${VITA_LIVEAREA_DIR}/pic0.png=sce_sys/pic0.png"
    "${VITA_LIVEAREA_DIR}/livearea/contents/bg0.png=sce_sys/livearea/contents/bg0.png"
    "${VITA_LIVEAREA_DIR}/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/vita/sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml"
  )
else()
  message(WARNING "LiveArea art skipped (no python3 or generator failed) - packaging with defaults.")
endif()
# Upstream game data (interpreter + scripts + art) bundled into the VPK
# under game/ so app0:/game/CorsixTH.lua (see patch above) resolves.
# Original Theme Hospital data stays user-provided in ux0:data/corsixth.
set(VITA_GAME_DIR "${CMAKE_BINARY_DIR}/game")
file(COPY "${corsixth_upstream_SOURCE_DIR}/CorsixTH/CorsixTH.lua" DESTINATION "${VITA_GAME_DIR}")
foreach(_vita_data_dir Lua Bitmap Campaigns Levels)
  file(COPY "${corsixth_upstream_SOURCE_DIR}/CorsixTH/${_vita_data_dir}" DESTINATION "${VITA_GAME_DIR}")
endforeach()
# Official config redirect: config_finder.lua honours Lua/config.path.txt, so
# config.txt, saves and screenshots land on the memory card instead of the
# read-only app partition. Created here (not upstream) for the same reason.
file(WRITE "${VITA_GAME_DIR}/Lua/config.path.txt" "ux0:data/corsixth\n")
# Temporary bring-up tracing inside the bundled interpreter copy (stdout goes
# to ux0:data/corsixth/stdout.txt). Shows how far CorsixTH.lua gets before a
# silent hang. Remove once the game reaches the menu. Bracket args below span
# real newlines on purpose.
file(READ "${VITA_GAME_DIR}/CorsixTH.lua" _vita_interp)
string(REPLACE
  [==[package.path = code_dir .. "?.lua;" .. code_dir .. "?/init.lua;" .. package.path]==]
  [==[package.path = code_dir .. "?.lua;" .. code_dir .. "?/init.lua;" .. package.path
print("vita: interpreter running, code_dir=" .. code_dir) -- VITA_LUA_TRACE]==]
  _vita_interp "${_vita_interp}")
string(REPLACE
  [==[corsixth.require("utility")]==]
  [==[print("vita: require utility") -- VITA_LUA_TRACE
corsixth.require("utility")]==]
  _vita_interp "${_vita_interp}")
string(REPLACE
  [==[corsixth.require("class")]==]
  [==[print("vita: require class") -- VITA_LUA_TRACE
corsixth.require("class")]==]
  _vita_interp "${_vita_interp}")
string(REPLACE
  [==[corsixth.require("app")]==]
  [==[print("vita: require app") -- VITA_LUA_TRACE
corsixth.require("app")]==]
  _vita_interp "${_vita_interp}")
string(REPLACE
  [==[TheApp = App()]==]
  [==[print("vita: creating App") -- VITA_LUA_TRACE
TheApp = App()]==]
  _vita_interp "${_vita_interp}")
string(REPLACE
  [==[assert(TheApp:init())]==]
  [==[print("vita: calling App:init") -- VITA_LUA_TRACE
assert(TheApp:init())
print("vita: App:init returned") -- VITA_LUA_TRACE]==]
  _vita_interp "${_vita_interp}")
if(NOT _vita_interp MATCHES "App:init returned")
  message(FATAL_ERROR "Bundled CorsixTH.lua layout changed - revisit the vita lua trace markers.")
endif()
file(WRITE "${VITA_GAME_DIR}/CorsixTH.lua" "${_vita_interp}")
file(GLOB_RECURSE _vita_game_files LIST_DIRECTORIES FALSE "${VITA_GAME_DIR}/*")
foreach(_vita_game_file IN LISTS _vita_game_files)
  file(RELATIVE_PATH _vita_game_rel "${VITA_GAME_DIR}" "${_vita_game_file}")
  list(APPEND VITA_ART_PAIRS "${VITA_GAME_DIR}/${_vita_game_rel}=game/${_vita_game_rel}")
endforeach()
string(REPLACE ";" "\n" _art_text "${VITA_ART_PAIRS}")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/vita_art.txt" CONTENT "${_art_text}\n")

# libSDL2.a / libSDL2_mixer.a come in by plain path (upstream uses legacy
# find modules), so their dependencies do not propagate as INTERFACE
# usage requirements the way SDL2::SDL2 would. Upstream also links mixer
# AFTER SDL, which breaks static linking (mixer calls SDL_LoadWAV_RW etc.).
# List the full static closure explicitly (matches sdl2_mixer 2.8.2 + freetype
# vdpm recipes) and wrap it in a linker group so order/cycles
# (mixer<->SDL, vorbisfile<->vorbis, etc.) cannot break the link.
# No libm: newlib keeps math in libc and SDL deliberately skips it on Vita.
set(_vita_libdir "$ENV{VITASDK}/arm-vita-eabi/lib")
find_library(VITA_SDL2_LIB NAMES SDL2 PATHS "${_vita_libdir}" NO_DEFAULT_PATH)
find_library(VITA_SDL2MIXER_LIB NAMES SDL2_mixer PATHS "${_vita_libdir}" NO_DEFAULT_PATH)
find_library(VITA_VORBISFILE_LIB NAMES vorbisfile PATHS "${_vita_libdir}" NO_DEFAULT_PATH)
find_library(VITA_VORBIS_LIB NAMES vorbis PATHS "${_vita_libdir}" NO_DEFAULT_PATH)
find_library(VITA_OGG_LIB NAMES ogg PATHS "${_vita_libdir}" NO_DEFAULT_PATH)
find_library(VITA_OPUSFILE_LIB NAMES opusfile PATHS "${_vita_libdir}" NO_DEFAULT_PATH)
find_library(VITA_OPUS_LIB NAMES opus PATHS "${_vita_libdir}" NO_DEFAULT_PATH)
find_library(VITA_MODPLUG_LIB NAMES modplug PATHS "${_vita_libdir}" NO_DEFAULT_PATH)
find_library(VITA_XMP_LIB NAMES xmp-lite xmp PATHS "${_vita_libdir}" NO_DEFAULT_PATH)
find_library(VITA_FLAC_LIB NAMES FLAC PATHS "${_vita_libdir}" NO_DEFAULT_PATH)
find_library(VITA_BZ2_LIB NAMES bz2 PATHS "${_vita_libdir}" NO_DEFAULT_PATH)
foreach(_vita_req VITA_SDL2_LIB VITA_SDL2MIXER_LIB VITA_VORBISFILE_LIB VITA_VORBIS_LIB VITA_OGG_LIB VITA_OPUSFILE_LIB VITA_OPUS_LIB VITA_MODPLUG_LIB VITA_XMP_LIB VITA_FLAC_LIB VITA_BZ2_LIB)
  if(NOT ${_vita_req})
    message(FATAL_ERROR "Vita link closure: ${_vita_req} not found in ${_vita_libdir} - check vdpm installs in .github/workflows/build.yml")
  endif()
endforeach()

target_link_libraries(CorsixTH PRIVATE
  -Wl,--wrap=_stat_r
  -Wl,--wrap=_fstat_r
  -Wl,--start-group
  ${VITA_SDL2MIXER_LIB}
  ${VITA_VORBISFILE_LIB}
  ${VITA_VORBIS_LIB}
  ${VITA_OGG_LIB}
  ${VITA_OPUSFILE_LIB}
  ${VITA_OPUS_LIB}
  ${VITA_MODPLUG_LIB}
  ${VITA_XMP_LIB}
  ${VITA_FLAC_LIB}
  ${VITA_BZ2_LIB}
  ${VITA_SDL2_LIB}
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
  SceIme_stub
  SceTouch_stub
  SceHid_stub
  SceMotion_stub
  ScePower_stub
  SceProcessmgr_stub
  -Wl,--end-group
)

# Upstream links its own libfreetype.a AFTER our group above (transitive
# PUBLIC deps come after direct ones), so freetype's bzip2 dependency needs
# a second copy after it: static linking is one-pass, our group's bz2 was
# already processed by then. Appending to CorsixTH_lib's INTERFACE lands
# after upstream's freetype entry.
if(NOT TARGET CorsixTH_lib)
  message(FATAL_ERROR "Upstream target 'CorsixTH_lib' not found - check CORSIXTH_TAG layout.")
endif()
target_link_libraries(CorsixTH_lib PUBLIC ${VITA_BZ2_LIB})

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
