find_package(GTest QUIET)
if(NOT TARGET GTest::gtest_main)
    include(FetchContent)
    set(BUILD_GMOCK OFF)
    set(INSTALL_GTEST OFF)
    FetchContent_Declare(
        googletest
        URL https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.18.0
        URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
    FetchContent_MakeAvailable(googletest)
endif()
include(GoogleTest)

function(pwbench_add_test suite timeout)
    add_executable(${suite}_test ${suite}_test.cpp)
    target_link_libraries(${suite}_test
        PRIVATE
            GTest::gtest_main
            pwbench::options
            ${ARGN}
    )
    gtest_discover_tests(${suite}_test
        DISCOVERY_MODE PRE_TEST
        PROPERTIES TIMEOUT ${timeout}
    )
endfunction()

foreach(suite IN ITEMS contracts cpu_executor registry wordlist)
    pwbench_add_test(${suite} 30 pwbench::core)
endforeach()

foreach(suite IN ITEMS app modules md5 argon2id)
    pwbench_add_test(${suite} 30 pwbench::${suite})
endforeach()

pwbench_add_test(cli 10)
add_dependencies(cli_test pwbench)
target_compile_definitions(cli_test PRIVATE "PWBENCH_CLI_PATH=\"$<TARGET_FILE:pwbench>\"")
