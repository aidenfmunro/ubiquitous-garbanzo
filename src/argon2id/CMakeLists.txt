find_package(Argon2 REQUIRED)

add_library(pwbench_argon2id argon2id.cpp)
add_library(pwbench::argon2id ALIAS pwbench_argon2id)
target_link_libraries(pwbench_argon2id
    PUBLIC
        pwbench::core
    PRIVATE
        Argon2::Argon2
        pwbench::options
)

if(PWBENCH_ENABLE_CUDA)
    add_library(pwbench_argon2id_cuda argon2id.cu)
    add_library(pwbench::argon2id_cuda ALIAS pwbench_argon2id_cuda)
    set_target_properties(pwbench_argon2id_cuda PROPERTIES CUDA_RUNTIME_LIBRARY Shared)
    target_link_libraries(pwbench_argon2id_cuda
        PUBLIC pwbench::cuda
        PRIVATE pwbench::options
    )

    add_executable(argon2id_cuda_bench cuda_bench.cpp)
    target_link_libraries(argon2id_cuda_bench
        PRIVATE pwbench::argon2id_cuda pwbench::argon2id pwbench::options
    )
endif()
