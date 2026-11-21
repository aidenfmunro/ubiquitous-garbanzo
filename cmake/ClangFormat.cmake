find_program(PWBENCH_CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(NOT PWBENCH_CLANG_FORMAT_EXECUTABLE)
    message(STATUS "clang-format not found; format and format-check targets are unavailable")
    return()
endif()

file(GLOB_RECURSE PWBENCH_FORMAT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*"
    "${PROJECT_SOURCE_DIR}/src/*"
    "${PROJECT_SOURCE_DIR}/tests/*"
)
list(FILTER PWBENCH_FORMAT_FILES INCLUDE REGEX "[.](h|hpp|cpp|cu|cuh)$")

add_custom_target(format
    COMMAND "${PWBENCH_CLANG_FORMAT_EXECUTABLE}" --style=file -i ${PWBENCH_FORMAT_FILES}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)

add_custom_target(format-check
    COMMAND "${PWBENCH_CLANG_FORMAT_EXECUTABLE}" --style=file --dry-run --Werror
        ${PWBENCH_FORMAT_FILES}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)
