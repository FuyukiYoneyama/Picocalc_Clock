if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

if(NOT DEFINED OUT_FILE)
    message(FATAL_ERROR "OUT_FILE is required")
endif()

if(NOT DEFINED BUILD_PURPOSE)
    set(BUILD_PURPOSE "unspecified")
endif()

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_HASH)
    set(GIT_HASH "unknown")
endif()

execute_process(
    COMMAND git diff --quiet
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE GIT_DIRTY_RESULT
    ERROR_QUIET
)

if(GIT_DIRTY_RESULT EQUAL 0)
    set(GIT_DIRTY "0")
else()
    set(GIT_DIRTY "1")
endif()

string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S %z")

get_filename_component(OUT_DIR "${OUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${OUT_DIR}")

file(WRITE "${OUT_FILE}" "#ifndef PICOCALC_CLOCK_BUILD_INFO_H\n")
file(APPEND "${OUT_FILE}" "#define PICOCALC_CLOCK_BUILD_INFO_H\n\n")
file(APPEND "${OUT_FILE}" "#define PICOCALC_CLOCK_GIT_HASH \"${GIT_HASH}\"\n")
file(APPEND "${OUT_FILE}" "#define PICOCALC_CLOCK_GIT_DIRTY ${GIT_DIRTY}\n")
file(APPEND "${OUT_FILE}" "#define PICOCALC_CLOCK_BUILD_TIME \"${BUILD_TIME}\"\n")
file(APPEND "${OUT_FILE}" "#define PICOCALC_CLOCK_BUILD_PURPOSE \"${BUILD_PURPOSE}\"\n\n")
file(APPEND "${OUT_FILE}" "#endif\n")
