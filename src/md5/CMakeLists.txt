add_library(pwbench_md5 md5.cpp)
add_library(pwbench::md5 ALIAS pwbench_md5)
target_link_libraries(pwbench_md5
    PUBLIC
        pwbench::core
    PRIVATE
        pwbench::options
)
