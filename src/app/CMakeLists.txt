add_library(pwbench_app cli.cpp)
add_library(pwbench::app ALIAS pwbench_app)
target_compile_options(pwbench_app PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(pwbench_app PUBLIC pwbench::core)
target_compile_definitions(pwbench_app PRIVATE
    PWBENCH_DEFAULT_WORDLIST="${PWBENCH_WORDLIST_PATH}"
)
