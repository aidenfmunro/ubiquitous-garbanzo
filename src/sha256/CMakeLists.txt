add_library(pwbench_sha256 sha256.impl.cpp)
add_library(pwbench::sha256 ALIAS pwbench_sha256)
target_link_libraries(pwbench_sha256
    PUBLIC
        pwbench::core
    PRIVATE
        pwbench::options
)

if(PWBENCH_ENABLE_CUDA)
    add_library(pwbench_sha256_cuda sha256.impl.cu)
    add_library(pwbench::sha256_cuda ALIAS pwbench_sha256_cuda)
    set_target_properties(pwbench_sha256_cuda
        PROPERTIES CUDA_RUNTIME_LIBRARY Shared
    )
    target_link_libraries(pwbench_sha256_cuda
        PUBLIC
            pwbench::cuda
            pwbench::sha256
        PRIVATE
            pwbench::options
    )
endif()
