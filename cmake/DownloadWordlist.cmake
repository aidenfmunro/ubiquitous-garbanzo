if(NOT DEFINED WORDLIST_PATH OR WORDLIST_PATH STREQUAL "")
    message(FATAL_ERROR "WORDLIST_PATH is required")
endif()

set(revision "08f12147a315be88a4710edd8e0246c36ed4649f")
set(expected_sha256
    "141a1ba1d2aabcb9069481d6aaa95159a3c0268e297ecedc40a30b6613d36b60"
)
set(url
    "https://raw.githubusercontent.com/danielmiessler/SecLists/${revision}/Passwords/Most-Popular-Letter-Passes.txt"
)

if(EXISTS "${WORDLIST_PATH}")
    file(SHA256 "${WORDLIST_PATH}" actual_sha256)
    if(actual_sha256 STREQUAL expected_sha256)
        return()
    endif()
endif()

get_filename_component(directory "${WORDLIST_PATH}" DIRECTORY)
file(MAKE_DIRECTORY "${directory}")
set(temporary_path "${WORDLIST_PATH}.part")

file(DOWNLOAD "${url}" "${temporary_path}"
    TLS_VERIFY ON
    TIMEOUT 60
    INACTIVITY_TIMEOUT 15
    STATUS download_status
)
list(GET download_status 0 status_code)
list(GET download_status 1 status_message)
if(NOT status_code EQUAL 0)
    file(REMOVE "${temporary_path}")
    message(FATAL_ERROR
        "Cannot download wordlist: ${status_message}. The first build requires network access."
    )
endif()

file(SHA256 "${temporary_path}" actual_sha256)
if(NOT actual_sha256 STREQUAL expected_sha256)
    file(REMOVE "${temporary_path}")
    message(FATAL_ERROR
        "Wordlist SHA-256 mismatch: expected ${expected_sha256}, got ${actual_sha256}"
    )
endif()

file(RENAME "${temporary_path}" "${WORDLIST_PATH}")
message(STATUS "Downloaded and verified Most-Popular-Letter-Passes.txt")
