find_package(CUDAToolkit REQUIRED)

add_library(pwbench_cuda batch.cpp runtime.cpp cuda_module.cpp)
add_library(pwbench::cuda ALIAS pwbench_cuda)
target_link_libraries(pwbench_cuda
    PUBLIC
        pwbench::core
        CUDA::cudart
    PRIVATE
        pwbench::options
)
