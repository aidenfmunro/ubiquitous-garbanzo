set(PWBENCH_WORDLIST_PATH
    "${PROJECT_BINARY_DIR}/data/Most-Popular-Letter-Passes.txt"
)

add_custom_target(pwbench_wordlist ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DWORDLIST_PATH=${PWBENCH_WORDLIST_PATH}"
        -P "${CMAKE_CURRENT_LIST_DIR}/DownloadWordlist.cmake"
    BYPRODUCTS "${PWBENCH_WORDLIST_PATH}"
    VERBATIM
)
