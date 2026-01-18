# Verify no .rdata section exists in the binary
# Usage: cmake -DMAP_FILE=<map_file> -P verify_no_rdata.cmake
#
# This verification ensures position-independence by confirming that no read-only
# data section (.rdata) was generated. The presence of .rdata would break PIC
# because it contains absolute addresses that cannot be relocated.

if(NOT DEFINED MAP_FILE)
    message(FATAL_ERROR "MAP_FILE must be defined")
endif()

# Map file may not exist for debug builds
if(NOT EXISTS "${MAP_FILE}")
    message(STATUS "Map file not found (expected for debug builds): ${MAP_FILE}")
    message(STATUS "Skipping .rdata verification")
    return()
endif()

# Read the map file
file(READ "${MAP_FILE}" MAP_CONTENT)

# Check for .rdata section (Windows PE format)
# We check for ANY .rdata section - both input and output sections
# Even if .rdata is merged into .text, its presence indicates problematic code generation
# Input sections have format: "segment:offset length name class" (e.g., "0001:0001fa80 00000040H .rdata CODE")
# We match any line with .rdata as a section name (not part of a longer name like .rdata.cst)
string(REGEX MATCH "[ \t]+[0-9a-fA-F]+:[0-9a-fA-F]+[ \t]+[0-9a-fA-F]+H[ \t]+\\.rdata[ \t]" RDATA_INPUT "${MAP_CONTENT}")

# Also check for output sections (format without segment:offset)
string(REGEX MATCH "[ \t]+[0-9a-fA-F]+[ \t]+[0-9a-fA-F]+[ \t]+[0-9a-fA-F]+[ \t]+[0-9]+[ \t]+\\.rdata[ \t]" RDATA_OUTPUT "${MAP_CONTENT}")

if(RDATA_INPUT)
    message(FATAL_ERROR
        "CRITICAL: .rdata input section detected in map file!\n"
        "Even though merged into .text, this indicates problematic code generation.\n"
        "The presence of ANY .rdata breaks position-independence. Check for:\n"
        "  - Static const variables with runtime initialization\n"
        "  - Jump tables from switch statements (use -fno-jump-tables)\n"
        "  - Floating-point constants not using EMBEDDED_DOUBLE\n"
        "  - String literals not using EMBEDDED_STRING\n"
        "  - RTTI or exception tables (use -fno-rtti -fno-exceptions)\n"
        "Map file: ${MAP_FILE}\n"
        "Matched line: ${RDATA_INPUT}"
    )
endif()

if(RDATA_OUTPUT)
    message(FATAL_ERROR
        "CRITICAL: .rdata OUTPUT section detected in binary!\n"
        "This breaks position-independence. Check for:\n"
        "  - Static const variables with runtime initialization\n"
        "  - Jump tables from switch statements (use -fno-jump-tables)\n"
        "  - RTTI or exception tables (use -fno-rtti -fno-exceptions)\n"
        "Map file: ${MAP_FILE}"
    )
endif()

# For ELF, check for .rodata as a top-level output section
# Internal .rodata.cst* sections get merged into .text by our linker script
# and don't create a separate .rodata output section
string(REGEX MATCH "[ \t]+[0-9a-fA-F]+[ \t]+[0-9a-fA-F]+[ \t]+[0-9a-fA-F]+[ \t]+[0-9]+[ \t]+\\.rodata[^.]" RODATA_OUTPUT "${MAP_CONTENT}")

if(RODATA_OUTPUT)
    message(FATAL_ERROR
        "CRITICAL: .rodata OUTPUT section detected in binary!\n"
        "This breaks position-independence. Check for:\n"
        "  - String literals (use EMBEDDED_STRING)\n"
        "  - Static const variables\n"
        "  - Jump tables from switch statements (use -fno-jump-tables)\n"
        "Map file: ${MAP_FILE}"
    )
endif()

message(STATUS "Verification PASSED: No .rdata/.rodata output sections found")
