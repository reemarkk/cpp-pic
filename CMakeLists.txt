cmake_minimum_required(VERSION 3.20)

# This project requires the toolchain file to be used:
#   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-clang.cmake
#
# The toolchain file sets CMAKE_SYSTEM_NAME to Generic to avoid Windows/MSVC-specific
# CMake behavior that would otherwise add unwanted flags like -fuse-ld=lld-link and
# -Xclang --dependent-lib=msvcrtd.

if(NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR
        "This project requires a toolchain file. Please configure with:\n"
        "  cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-clang.cmake\n"
    )
endif()

project(cpp-pic LANGUAGES CXX)

# Configuration options
set(ARCHITECTURE "x86_64" CACHE STRING "Target architecture: i386, x86_64, armv7a, aarch64")
set(PLATFORM "windows" CACHE STRING "Target platform: windows, linux, uefi")
set(BUILD_TYPE "release" CACHE STRING "Build type: debug, release")
option(LAUNCH_QEMU "Enable QEMU launch targets for UEFI/Linux platforms" ON)

# Normalize inputs to lowercase (except BUILD_TYPE keeps original case for output dir)
string(TOLOWER "${ARCHITECTURE}" ARCHITECTURE_LC)
string(TOLOWER "${PLATFORM}" PLATFORM_LC)
string(TOLOWER "${BUILD_TYPE}" BUILD_TYPE_LC)
# Preserve original BUILD_TYPE case for output directory (DEBUG vs release)
set(BUILD_TYPE_DIR "${BUILD_TYPE}")

# Validate inputs
set(VALID_ARCHITECTURES "i386" "x86_64" "armv7a" "aarch64")
set(VALID_PLATFORMS "windows" "linux" "uefi")
set(VALID_BUILD_TYPES "debug" "release")

if(NOT ARCHITECTURE_LC IN_LIST VALID_ARCHITECTURES)
    message(FATAL_ERROR "Invalid ARCHITECTURE: ${ARCHITECTURE}. Must be one of: ${VALID_ARCHITECTURES}")
endif()
if(NOT PLATFORM_LC IN_LIST VALID_PLATFORMS)
    message(FATAL_ERROR "Invalid PLATFORM: ${PLATFORM}. Must be one of: ${VALID_PLATFORMS}")
endif()
if(NOT BUILD_TYPE_LC IN_LIST VALID_BUILD_TYPES)
    message(FATAL_ERROR "Invalid BUILD_TYPE: ${BUILD_TYPE}. Must be one of: ${VALID_BUILD_TYPES}")
endif()

# Set output directory - hierarchical structure: build/platform/arch/buildtype/
# Artifacts go to root, CMake files go to cmake/ subdirectory
# This must be defined early so it can be used in linker flags
set(BUILD_ROOT "${CMAKE_SOURCE_DIR}/build/${PLATFORM_LC}/${ARCHITECTURE_LC}/${BUILD_TYPE_LC}")
set(OUTPUT_DIR "${BUILD_ROOT}")

# Set optimization level based on build type
if(BUILD_TYPE_LC STREQUAL "debug")
    # Debug optimization flags:
    #   -Og              - Optimize for debugging experience (minimal optimizations that don't hurt debugging)
    #   -ferror-limit=200 - Show up to 200 errors before stopping (helpful during development)
    set(OPTIMIZATION_FLAGS "-Og" "-ferror-limit=200")
else()
    # Release optimization flag:
    #   -O3 - Maximum optimization level (aggressive inlining, vectorization, loop opts)
    set(OPTIMIZATION_FLAGS "-O3")
endif()

# Base compiler flags (all platforms/architectures)
# These flags are essential for generating position-independent code without .rdata dependencies
set(BASE_COMPILER_FLAGS
    -std=c++23                              # C++23 standard for consteval, concepts, fold expressions
    -fno-jump-tables                        # CRITICAL: Prevent switch statements from generating jump tables in .rdata
    -Werror                                 # Treat all warnings as errors for strict code quality
    -Wall                                   # Enable all common compiler warnings
    -Wextra                                 # Enable additional warnings beyond -Wall
    -Wno-gnu-string-literal-operator-template # Suppress warning for GNU extension used in _embed string literals
    -Qn                                     # Suppress compiler identification in output (reduces binary fingerprint)
    -nostdlib                               # Do not link standard C/C++ libraries (no CRT dependency)
    -fno-ident                              # Do not emit .comment section with compiler version strings
    -fno-exceptions                         # Disable C++ exceptions (no .pdata/.xdata unwind tables)
    -fno-rtti                               # Disable RTTI (no typeinfo/vtable references in .rdata)
    -fno-stack-check                        # Disable stack limit checking code generation
    -ffunction-sections                     # Place each function in its own section for linker dead-code elimination
    -fdata-sections                         # Place each data item in its own section for linker garbage collection
    -fno-builtin                            # Disable compiler built-in replacements (ensures our implementations are used)
    -fshort-wchar                           # Use 2-byte wchar_t (matches Windows ABI for wide strings)
)

# x86-specific compiler flags (not valid on ARM architectures)
# Note: -msoft-float is only used for Windows/UEFI platforms, not Linux
# On Linux, -msoft-float breaks varargs ABI for double parameters
if(ARCHITECTURE_LC STREQUAL "i386" OR ARCHITECTURE_LC STREQUAL "x86_64")
    list(APPEND BASE_COMPILER_FLAGS
        -mno-stack-arg-probe                # Disable Windows stack probing (__chkstk calls) - x86 only
    )
    # Only use -msoft-float on Windows and UEFI platforms
    # Linux x86_64 requires hardware float ABI for proper varargs compatibility
    if(PLATFORM_LC STREQUAL "windows" OR PLATFORM_LC STREQUAL "uefi")
        list(APPEND BASE_COMPILER_FLAGS
            -msoft-float                    # Use software floating-point ABI (no FPU register dependencies)
        )
    endif()
endif()

# ARM-specific compiler flags (aarch64 and armv7a)
if(ARCHITECTURE_LC STREQUAL "armv7a" OR ARCHITECTURE_LC STREQUAL "aarch64")
    list(APPEND BASE_COMPILER_FLAGS
        -mno-implicit-float                 # Disable implicit generation of floating-point instructions
    )
endif()

# Base linker flags (all platforms/architectures)
set(BASE_LINKER_FLAGS
    -fuse-ld=lld                            # Use LLVM's LLD linker (fast, supports all target platforms)
    -nostdlib                               # Do not link standard libraries (no CRT startup code)
)

# Platform-specific configuration
if(PLATFORM_LC STREQUAL "windows")
    set(INCLUDE_PATHS
        ${CMAKE_SOURCE_DIR}/
        ${CMAKE_SOURCE_DIR}/include/
        ${CMAKE_SOURCE_DIR}/include/runtime/platform/
        ${CMAKE_SOURCE_DIR}/include/runtime/platform/windows/
        ${CMAKE_SOURCE_DIR}/include/runtime/
        ${CMAKE_SOURCE_DIR}/include/runtime/primitives/
        ${CMAKE_SOURCE_DIR}/tests/
    )
    file(GLOB_RECURSE SOURCES
        ${CMAKE_SOURCE_DIR}/src/*.cc
    )
    # Exclude linux sources
    list(FILTER SOURCES EXCLUDE REGEX ".*/linux/.*")
    set(OUTPUT_EXT ".exe")
    # Windows linker flags (passed to LLD via -Wl):
    #   /Entry:_start        - Use _start as entry point (bypass CRT initialization)
    #   /SUBSYSTEM:CONSOLE   - Console application (allocates console window)
    #   /ORDER:@orderfile    - Control function placement order in .text section
    #   /MERGE:.rdata=.text  - CRITICAL: Merge read-only data into code section for PIC
    #                          (LTO may generate .rdata constants that would break position-independence)
    set(LINKER_BASE "-Wl,/Entry:_start,/SUBSYSTEM:CONSOLE,/ORDER:@${CMAKE_SOURCE_DIR}/orderfile.txt,/MERGE:.rdata=.text")

    # Architecture-specific configuration for Windows
    if(ARCHITECTURE_LC STREQUAL "i386")
        set(TARGET_TRIPLE "i386-pc-windows-gnu")
        set(ARCH_DEFINES ARCHITECTURE_I386 PLATFORM_WINDOWS PLATFORM_WINDOWS_I386)
        # i386-specific linker flags:
        #   /BASE:0x400000    - Preferred load address (standard for 32-bit Windows EXEs)
        #   /FILEALIGN:0x1000 - File section alignment (4KB pages)
        set(LINKER_EXTRA ",/BASE:0x400000,/FILEALIGN:0x1000,/SAFESEH:NO")
    elseif(ARCHITECTURE_LC STREQUAL "x86_64")
        set(TARGET_TRIPLE "x86_64-pc-windows-gnu")
        set(ARCH_DEFINES ARCHITECTURE_X86_64 PLATFORM_WINDOWS PLATFORM_WINDOWS_X86_64)
        set(LINKER_EXTRA "")
    elseif(ARCHITECTURE_LC STREQUAL "armv7a")
        set(TARGET_TRIPLE "armv7a-pc-windows-gnu")
        set(ARCH_DEFINES ARCHITECTURE_ARMV7A PLATFORM_WINDOWS PLATFORM_WINDOWS_ARMV7A)
        set(LINKER_EXTRA "")
    elseif(ARCHITECTURE_LC STREQUAL "aarch64")
        set(TARGET_TRIPLE "aarch64-pc-windows-gnu")
        set(ARCH_DEFINES ARCHITECTURE_AARCH64 PLATFORM_WINDOWS PLATFORM_WINDOWS_AARCH64)
        set(LINKER_EXTRA "")
    endif()

    # Build type specific flags for Windows
    if(BUILD_TYPE_LC STREQUAL "debug")
        list(APPEND ARCH_DEFINES DEBUG)
        # Debug compiler flags:
        #   -gcodeview           - Generate CodeView debug info (for Windows debuggers like WinDbg/VS)
        #   -g3                  - Maximum debug information level (includes macro definitions)
        #   -fno-omit-frame-pointer - Keep frame pointer for accurate stack traces
        list(APPEND BASE_COMPILER_FLAGS -gcodeview -g3 -fno-omit-frame-pointer)
        # Debug linker flags:
        #   /DEBUG - Include debug information in PE file
        #   /MAP   - Generate linker map file for symbol analysis
        set(LINKER_FLAGS "${LINKER_BASE}${LINKER_EXTRA},/DEBUG,/MAP:${OUTPUT_DIR}/output.map.txt")
    else()
        # Release compiler flags for maximum optimization:
        list(APPEND BASE_COMPILER_FLAGS
            -fno-omit-frame-pointer             # Keep frame pointer (useful for profiling/crash analysis)
            -fno-asynchronous-unwind-tables     # No .eh_frame section (reduces binary size)
            -fno-unwind-tables                  # No unwind tables (exceptions disabled anyway)
            -flto=full                          # Full link-time optimization (cross-module inlining)
            -finline-functions                  # Aggressively inline functions for performance
            -funroll-loops                      # Unroll loops to reduce branch overhead
            -fwhole-program-vtables             # LTO optimization for devirtualization
        )
        # Release linker flags:
        #   --strip-all - Remove all symbol and debug information
        #   /OPT:REF    - Remove unreferenced functions and data (dead code elimination)
        #   /OPT:ICF    - Fold identical code sequences (reduces size)
        #   /RELEASE    - Set release flag in PE header (optimizes loading)
        #   /LTCG       - Link-time code generation (works with -flto)
        #   /MAP        - Generate linker map file for analysis
        set(LINKER_FLAGS "${LINKER_BASE}${LINKER_EXTRA},--strip-all,/OPT:REF,/OPT:ICF,/RELEASE,/LTCG,/MAP:${OUTPUT_DIR}/output.map.txt")
    endif()

elseif(PLATFORM_LC STREQUAL "linux")
    set(INCLUDE_PATHS
        ${CMAKE_SOURCE_DIR}/
        ${CMAKE_SOURCE_DIR}/include/
        ${CMAKE_SOURCE_DIR}/include/runtime/platform/
        ${CMAKE_SOURCE_DIR}/include/runtime/
        ${CMAKE_SOURCE_DIR}/include/runtime/primitives/
        ${CMAKE_SOURCE_DIR}/tests/
    )
    file(GLOB_RECURSE SOURCES
        ${CMAKE_SOURCE_DIR}/src/*.cc
    )
    # Exclude windows sources
    list(FILTER SOURCES EXCLUDE REGEX ".*/windows/.*")
    set(OUTPUT_EXT ".elf")
    # Linux linker flags (passed to LLD via -Wl):
    #   -T linker.script - Use custom linker script for section layout control
    #   -e _start        - Specify _start as the entry point symbol
    set(LINKER_BASE "-Wl,-T,${CMAKE_SOURCE_DIR}/linker.script,-e,_start")

    # Architecture-specific configuration for Linux
    if(ARCHITECTURE_LC STREQUAL "i386")
        set(TARGET_TRIPLE "i386-unknown-linux-gnu")
        set(ARCH_DEFINES ARCHITECTURE_I386 PLATFORM_LINUX PLATFORM_LINUX_I386)
    elseif(ARCHITECTURE_LC STREQUAL "x86_64")
        set(TARGET_TRIPLE "x86_64-unknown-linux-gnu")
        set(ARCH_DEFINES ARCHITECTURE_X86_64 PLATFORM_LINUX PLATFORM_LINUX_X86_64)
    elseif(ARCHITECTURE_LC STREQUAL "armv7a")
        set(TARGET_TRIPLE "armv7a-unknown-linux-gnueabi")
        set(ARCH_DEFINES ARCHITECTURE_ARMV7A PLATFORM_LINUX PLATFORM_LINUX_ARMV7A)
    elseif(ARCHITECTURE_LC STREQUAL "aarch64")
        set(TARGET_TRIPLE "aarch64-unknown-linux-gnu")
        set(ARCH_DEFINES ARCHITECTURE_AARCH64 PLATFORM_LINUX PLATFORM_LINUX_AARCH64)
    endif()

    # Build type specific flags for Linux
    if(BUILD_TYPE_LC STREQUAL "debug")
        list(APPEND ARCH_DEFINES DEBUG)
        # Debug compiler flags:
        #   -g3                    - Maximum debug information (DWARF format on Linux)
        #   -fno-omit-frame-pointer - Keep frame pointer for stack traces
        list(APPEND BASE_COMPILER_FLAGS -g3 -fno-omit-frame-pointer)
        set(LINKER_FLAGS "${LINKER_BASE}")
    else()
        # Release compiler flags for Linux:
        list(APPEND BASE_COMPILER_FLAGS
            -fno-asynchronous-unwind-tables     # No .eh_frame section (reduces binary size)
            -fno-unwind-tables                  # No unwind tables (no exceptions)
            -g                                  # Include debug info (useful for post-mortem analysis)
            -flto                               # Link-time optimization (enables cross-TU inlining)
        )
        # Release linker flags:
        #   --gc-sections - Garbage collect unused sections (works with -ffunction-sections)
        #   --strip-all   - Remove all symbol and debug information from output
        #   -Map          - Generate linker map file for symbol/size analysis
        set(LINKER_FLAGS "${LINKER_BASE},--gc-sections,--strip-all,-Map=${OUTPUT_DIR}/output.map.txt")
    endif()

elseif(PLATFORM_LC STREQUAL "uefi")
    set(INCLUDE_PATHS
        ${CMAKE_SOURCE_DIR}/
        ${CMAKE_SOURCE_DIR}/include/
        ${CMAKE_SOURCE_DIR}/include/runtime/platform/
        ${CMAKE_SOURCE_DIR}/include/runtime/platform/uefi/
        ${CMAKE_SOURCE_DIR}/include/runtime/
        ${CMAKE_SOURCE_DIR}/include/runtime/primitives/
        ${CMAKE_SOURCE_DIR}/tests/
    )
    file(GLOB_RECURSE SOURCES
        ${CMAKE_SOURCE_DIR}/src/*.cc
    )
    # Exclude windows and linux sources
    list(FILTER SOURCES EXCLUDE REGEX ".*/windows/.*")
    list(FILTER SOURCES EXCLUDE REGEX ".*/linux/.*")
    set(OUTPUT_EXT ".efi")

    # UEFI linker flags:
    #   /Entry:_start   - UEFI entry point (uses same _start as Windows/Linux)
    #   /SUBSYSTEM:EFI_APPLICATION - UEFI application subsystem
    set(LINKER_BASE "-Wl,/Entry:_start,/SUBSYSTEM:EFI_APPLICATION")

    # Architecture-specific configuration for UEFI
    # Note: Only x86_64-unknown-uefi is officially supported in LLVM/Clang.
    # For i386 and aarch64, we use *-unknown-windows as a compatible fallback
    # since UEFI uses PE/COFF format (Windows-compatible) and similar ABI.
    if(ARCHITECTURE_LC STREQUAL "i386")
        set(TARGET_TRIPLE "i386-unknown-windows")
        set(ARCH_DEFINES ARCHITECTURE_I386 PLATFORM_UEFI PLATFORM_UEFI_I386)
        set(LINKER_EXTRA "")
    elseif(ARCHITECTURE_LC STREQUAL "x86_64")
        set(TARGET_TRIPLE "x86_64-unknown-uefi")
        set(ARCH_DEFINES ARCHITECTURE_X86_64 PLATFORM_UEFI PLATFORM_UEFI_X86_64)
        set(LINKER_EXTRA "")
    elseif(ARCHITECTURE_LC STREQUAL "aarch64")
        set(TARGET_TRIPLE "aarch64-unknown-windows")
        set(ARCH_DEFINES ARCHITECTURE_AARCH64 PLATFORM_UEFI PLATFORM_UEFI_AARCH64)
        set(LINKER_EXTRA "")
    endif()

    # Build type specific flags for UEFI
    if(BUILD_TYPE_LC STREQUAL "debug")
        list(APPEND ARCH_DEFINES DEBUG)
        list(APPEND BASE_COMPILER_FLAGS -gcodeview -g3 -fno-omit-frame-pointer)
        set(LINKER_FLAGS "${LINKER_BASE}${LINKER_EXTRA},/DEBUG,/MAP:${OUTPUT_DIR}/output.map.txt")
    else()
        list(APPEND BASE_COMPILER_FLAGS
            -fno-omit-frame-pointer
            -fno-asynchronous-unwind-tables
            -fno-unwind-tables
            -flto=full
            -finline-functions
            -funroll-loops
            -fwhole-program-vtables
        )
        set(LINKER_FLAGS "${LINKER_BASE}${LINKER_EXTRA},--strip-all,/OPT:REF,/OPT:ICF,/RELEASE,/LTCG,/MAP:${OUTPUT_DIR}/output.map.txt")
    endif()
endif()

# Create output directory and set CMake output paths
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_PDB_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}")
set(CMAKE_PDB_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}")

# Create the executable
add_executable(${TARGET_TRIPLE} ${SOURCES})

# Set target properties - use simple "output" name since directory structure provides context
set_target_properties(${TARGET_TRIPLE} PROPERTIES
    OUTPUT_NAME "output"
    SUFFIX "${OUTPUT_EXT}"
)

# Include directories
target_include_directories(${TARGET_TRIPLE} PRIVATE ${INCLUDE_PATHS})

# Compile definitions
target_compile_definitions(${TARGET_TRIPLE} PRIVATE ${ARCH_DEFINES})

# Compile options
target_compile_options(${TARGET_TRIPLE} PRIVATE
    ${BASE_COMPILER_FLAGS}
    ${OPTIMIZATION_FLAGS}
    -target ${TARGET_TRIPLE}
)

# Link options
target_link_options(${TARGET_TRIPLE} PRIVATE
    -target ${TARGET_TRIPLE}
    ${BASE_LINKER_FLAGS}
    "SHELL:${LINKER_FLAGS}"
)

# Post-build commands for generating PIC blob and analysis files
set(OUTPUT_EXE "${OUTPUT_DIR}/output${OUTPUT_EXT}")
set(OUTPUT_BIN "${OUTPUT_DIR}/output.bin")
set(OUTPUT_TXT "${OUTPUT_DIR}/output.txt")
set(OUTPUT_STR "${OUTPUT_DIR}/output.strings.txt")
set(OUTPUT_B64 "${OUTPUT_DIR}/output.b64.txt")
set(OUTPUT_MAP "${OUTPUT_DIR}/output.map.txt")

add_custom_command(TARGET ${TARGET_TRIPLE} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Build SUCCEEDED: ${OUTPUT_EXE}"

    # Generate object dump
    COMMAND llvm-objdump -d -s -h -j .text ${OUTPUT_EXE} > ${OUTPUT_TXT}

    # Extract .text section as PIC blob
    COMMAND llvm-objcopy "--dump-section=.text=${OUTPUT_BIN}" ${OUTPUT_EXE}

    # Generate strings file
    COMMAND llvm-strings ${OUTPUT_EXE} > ${OUTPUT_STR}

    # Base64 encode the PIC blob (platform-specific)
    COMMAND ${CMAKE_COMMAND} -DPIC_FILE=${OUTPUT_BIN} -DBASE64_FILE=${OUTPUT_B64} -P "${CMAKE_SOURCE_DIR}/cmake/base64_encode.cmake"

    # Verify no .rdata section exists (critical for PIC - would break position-independence)
    COMMAND ${CMAKE_COMMAND} -DMAP_FILE=${OUTPUT_MAP} -P "${CMAKE_SOURCE_DIR}/cmake/verify_no_rdata.cmake"

    COMMENT "Generating PIC blob and analysis files..."
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Build Configuration ===")
message(STATUS "ARCHITECTURE:     ${ARCHITECTURE_LC}")
message(STATUS "PLATFORM:         ${PLATFORM_LC}")
message(STATUS "BUILD_TYPE:       ${BUILD_TYPE_LC}")
message(STATUS "TARGET_TRIPLE:    ${TARGET_TRIPLE}")
message(STATUS "OUTPUT_DIR:       ${OUTPUT_DIR}")
message(STATUS "OUTPUT:           output${OUTPUT_EXT}")
message(STATUS "LAUNCH_QEMU:      ${LAUNCH_QEMU}")
message(STATUS "===========================")
message(STATUS "")

# Include QEMU launch support for UEFI and Linux builds
if(LAUNCH_QEMU AND (PLATFORM_LC STREQUAL "uefi" OR PLATFORM_LC STREQUAL "linux"))
    include(${CMAKE_SOURCE_DIR}/cmake/launch_qemu.cmake)
endif()
