foreach(suite IN ITEMS contracts cpu_executor registry wordlist)
    add_executable(${suite}_test ${suite}_test.cpp)
    target_link_libraries(${suite}_test
        PRIVATE
            pwbench::core
            pwbench::options
    )

    add_test(NAME pwbench.${suite} COMMAND ${suite}_test)
    set_tests_properties(pwbench.${suite} PROPERTIES TIMEOUT 30)
endforeach()

add_executable(app_test app_test.cpp)
target_link_libraries(app_test
    PRIVATE
        pwbench::app
        pwbench::options
)
add_test(NAME pwbench.app COMMAND app_test)
set_tests_properties(pwbench.app PROPERTIES TIMEOUT 30)

add_executable(modules_test modules_test.cpp)
target_link_libraries(modules_test
    PRIVATE
        pwbench::modules
        pwbench::options
)
add_test(NAME pwbench.modules COMMAND modules_test)
set_tests_properties(pwbench.modules PROPERTIES TIMEOUT 30)

foreach(algorithm IN ITEMS md5 argon2id)
    add_executable(${algorithm}_test ${algorithm}_test.cpp)
    target_link_libraries(${algorithm}_test
        PRIVATE
            pwbench::${algorithm}
            pwbench::options
    )
    add_test(NAME pwbench.${algorithm} COMMAND ${algorithm}_test)

    add_test(
        NAME pwbench.${algorithm}_cli
        COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:pwbench>"
            "-DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/${algorithm}-cli-tests"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/${algorithm}_cli.cmake"
    )
    set_tests_properties(pwbench.${algorithm} pwbench.${algorithm}_cli
        PROPERTIES TIMEOUT 30
    )
endforeach()
