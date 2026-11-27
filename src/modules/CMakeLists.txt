add_library(pwbench_modules md5.cpp sha256.cpp argon2id.cpp pbkdf2_sha256.cpp wordlist.cpp)
add_library(pwbench::modules ALIAS pwbench_modules)
target_link_libraries(pwbench_modules
    PUBLIC
        pwbench::md5
        pwbench::sha256
        pwbench::argon2id
        pwbench::pbkdf2_sha256
    PRIVATE
        pwbench::options
)
if(PWBENCH_ENABLE_CUDA)
    target_compile_definitions(pwbench_modules PRIVATE PWBENCH_ENABLE_CUDA)
    target_link_libraries(pwbench_modules PUBLIC pwbench::md5_cuda pwbench::sha256_cuda)
endif()

target_compile_definitions(pwbench_modules
    PRIVATE
        PWBENCH_DEFAULT_WORDLIST="${PWBENCH_WORDLIST_PATH}"
)
