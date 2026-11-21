add_library(pwbench_app cli.cpp)
add_library(pwbench::app ALIAS pwbench_app)
target_compile_options(pwbench_app PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(pwbench_app PUBLIC pwbench::core)
target_compile_definitions(pwbench_app PRIVATE
    PWBENCH_DEFAULT_WORDLIST="${PWBENCH_WORDLIST_PATH}"
)

add_executable(pwbench main.cpp md5_experiment.cpp)
target_compile_options(pwbench PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(pwbench PRIVATE pwbench::app pwbench::md5)
if(PWBENCH_ENABLE_ARGON2ID)
    target_sources(pwbench PRIVATE argon2id_experiment.cpp)
    target_compile_definitions(pwbench PRIVATE PWBENCH_ENABLE_ARGON2ID)
    target_link_libraries(pwbench PRIVATE pwbench::argon2id)
endif()
set_target_properties(pwbench PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_dependencies(pwbench pwbench_wordlist)

if(BUILD_TESTING)
    add_test(NAME pwbench.md5_cli
        COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:pwbench>"
            "-DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/cli-tests"
            -P "${PROJECT_SOURCE_DIR}/tests/md5_cli.cmake"
    )
    set_tests_properties(pwbench.md5_cli PROPERTIES TIMEOUT 30)

    if(PWBENCH_ENABLE_ARGON2ID)
        add_test(NAME pwbench.argon2id_cli
            COMMAND "${CMAKE_COMMAND}"
                "-DPROGRAM=$<TARGET_FILE:pwbench>"
                "-DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/argon2id-cli-tests"
                -P "${PROJECT_SOURCE_DIR}/tests/argon2id_cli.cmake"
        )
        set_tests_properties(pwbench.argon2id_cli PROPERTIES TIMEOUT 30)
    endif()
endif()
