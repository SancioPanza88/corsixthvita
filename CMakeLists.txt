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
# If no python3 is around, package without custom art instead of failing
# the whole configure.
set(VITA_LIVEAREA_DIR "${CMAKE_BINARY_DIR}/sce_sys")
find_program(VITA_PYTHON python3)
set(VITA_LIVEAREA_FILES "")
if(VITA_PYTHON)
  execute_process(
    COMMAND "${VITA_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/vita/gen_livearea.py" "${VITA_LIVEAREA_DIR}"
    RESULT_VARIABLE _livearea_rc
  )
endif()
if(VITA_PYTHON AND _livearea_rc EQUAL 0)
  set(VITA_LIVEAREA_FILES
    FILE "${VITA_LIVEAREA_DIR}/icon0.png" sce_sys/icon0.png
    FILE "${VITA_LIVEAREA_DIR}/livearea/contents/bg.png" sce_sys/livearea/contents/bg.png
    FILE "${VITA_LIVEAREA_DIR}/livearea/contents/startup.png" sce_sys/livearea/contents/startup.png
    FILE "${CMAKE_CURRENT_SOURCE_DIR}/vita/sce_sys/livearea/contents/template.xml" sce_sys/livearea/contents/template.xml
  )
else()
  message(WARNING "LiveArea art skipped (no python3 or generator failed) - packaging with defaults.")
endif()

vita_create_self(eboot.bin CorsixTH UNSAFE)

# vita_create_self only wires the velf step to the executable by file path.
# Under parallel make that is not enough to serialize it after the (slow)
# link, so pin the order explicitly. Belt and braces; harmless if redundant.
if(TARGET CorsixTH-velf)
  add_dependencies(CorsixTH-velf CorsixTH)
endif()

vita_create_vpk(corsixth.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  ${VITA_LIVEAREA_FILES}
)
