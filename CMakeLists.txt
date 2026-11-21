cmake_minimum_required(VERSION 3.24)
project(ubiquitous_garbanzo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PWBENCH_ENABLE_CUDA "Enable the toolchain for future CUDA modules" OFF)
if(PWBENCH_ENABLE_CUDA)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 20)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    set(CMAKE_CUDA_EXTENSIONS OFF)
endif()

find_package(Threads REQUIRED)

add_library(pwbench_core
    src/cpu_run_state.cpp
    src/registry.cpp
)
add_library(pwbench::core ALIAS pwbench_core)

target_compile_features(pwbench_core PUBLIC cxx_std_20)
target_compile_options(pwbench_core PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(pwbench_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(pwbench_core PUBLIC Threads::Threads)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include(cmake/ClangFormat.cmake)
