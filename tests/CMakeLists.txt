foreach(suite IN ITEMS contracts cpu_executor registry wordlist)
    add_executable(${suite}_test ${suite}_test.cpp)
    target_compile_options(${suite}_test PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(${suite}_test PRIVATE pwbench::core)

    add_test(NAME pwbench.${suite} COMMAND ${suite}_test)
    set_tests_properties(pwbench.${suite} PROPERTIES TIMEOUT 30)
endforeach()

add_executable(app_test app_test.cpp)
target_compile_options(app_test PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(app_test PRIVATE pwbench::app)
add_test(NAME pwbench.app COMMAND app_test)
set_tests_properties(pwbench.app PROPERTIES TIMEOUT 30)
