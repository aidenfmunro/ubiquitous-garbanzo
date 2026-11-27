find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_library(pwbench_pbkdf2_sha256 pbkdf2_sha256.impl.cpp)
add_library(pwbench::pbkdf2_sha256 ALIAS pwbench_pbkdf2_sha256)
target_link_libraries(pwbench_pbkdf2_sha256
    PUBLIC pwbench::core
    PRIVATE OpenSSL::Crypto pwbench::options
)

if(PWBENCH_ENABLE_CUDA)
    add_library(pwbench_pbkdf2_sha256_cuda pbkdf2_sha256.impl.cu)
    add_library(pwbench::pbkdf2_sha256_cuda ALIAS pwbench_pbkdf2_sha256_cuda)
    set_target_properties(pwbench_pbkdf2_sha256_cuda PROPERTIES CUDA_RUNTIME_LIBRARY Shared)
    target_link_libraries(pwbench_pbkdf2_sha256_cuda
        PUBLIC pwbench::cuda pwbench::pbkdf2_sha256
        PRIVATE pwbench::options
    )
endif()
