if(NOT DEFINED PROGRAM OR NOT DEFINED TEST_DIRECTORY)
    message(FATAL_ERROR "PROGRAM and TEST_DIRECTORY are required")
endif()

file(MAKE_DIRECTORY "${TEST_DIRECTORY}")
file(WRITE "${TEST_DIRECTORY}/words.txt" "another-fixture\nbench-fixture\n")

function(check_run expected_code expected_text)
    execute_process(
        COMMAND "${PROGRAM}" --wordlist "${TEST_DIRECTORY}/words.txt" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors
        TIMEOUT 10
    )
    string(FIND "${output}${errors}" "${expected_text}" position)
    if(NOT result STREQUAL expected_code OR position EQUAL -1)
        message(FATAL_ERROR "Expected exit ${expected_code} and '${expected_text}', got ${result}\n${output}${errors}")
    endif()
endfunction()

set(argon2id --algorithm argon2id --memory-kib 16 --iterations 1 --lanes 1)
set(hash "$argon2id$v=19$m=16,t=1,p=1$cHdiZW5jaC1hcmdvbjJpZA$MV5AkayZpGl+DVUrqW9A7X3cN0W4LtlznnWUE8nS4Ks")
check_run(0 "target_hash: ${hash}\nstatus: found\ncandidate: \"bench-fixture\"\nchecks: 2\n"
          ${argon2id} --password bench-fixture)
check_run(1 "status: exhausted\nchecks: 2\n" ${argon2id} --password missing-fixture)
check_run(2 "Argon2id: Memory cost is too small" --algorithm argon2id
          --password bench-fixture --memory-kib 8 --lanes 2)
check_run(2 "--lanes must be a positive integer" --algorithm argon2id
          --password bench-fixture --lanes 4294967296)
check_run(2 "require --algorithm argon2id" --algorithm md5 --password bench-fixture --iterations 1)
