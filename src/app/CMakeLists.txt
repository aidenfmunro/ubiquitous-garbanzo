add_library(pwbench_app cli.cpp)
add_library(pwbench::app ALIAS pwbench_app)
target_link_libraries(pwbench_app
    PUBLIC
        pwbench::core
    PRIVATE
        pwbench::options
)

add_executable(pwbench main.cpp registrations.cpp)
target_link_libraries(pwbench
    PRIVATE
        pwbench::app
        pwbench::modules
        pwbench::options
)
if(PWBENCH_ENABLE_CUDA)
    target_compile_definitions(pwbench PRIVATE PWBENCH_ENABLE_CUDA)
endif()
add_dependencies(pwbench pwbench_wordlist)
