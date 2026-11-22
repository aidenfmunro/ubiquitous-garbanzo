add_library(pwbench_modules md5.cpp argon2id.cpp wordlist.cpp)
add_library(pwbench::modules ALIAS pwbench_modules)
target_link_libraries(pwbench_modules
    PUBLIC
        pwbench::md5
        pwbench::argon2id
    PRIVATE
        pwbench::options
)
target_compile_definitions(pwbench_modules
    PRIVATE
        PWBENCH_DEFAULT_WORDLIST="${PWBENCH_WORDLIST_PATH}"
)
