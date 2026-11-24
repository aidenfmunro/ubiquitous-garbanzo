add_library(pwbench_md5 md5.cpp)
add_library(pwbench::md5 ALIAS pwbench_md5)
target_link_libraries(pwbench_md5
    PUBLIC
        pwbench::core
    PRIVATE
        pwbench::options
)

if(PWBENCH_ENABLE_CUDA)
    add_library(pwbench_md5_cuda md5.cu)
    add_library(pwbench::md5_cuda ALIAS pwbench_md5_cuda)
    set_target_properties(pwbench_md5_cuda
        PROPERTIES CUDA_RUNTIME_LIBRARY Shared
    )
    target_link_libraries(pwbench_md5_cuda
        PUBLIC
            pwbench::cuda
            pwbench::md5
        PRIVATE
            pwbench::options
    )
endif()
