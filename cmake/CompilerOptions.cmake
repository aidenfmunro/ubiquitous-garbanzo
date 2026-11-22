set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CUDA_EXTENSIONS OFF)

add_library(pwbench_options INTERFACE)
add_library(pwbench::options ALIAS pwbench_options)
target_compile_features(pwbench_options INTERFACE cxx_std_20)
if(PWBENCH_ENABLE_CUDA)
    target_compile_features(pwbench_options INTERFACE cuda_std_20)
endif()

target_compile_options(pwbench_options
    INTERFACE
        "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>"
)
