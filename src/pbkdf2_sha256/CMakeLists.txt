find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_library(pwbench_pbkdf2_sha256 pbkdf2_sha256.cpp)
add_library(pwbench::pbkdf2_sha256 ALIAS pwbench_pbkdf2_sha256)
target_link_libraries(pwbench_pbkdf2_sha256
    PUBLIC pwbench::core
    PRIVATE OpenSSL::Crypto pwbench::options
)
