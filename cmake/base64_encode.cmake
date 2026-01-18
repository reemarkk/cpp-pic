# Base64 encode script - cross-platform
# Usage: cmake -DPIC_FILE=<input> -DBASE64_FILE=<output> -P base64_encode.cmake

if(NOT DEFINED PIC_FILE OR NOT DEFINED BASE64_FILE)
    message(FATAL_ERROR "PIC_FILE and BASE64_FILE must be defined")
endif()

if(NOT EXISTS "${PIC_FILE}")
    message(FATAL_ERROR "Input file does not exist: ${PIC_FILE}")
endif()

if(WIN32)
    # Use certutil on Windows
    execute_process(
        COMMAND certutil -encodehex -f "${PIC_FILE}" "${BASE64_FILE}" 0x40000001
        RESULT_VARIABLE RESULT
    )
else()
    # Use base64 on Unix-like systems
    # Try GNU base64 (with -w flag) first, fall back to BSD base64
    execute_process(
        COMMAND base64 -w 0 "${PIC_FILE}"
        OUTPUT_FILE "${BASE64_FILE}"
        ERROR_QUIET
        RESULT_VARIABLE RESULT
    )
    if(NOT RESULT EQUAL 0)
        # Fallback to BSD base64 (macOS) - output to variable then write
        execute_process(
            COMMAND base64 "${PIC_FILE}"
            OUTPUT_VARIABLE BASE64_CONTENT
            RESULT_VARIABLE RESULT
        )
        # Remove newlines from BSD base64 output
        string(REPLACE "\n" "" BASE64_CONTENT "${BASE64_CONTENT}")
        file(WRITE "${BASE64_FILE}" "${BASE64_CONTENT}")
    endif()
    # Add trailing newline
    file(APPEND "${BASE64_FILE}" "\n")
endif()

if(NOT RESULT EQUAL 0)
    message(WARNING "Base64 encoding may have failed with result: ${RESULT}")
endif()
