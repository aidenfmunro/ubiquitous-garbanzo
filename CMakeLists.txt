cmake_minimum_required(VERSION 3.24)
project(ubiquitous_garbanzo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PWBENCH_ENABLE_CUDA "Enable the toolchain for future CUDA modules" OFF)

if(PWBENCH_ENABLE_CUDA)
    include(CheckLanguage)
    check_language(CUDA)
    if(NOT CMAKE_CUDA_COMPILER)
        message(FATAL_ERROR
            "CUDA was requested, but no CUDA compiler was found. "
            "Set CUDACXX or configure with -DPWBENCH_ENABLE_CUDA=OFF.")
    endif()
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
set_target_properties(pwbench_core PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(pwbench_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pwbench_core PUBLIC Threads::Threads)

function(pwbench_warnings target)
    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>")
endfunction()
pwbench_warnings(pwbench_core)

find_program(PWBENCH_CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(PWBENCH_CLANG_FORMAT_EXECUTABLE)
    set(PWBENCH_FORMAT_FILES)
    foreach(directory IN ITEMS include src tests)
        file(GLOB_RECURSE directory_files CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/*.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/*.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/*.cu"
            "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/*.cuh"
        )
        list(APPEND PWBENCH_FORMAT_FILES ${directory_files})
    endforeach()
    add_custom_target(format
        COMMAND "${PWBENCH_CLANG_FORMAT_EXECUTABLE}" --style=file -i ${PWBENCH_FORMAT_FILES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND "${PWBENCH_CLANG_FORMAT_EXECUTABLE}" --style=file --dry-run --Werror
            ${PWBENCH_FORMAT_FILES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM
    )
else()
    message(STATUS "clang-format not found; format and format-check targets are unavailable")
endif()

include(CTest)
if(BUILD_TESTING)
    foreach(suite IN ITEMS contracts cpu_executor registry)
        add_executable(${suite}_test tests/${suite}_test.cpp)
        target_link_libraries(${suite}_test PRIVATE pwbench::core)
        set_target_properties(${suite}_test PROPERTIES CXX_EXTENSIONS OFF)
        pwbench_warnings(${suite}_test)
        add_test(NAME pwbench.${suite} COMMAND ${suite}_test)
        set_tests_properties(pwbench.${suite} PROPERTIES TIMEOUT 30)
    endforeach()
endif()
