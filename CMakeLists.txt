cmake_minimum_required(VERSION 3.24)
project(ubiquitous_garbanzo VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PWBENCH_ENABLE_CUDA "Enable the toolchain for future CUDA modules" OFF)
if(PWBENCH_ENABLE_CUDA)
    enable_language(CUDA)
endif()

include(cmake/CompilerOptions.cmake)
find_package(Threads REQUIRED)

add_library(pwbench_core
    src/cpu_run_state.cpp
    src/registry.cpp
    src/wordlist.cpp
)
add_library(pwbench::core ALIAS pwbench_core)

target_compile_features(pwbench_core PUBLIC cxx_std_20)
target_include_directories(pwbench_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(pwbench_core
    PUBLIC
        Threads::Threads
    PRIVATE
        pwbench::options
)

include(cmake/Wordlist.cmake)

add_subdirectory(src/md5)
add_subdirectory(src/argon2id)
add_subdirectory(src/modules)
add_subdirectory(src/app)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include(cmake/ClangFormat.cmake)
