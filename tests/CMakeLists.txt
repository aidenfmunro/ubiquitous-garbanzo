foreach(suite IN ITEMS contracts cpu_executor registry wordlist)
    add_executable(${suite}_test ${suite}_test.cpp)
    target_compile_options(${suite}_test PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(${suite}_test PRIVATE pwbench::core)

    add_test(NAME pwbench.${suite} COMMAND ${suite}_test)
    set_tests_properties(pwbench.${suite} PROPERTIES TIMEOUT 30)
endforeach()
