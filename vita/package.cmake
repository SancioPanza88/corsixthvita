# Packs eboot.bin + corsixth.vpk from an already-linked ARM ELF.
#
# Invoked by the `package-vpk` custom target, i.e. strictly after the full
# build has finished: every tool runs here in one process, in order, so the
# parallel-make ordering hazards of ALL-target packaging macros cannot occur
# by construction.
#
# Required -D flags: ELF_FILE, OUT_DIR, TITLEID, VERSION, APP_NAME, ART_LIST.
# ART_LIST points at a text file with one "src=dst" pair per line; pairs
# whose source is missing are skipped (default LiveArea art in that case).

cmake_minimum_required(VERSION 3.16)

foreach(arg ELF_FILE OUT_DIR TITLEID VERSION APP_NAME ART_LIST)
  if(NOT DEFINED ${arg} OR "${${arg}}" STREQUAL "")
    message(FATAL_ERROR "package.cmake: -D${arg}=... is required")
  endif()
endforeach()
if(NOT EXISTS "${ELF_FILE}")
  message(FATAL_ERROR "package.cmake: ELF not found: ${ELF_FILE}")
endif()

if(DEFINED ENV{VITASDK})
  list(APPEND CMAKE_PROGRAM_PATH "$ENV{VITASDK}/bin")
endif()
find_program(VITA_ELF_CREATE vita-elf-create REQUIRED)
find_program(VITA_MAKE_FSELF vita-make-fself REQUIRED)
find_program(VITA_MKSFOEX vita-mksfoex REQUIRED)
find_program(VITA_PACK_VPK vita-pack-vpk REQUIRED)

function(vita_run)
  execute_process(${ARGN} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "package.cmake failed: ${ARGN}\n--- stdout ---\n${_out}\n--- stderr ---\n${_err}")
  endif()
endfunction()

set(VELF "${OUT_DIR}/CorsixTH.velf")
set(EBOOT "${OUT_DIR}/eboot.bin")
set(SFO "${OUT_DIR}/param.sfo")
set(VPK "${OUT_DIR}/corsixth.vpk")

message(STATUS "package.cmake: elf-create")
vita_run(COMMAND "${VITA_ELF_CREATE}" "${ELF_FILE}" "${VELF}")

message(STATUS "package.cmake: make-fself (compressed, unsafe)")
vita_run(COMMAND "${VITA_MAKE_FSELF}" -c "${VELF}" "${EBOOT}")

message(STATUS "package.cmake: param.sfo")
vita_run(COMMAND "${VITA_MKSFOEX}" -s "APP_VER=${VERSION}" -s "TITLE_ID=${TITLEID}" "${APP_NAME}" "${SFO}")

set(PACK_ARGS -s "${SFO}" -b "${EBOOT}")
file(STRINGS "${ART_LIST}" ART_PAIRS)
foreach(pair IN LISTS ART_PAIRS)
  if(pair STREQUAL "")
    continue()
  endif()
  string(FIND "${pair}" "=" _eq)
  if(_eq EQUAL -1)
    message(FATAL_ERROR "package.cmake: malformed art pair: ${pair}")
  endif()
  string(SUBSTRING "${pair}" 0 ${_eq} _src)
  math(EXPR _dst_off "${_eq} + 1")
  string(SUBSTRING "${pair}" ${_dst_off} -1 _dst)
  if(EXISTS "${_src}")
    list(APPEND PACK_ARGS -a "${_src}=${_dst}")
  else()
    message(STATUS "package.cmake: art skipped (missing): ${_src}")
  endif()
endforeach()

message(STATUS "package.cmake: pack-vpk")
vita_run(COMMAND "${VITA_PACK_VPK}" ${PACK_ARGS} "${VPK}")
message(STATUS "package.cmake: wrote ${VPK}")
