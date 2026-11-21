find_path(PWBENCH_ARGON2_INCLUDE_DIR NAMES argon2.h)
find_library(PWBENCH_ARGON2_LIBRARY NAMES argon2)
if(NOT PWBENCH_ARGON2_INCLUDE_DIR OR NOT PWBENCH_ARGON2_LIBRARY)
    message(FATAL_ERROR
        "Argon2id requires libargon2 and argon2.h. Install the development package "
        "or point CMAKE_PREFIX_PATH at the libargon2 installation."
    )
endif()

add_library(pwbench_argon2id argon2id.cpp)
add_library(pwbench::argon2id ALIAS pwbench_argon2id)
target_compile_options(pwbench_argon2id PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(pwbench_argon2id PRIVATE "${PWBENCH_ARGON2_INCLUDE_DIR}")
target_link_libraries(pwbench_argon2id PUBLIC pwbench::core PRIVATE "${PWBENCH_ARGON2_LIBRARY}")

if(BUILD_TESTING)
    add_executable(argon2id_test "${PROJECT_SOURCE_DIR}/tests/argon2id_test.cpp")
    target_compile_options(argon2id_test PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(argon2id_test PRIVATE pwbench::argon2id)
    add_test(NAME pwbench.argon2id COMMAND argon2id_test)
    set_tests_properties(pwbench.argon2id PROPERTIES TIMEOUT 30)
endif()
