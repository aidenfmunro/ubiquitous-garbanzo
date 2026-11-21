if(NOT DEFINED PROGRAM OR NOT DEFINED TEST_DIRECTORY)
    message(FATAL_ERROR "PROGRAM and TEST_DIRECTORY are required")
endif()

file(MAKE_DIRECTORY "${TEST_DIRECTORY}")
file(WRITE "${TEST_DIRECTORY}/words.txt" "alpha\nbeta\ngamma\n")
file(WRITE "${TEST_DIRECTORY}/empty-password.txt" "\n")
file(WRITE "${TEST_DIRECTORY}/empty-file.txt" "")

function(check_password password wordlist threads expected_code expected_pattern)
    execute_process(
        COMMAND "${PROGRAM}" --password "${password}" --wordlist "${wordlist}" --threads "${threads}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors
        TIMEOUT 10
    )
    if(NOT result STREQUAL expected_code)
        message(FATAL_ERROR "Expected exit ${expected_code}, got ${result}\n${output}${errors}")
    endif()
    set(combined "${output}${errors}")
    if(NOT combined MATCHES "${expected_pattern}")
        message(FATAL_ERROR "Missing output '${expected_pattern}'\n${combined}")
    endif()
    if(expected_code LESS 2)
        string(MD5 expected_hash "${password}")
        if(NOT output MATCHES "target_hash: ${expected_hash}")
            message(FATAL_ERROR "Incorrect target hash\n${output}")
        endif()
    endif()
endfunction()

check_password("beta" "${TEST_DIRECTORY}/words.txt" 1 0 "status: found")
check_password("beta" "${TEST_DIRECTORY}/words.txt" 4 0 "status: found")
check_password("absent" "${TEST_DIRECTORY}/words.txt" 1 1 "status: exhausted")
check_password("" "${TEST_DIRECTORY}/empty-password.txt" 1 0 "status: found")
check_password("beta" "${TEST_DIRECTORY}/empty-file.txt" 1 1 "checks: 0")
check_password("beta" "${TEST_DIRECTORY}/missing.txt" 1 2 "cannot open wordlist")
check_password("beta" "${TEST_DIRECTORY}/words.txt" 0 2 "--threads must be a positive integer")

execute_process(COMMAND "${PROGRAM}" --help
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 10)
if(NOT result STREQUAL "0" OR NOT output MATCHES "Usage: pwbench")
    message(FATAL_ERROR "Help failed: ${result}\n${output}${errors}")
endif()

execute_process(COMMAND "${PROGRAM}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 10)
if(NOT result STREQUAL "2" OR NOT errors MATCHES "--password is required")
    message(FATAL_ERROR "Missing password was not rejected: ${result}\n${output}${errors}")
endif()
