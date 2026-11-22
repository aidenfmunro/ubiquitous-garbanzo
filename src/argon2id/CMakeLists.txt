find_package(Argon2 REQUIRED)

add_library(pwbench_argon2id argon2id.cpp)
add_library(pwbench::argon2id ALIAS pwbench_argon2id)
target_link_libraries(pwbench_argon2id PUBLIC pwbench::core PRIVATE Argon2::Argon2)
