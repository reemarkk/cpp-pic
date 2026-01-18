# CPP-PIC Architecture Documentation

## Overview

CPP-PIC is a position-independent code (PIC) runtime library that provides a minimal, Windows-only runtime environment without dependencies on standard libraries or dynamic linking.

## Architecture Layers

```
┌─────────────────────────────────────────────┐
│      Application (Test Suite)               │
├─────────────────────────────────────────────┤
│   Embedded Types & Primitives               │
│   (String, Double, Int64, Uint64)           │
├─────────────────────────────────────────────┤
│   Runtime Utilities                         │
│   (Logger, Console, Memory, String)         │
├─────────────────────────────────────────────┤
│   Platform Abstraction Layer                │
│   (Allocator, Console, Platform Init)       │
├─────────────────────────────────────────────┤
│   Windows Platform Implementation           │
├─────────────────────────────────────────────┤
│   Entry Point (_start)                      │
└─────────────────────────────────────────────┘
```

## Core Components

### 1. Entry Point (`src/start.cc`)

The entry point initializes the runtime environment and executes the test suite:

```cpp
INT32 _start(VOID)
```

### 2. Platform Abstraction Layer

Generic interfaces that delegate to Windows-specific implementations:

- **Allocator** ([include/runtime/platform/allocator.h](../include/runtime/platform/allocator.h))
  - Memory allocation/deallocation interface

- **Platform** ([include/runtime/platform/platform.h](../include/runtime/platform/platform.h))
  - Initialization and environment setup

- **Console** ([include/runtime/console.h](../include/runtime/console.h))
  - Text output abstraction

### 3. Windows Platform Implementation

Located in `src/runtime/platform/windows/`:

- **PEB Walking** ([peb.cc](../src/runtime/platform/windows/peb.cc)) - Locates process environment block
- **PE Parsing** ([pe.cc](../src/runtime/platform/windows/pe.cc)) - Reads PE file structures for API resolution
- **API Resolution** ([ntdll.cc](../src/runtime/platform/windows/ntdll.cc), [kernel32.cc](../src/runtime/platform/windows/kernel32.cc))
  - Hash-based API lookup via DJB2 without GetProcAddress
- **Direct Syscalls** - `NtAllocateVirtualMemory`, `NtFreeVirtualMemory`
- **Console Output** - `WriteConsoleW` for Unicode text output

### 4. Embedded Primitives

Position-independent type implementations:

- **EMBEDDED_STRING** ([embedded_string.h](../include/runtime/platform/primitives/embedded_string.h))
  - Compile-time string decomposition into character literals
  - Avoids .rdata section by building strings on stack at runtime

- **EMBEDDED_DOUBLE** ([embedded_double.h](../include/runtime/platform/primitives/embedded_double.h))
  - IEEE-754 double as bit pattern decomposition
  - Embeds as 64-bit immediate values instead of .rdata constants

- **UINT64/INT64** ([uint64.h](../include/runtime/platform/primitives/uint64.h), [int64.h](../include/runtime/platform/primitives/int64.h))
  - Software-implemented 64-bit arithmetic (for 32-bit platforms)
  - Pure 32-bit word manipulation without native 64-bit instructions

- **DOUBLE** ([double.h](../include/runtime/platform/primitives/double.h))
  - IEEE-754 operations and integer conversions
  - Software floating-point implementation

### 5. Runtime Utilities

- **Logger** ([logger.h](../include/runtime/logger.h)) - Formatted logging output
- **Memory** ([memory.h](../include/runtime/memory.h)) - Memory operations (Copy, Zero, Compare)
- **String** ([string.h](../include/runtime/string.h)) - String manipulation
- **StringFormatter** ([string_formatter.h](../include/runtime/string_formatter.h)) - Printf-style formatting
- **DJB2** ([djb2.h](../include/runtime/djb2.h)) - Hash function for symbol lookup

## Position Independence Strategy

### Compiler Flags

```cmake
# Prevent jump tables and RTTI
-fno-jump-tables
-fno-rtti
-fno-exceptions

# Windows ABI compatibility
-fshort-wchar

# Software floating-point (x86)
-msoft-float
```

### Linker Configuration

**Windows**:
```
/MERGE:.rdata=.text    # Merge read-only data into code section
/Entry:_start          # Custom entry point
/ORDER:@orderfile.txt  # Control function placement
```

### No External Dependencies

- **No CRT** - Custom entry point bypasses C runtime
- **No Standard Library** - All functionality implemented from scratch
- **No Dynamic Linking** - Direct syscalls or API resolution via PEB/PE parsing
- **No Global Constructors** - Avoid `.init_array` sections

## Supported Platforms

| Platform | Architectures | Entry Point | Memory | Console |
|----------|---------------|-------------|--------|---------|
| **Windows** | i386, x86_64, armv7a, aarch64 | `_start()` | NtAllocateVirtualMemory | WriteConsoleW |

## Build System

### CMake Configuration

- **Toolchain**: Clang/LLVM 20+ with LLD linker
- **Cross-Compilation**: `-target` flag for multi-architecture
- **Output**: `.exe` (Windows executables)

### Build Artifacts

Each build generates:

1. **Executable** - `output.exe`
2. **Disassembly** - `output.txt` (sections + code)
3. **String Analysis** - `output.strings.txt`
4. **PIC Blob** - `output.bin` (extracted `.text` section)
5. **Base64 Blob** - `output.b64.txt` (for injection/loading)
6. **Linker Map** - `output.map.txt`

### Directory Structure

```
build/
└── windows/
    └── <architecture>/
        ├── debug/
        │   ├── cmake/              # Build files
        │   ├── output.exe          # Executable
        │   ├── output.bin          # PIC blob
        │   ├── output.b64.txt      # Base64 PIC blob
        │   ├── output.txt          # Disassembly
        │   └── output.map.txt      # Linker map
        └── release/
```

## Testing

### Test Suites

Located in [include/tests/](../include/tests/):

1. **Djb2Tests** - Hash function consistency
2. **MemoryTests** - Memory operation validation
3. **StringTests** - String manipulation
4. **Uint64Tests** - 64-bit unsigned arithmetic
5. **Int64Tests** - 64-bit signed arithmetic
6. **DoubleTests** - Floating-point operations
7. **StringFormatterTests** - Printf formatting

### Running Tests

```powershell
# Build specific configuration
cmake -B "build/windows/x86_64/debug/cmake" `
      -G Ninja `
      "-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-clang.cmake" `
      "-DARCHITECTURE=x86_64" `
      "-DPLATFORM=windows" `
      "-DBUILD_TYPE=debug"
cmake --build "build/windows/x86_64/debug/cmake"

# Run Windows test
.\build\windows\x86_64\debug\output.exe

# Load PIC blob
.\scripts\loader.ps1 -BlobPath .\build\windows\x86_64\release\output.bin
```

## Security Considerations

### API Resolution (Windows)

- **PEB Walking** - Locates loaded modules without `GetModuleHandle`
- **PE Parsing** - Finds exports without `GetProcAddress`
- **Hash-Based Lookup** - Uses DJB2 hash to avoid storing strings

### Direct Syscalls

- **Windows** - Direct `syscall` instruction to `ntdll.dll` functions
- No import table dependencies

### Position Independence

- **No Relocations** - All code/data in single `.text` section
- **Embedded Constants** - Strings/doubles decomposed at compile-time
- **Base Address Agnostic** - Works at any memory address

## Development Workflow

### VSCode Integration

- **8 Build Configurations** - All Windows architecture/build-type combinations
- **Debug Configurations** - Native debugging using cppvsdbg debugger
- **Quick Build** - `Ctrl+Shift+B`

### Automation Scripts

- [scripts/loader.ps1](../scripts/loader.ps1) - Load PIC blob (Windows)

## Future Enhancements

1. **Networking** - Socket implementation via Windows APIs
2. **File I/O** - File operations
3. **Threading** - Multi-threading support
4. **Cryptography** - Embedded crypto primitives

## References

- [Platform Guide](platform_guide.md) - Windows implementation details
- [Build Guide](../README.md) - Build instructions and configuration
- [Tests](../tests/README.md) - Testing documentation
