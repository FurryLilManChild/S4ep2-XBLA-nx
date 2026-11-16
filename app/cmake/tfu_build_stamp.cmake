# TFU-nx (2026-11-12): writes the build stamp header on every build (cmake -DOUT=<header> -DROOT=<repo> -DKIND=<pc|switch>
# -P tfu_build_stamp.cmake). Only when the text changes is the file rewritten, so only src/tfu_build_stamp.cpp recompiles.
# The stamp: kind, commit (with "+changes" when the tree has uncommitted edits) and the UTC build time. It is the first
# line of every log and crash report (sdk/src/ui/switch_crash_hooks.c), so each log is tied to its NRO and ELF.
execute_process(COMMAND git -C "${ROOT}" rev-parse --short=10 HEAD
                OUTPUT_VARIABLE _commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _r1)
if(NOT _r1 EQUAL 0 OR _commit STREQUAL "")
    set(_commit "no-git")
endif()
execute_process(COMMAND git -C "${ROOT}" status --porcelain --untracked-files=no
                OUTPUT_VARIABLE _dirty OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT _dirty STREQUAL "")
    string(APPEND _commit "+changes")
endif()
string(TIMESTAMP _time "%Y-%m-%d %H:%M:%S UTC" UTC)
set(_text "#define TFU_BUILD_STAMP \"S4ep1-nx ${KIND} build ${_commit}, built ${_time}\"\n")
set(_old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" _old)
endif()
if(NOT _old STREQUAL _text)
    file(WRITE "${OUT}" "${_text}")
endif()
