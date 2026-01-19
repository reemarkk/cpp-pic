# CPP-PIC: Position-Independent C++23 Runtime for Windows

**A Revolutionary Approach to Zero-Dependency C++ Code Generation**

[![License](https://img.shields.io/badge/license-Proprietary-blue.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)](README.md)
[![Architecture](https://img.shields.io/badge/arch-i386%20%7C%20x86__64%20%7C%20armv7a%20%7C%20aarch64-orange.svg)](README.md)

---

## Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Platform Support](#platform-support)
- [Quick Start](#quick-start)
- [How It Works](#how-it-works)
- [Building](#building)
- [Windows Implementation](#windows-implementation)
- [Use Cases](#use-cases)
- [Documentation](#documentation)

---

## Overview

CPP-PIC is a C++23 runtime library for Windows designed to operate without any .rdata section depenedencies.
By embedding constants directly in executable code, we've created a truly position-independent runtime, making it suitable for shellcode, code injection, and embedded systems.

This project is the result of years of research into compailer behavior, binary strucrure and low-level Windows development.
Whether used for security research, kernel or system development, or experimentation with modern C++,
CPP-PIC offers exceptional control over binary memory layout.

## Key Features

- **Zero .rdata Dependencies**: All string literals and floating-point constants are embedded directly as immediate values in code, eliminating read-only data sections.
- **Position-Independent**: Address-independent execution without static relocation dependencies, suitable for in-memory execution, manual mapping, and custom loaders.
- **No CRT Required**: Implements a fully standalone runtime that operates without the Windows C or C++ runtime libraries.
- **Modern C++23**: Utilizes the latest C++23 features, including concepts, consteval, and fold expressions, to enable compile-time computation and improve strong type safety.
- **Windows-Native**: Implements functionality through native interfaces and runtime inspection of Windows structures, avoiding the use of imported functions.
- **Multi-Architecture**: Provides architecture-specific implementation for i386, x86_64, armv7a, and aarch64 targets.
- **Full Optimization Support**: Supports all LLVM optimization levels, allowing builds from unoptimized (-O0) to maximum optimization or performance size(-Oz or -03).

## Platform Support

| Architecture | Target Triple | Status |
|-------------|---------------|--------|
| **i386** | `i386-pc-windows-gnu` | ✅ Full support |
| **x86_64** | `x86_64-pc-windows-gnu` | ✅ Full support |
| **armv7a** | `armv7a-pc-windows-gnu` | ✅ Full support |
| **aarch64** | `aarch64-pc-windows-gnu` | ✅ Full support |

All architectures support both debug and release builds with comprehensive testing via GitHub Actions CI/CD.

## Quick Start

### Prerequisites

**Required Tools:**
- **LLVM/Clang 20+** - Download from [LLVM GitHub Releases](https://github.com/llvm/llvm-project/releases)
  - Version 20.1.6 or later recommended
  - Must include LLD linker
- **CMake 3.20+** - Download from [cmake.org](https://cmake.org/download/)
- **Ninja** - Install via: `winget install -e --id Ninja-build.Ninja`

**Installation:**

1. Download and install LLVM 20+ for Windows (LLVM-20.1.6-win64.exe)
2. Add LLVM to your PATH during installation
3. Install CMake and Ninja
4. Verify installation:
```powershell
clang --version   # Should show version 20.x.x
cmake --version   # Should show version 3.20+
ninja --version
```

### Build

```bash
# Configure (required: specify toolchain file)
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-clang.cmake

# Build
cmake --build build

# Run
.\build\windows\x86_64\release\output.exe
```

### Build Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `ARCHITECTURE` | `i386`, `x86_64`, `armv7a`, `aarch64` | `x86_64` | Target CPU architecture |
| `PLATFORM` | `windows` | `windows` | Target platform |
| `BUILD_TYPE` | `debug`, `release` | `release` | Build configuration |

### Build Examples

**Windows x64 Release (default):**
```bash
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-clang.cmake
cmake --build build
```

**Windows i386 Debug:**
```bash
cmake -B build/windows/i386/debug/cmake -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-clang.cmake \
    -DARCHITECTURE=i386 \
    -DBUILD_TYPE=debug
cmake --build build/windows/i386/debug/cmake
```

**Windows ARM64 Release:**
```bash
cmake -B build/windows/aarch64/release/cmake -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-clang.cmake \
    -DARCHITECTURE=aarch64 \
    -DBUILD_TYPE=release
cmake --build build/windows/aarch64/release/cmake
```

## How It Works

CPP-PIC leverages modern C++23 features to achieve full position independence through three key innovations:

### 1. Compile-Time String Decomposition

Strings are broken down into individual characters at compile time using user-defined literal operators and variadic templates.
This approach allows all string data to be embedded directly into the instruction stream, eliminating reliance on read-only data sections.

```cpp
template <typename TChar, TChar... Chars>
class EMBEDDED_STRING {
    TChar data[sizeof...(Chars) + 1];

    NOINLINE DISABLE_OPTIMIZATION EMBEDDED_STRING() {
        USIZE i = 0;
        ((data[i++] = Chars), ...);  // Fold expression
        data[i] = 0;
    }
};
```

**Usage:**
```cpp
auto msg = "Hello, World!"_embed;   // Embedded in code, not .rdata
```

**Assembly Output:**
```asm
movw $0x48, (%rdi)      ; 'H'
movw $0x65, 2(%rdi)     ; 'e'
movw $0x6C, 4(%rdi)     ; 'l'
movw $0x6C, 6(%rdi)     ; 'l'
movw $0x6F, 8(%rdi)     ; 'o'
```

### 2. IEEE-754 Bit Pattern Embedding

CPP-PIC transforms floating-point literals into their IEEE-754 binary encoding during compilation.
Each value is stored inline in the executable code, avoiding .rdata dependencies and supporting fully position-independent execution.

```cpp
struct EMBEDDED_DOUBLE {
    consteval explicit EMBEDDED_DOUBLE(double v) {
        bits = __builtin_bit_cast(unsigned long long, v);
    }

    operator double() const {
        return __builtin_bit_cast(double, bits);
    }
};
```

**Usage:**
```cpp
auto pi = 3.14159_embed;  // IEEE-754 as immediate value
```

**Assembly Output:**
```asm
movabsq $0x400921f9f01b866e, %rax  ; Pi as 64-bit immediate
```

### 3. Pure Integer-Based Type Conversions

All type conversions are performed using bitwise operations rather than compiler-generated constants.
This ensures that no additional data is stored in read-only sections and that all conversions remain fully position-independent.

```cpp
// Extracts integer value from IEEE-754 without FPU instructions
INT64 operator(INT64)(const DOUBLE& d) {
    UINT64 bits = d.bits;
    int exponent = ((bits >> 52) & 0x7FF) - 1023;
    UINT64 mantissa = (bits & 0xFFFFFFFFFFFFF) | 0x10000000000000;
    // ... bit shifting magic ...
}
```

## Building

### VSCode Integration (Recommended)

The project includes comprehensive VSCode integration:

**Quick Actions:**
- `Ctrl+Shift+B` - Build all configurations
- `F5` - Run/Debug with selected configuration

**Available Build Tasks:**
- `i386-pc-windows-gnu-debug/release`
- `x86_64-pc-windows-gnu-debug/release`
- `armv7a-pc-windows-gnu-debug/release`
- `aarch64-pc-windows-gnu-debug/release`
- `Build All` - Builds all release configurations in parallel

### Build Outputs

After building, artifacts are placed in `build/windows/<arch>/<type>/`:

| File | Description |
|------|-------------|
| `output.exe` | Main executable |
| `output.bin` | Extracted `.text` section (PIC blob) |
| `output.b64.txt` | Base64-encoded PIC blob |
| `output.txt` | Disassembly and section dump |
| `output.strings.txt` | Extracted strings (should be minimal) |
| `output.map.txt` | Linker map file |

### Optimization Levels

**Default Settings:**
- **DEBUG builds** use `-Og` (optimized for debugging experience)
- **RELEASE builds** use `-O3` (maximum performance optimization)

**Features:**
- Debug builds include CodeView symbols for WinDbg/Visual Studio debugging
- Release builds include LTO (Link-Time Optimization), aggressive inlining, and loop unrolling
- No string literals or floating-point constants in `.rdata` at any optimization level

## Windows Implementation

### Low-Level Native Interfaces

CPP-PIC relies on low-level native APIs, avoiding external libraries or exported DLLs whenever possible. The project primarily interacts with the system through the following:

**Native interfaces (ntdll.dll):**
Core operations are handled using low-level native functions for memory management and process control.
- `NtAllocateVirtualMemory` - Allocates virtual memory pages
- `NtFreeVirtualMemory` - Releases virtual memory pages
- `NtTerminateProcess` - Terminates the current process

**Minimal Win32 interaction (kernel32.dll):**
Limited use of Win32 APIs for basic console output:
- `WriteConsoleA` - Writes ANSI (narrow) character output to the console
- `WriteConsoleW` - Writes wide-character output to the console

### PEB Walking

CPP-PIC uses the PEB to enumerate loaded modules directly, avoiding Win32 API dependencies for module discovery.

```cpp
// Locate PEB (x64)
PEB* peb = GetPEB();

// Walk loaded modules
PEB_LDR_DATA* ldr = peb->Ldr;
LIST_ENTRY* moduleList = &ldr->InMemoryOrderModuleList;

// Enumerate modules: ntdll.dll, kernel32.dll, etc.
```

### PE Parsing

CPP-PIC parses Portable Executable (PE) headers and export tables to resolve API functions by hash at runtime.

```cpp
// Find export by DJB2 hash
FARPROC GetExportByHash(HMODULE module, UINT32 hash);

// Example: Resolve WriteConsoleW
auto writeConsole = GetExportByHash(kernel32, 0x7B8F69D2);
```

**Benefits:**
- Import-Free Execution – No import table or GetProcAddress required
- Hash-Based API Resolution – Avoids string-based function references
- Fully Position-Independent – Can execute at arbitrary memory addresse

### Console Output

Printf-style formatting without CRT:

```cpp
Console::WriteFormatted<WCHAR>(L"Integer: %d\n"_embed, 42);
Console::WriteFormatted<WCHAR>(L"Float: %.5f\n"_embed, 3.14159_embed);
Console::WriteFormatted<WCHAR>(L"Hex: 0x%X\n"_embed, 255);
```

**Format Specifiers:**
- `%d` - Signed decimal integer
- `%u` - Unsigned decimal integer
- `%X` - Uppercase hexadecimal
- `%f`, `%.Nf` - Floating-point with precision
- `%s`, `%ls` - Narrow/wide strings
- `%p` - Pointer

### Linker Configuration

Critical flags for position-independence:

```cmake
# Merge .rdata into .text (CRITICAL for PIC)
/MERGE:.rdata=.text

# Custom entry point (no CRT)
/Entry:_start

# Function ordering (i386 only)
/ORDER:@orderfile.txt

# Release optimizations
/OPT:REF        # Remove unreferenced code
/OPT:ICF        # Fold identical code
/LTCG           # Link-time code generation
```

## Compiler & Linker Flags

### PIC-Critical Flags

| Flag | Purpose |
|------|---------|
| `-fno-jump-tables` | Prevents switch statement jump tables in .rdata |
| `-fno-rtti` | Disables runtime type information |
| `-fno-exceptions` | Disables C++ exceptions |
| `/MERGE:.rdata=.text` | Merges read-only data into code section |

### Base Compiler Flags

| Flag | Purpose |
|------|---------|
| `-std=c++23` | Enable C++23 standard features |
| `-nostdlib` | No standard C/C++ libraries |
| `-fno-builtin` | Disable compiler built-ins |
| `-fshort-wchar` | Use 2-byte wchar_t (Windows ABI) |
| `-msoft-float` | Software floating-point (x86 only) |

## Project Structure

```
cpp-pic/
├── cmake/                          # Build system
│   ├── toolchain-clang.cmake      # Clang toolchain
│   ├── base64_encode.cmake        # Base64 encoding
│   └── verify_no_rdata.cmake      # .rdata validation
├── include/
│   └── runtime/
│       ├── platform/
│       │   ├── primitives/        # EMBEDDED_STRING, DOUBLE, etc.
│       │   ├── windows/           # Windows headers
│       │   ├── allocator.h
│       │   └── platform.h
│       ├── console.h
│       ├── logger.h
│       ├── memory.h
│       └── string_formatter.h
├── src/
│   ├── start.cc                   # Entry point
│   └── runtime/
│       ├── platform/windows/      # Windows implementation
│       └── console/windows/       # Windows console
├── build/windows/                 # Build artifacts
├── docs/                          # Documentation
├── scripts/                       # Automation scripts
├── tests/                         # Test suite
├── .vscode/                       # VSCode integration
├── .github/workflows/             # CI/CD pipeline
├── CMakeLists.txt
└── README.md
```

## Use Cases

| Domain | Examples |
|--------|----------|
| **Security Research** | Shellcode, code injection, exploit development |
| **Embedded Systems** | Minimal dependencies, no CRT required, bare-metal environments |
| **Kernel Development** | Windows kernel modules,device drivers |
| **Binary Analysis** | Understanding compiler behavior, reverse engineering, PE analysis |
| **Education** | Compiler design, OS development, low-level programming |

### Why CPP-PIC?

- **Security**: Address-independent execution without relocations.
- **Embedded**: Minimal, standalone runtime for constrained systems.
- **Kernel**: Modern C++23 features without CRT or runtime initialization.
- **Research**: Study compiler behavior, PE structure, and low-level binaries.

## Testing

### Running Tests

Executing the compiled binary automatically runs all included tests, validating core functionality and architecture support.

```powershell
# Run x64 release build
.\build\windows\x86_64\release\output.exe

# Run i386 debug build
.\build\windows\i386\debug\output.exe
```

**Expected Output:**
```
CPP-PIC Runtime Starting...
Running UINT64 Tests... PASSED
Running INT64 Tests... PASSED
Running Double Tests... PASSED
Running String Tests... PASSED
Running StringFormatter Tests... PASSED
All tests passed!
```

### Test Coverage

The test suite validates:
- **UINT64/INT64**: Software 64-bit integer operations
- **DOUBLE**: IEEE-754 floating-point operations
- **String**: String manipulation functions
- **StringFormatter**: Printf-style formatting
- **Memory**: Memory operations (Copy, Zero, Compare)
- **DJB2**: Hash function consistency

### CI/CD Testing

GitHub Actions automatically tests all configurations:
- Builds: i386, x86_64, aarch64 (Windows)
- Tests: i386 (WoW64), x86_64 (native), aarch64 (self-hosted ARM64 runner)
- Validation: Ensures no .rdata dependencies and verifies correct exit codes

## Binary Analysis

### Verifying Position Independence

```powershell
# Check .rdata section (should be minimal)
llvm-objdump -h build\windows\x86_64\release\output.exe | findstr .rdata

# Extract strings (should not contain your embedded strings)
llvm-strings build\windows\x86_64\release\output.exe

# View disassembly showing immediate values
llvm-objdump -d build\windows\x86_64\release\output.exe | findstr "mov.*\$0x"
```

**Expected Results:**
- `.rdata`: Only minimal compiler-generated constants (~32 bytes)
- Strings: Fully embedded in code, no read-only storage
- Disassembly: Characters encoded as immediate operands, e.g., movw $0x57, (%rax)

## Documentation

For more detailed information:

- **[Architecture Guide](docs/architecture.md)** - System design and components
- **[Platform Guide](docs/platform_guide.md)** - Windows implementation details
- **[Project Structure](STRUCTURE.md)** - Project organization
- **[Scripts Documentation](scripts/README.md)** - Automation scripts

## Contributing

This is a private research project intended for educational and authorized security research purposes only.

## License

Proprietary - All rights reserved

## Security Notice

This runtime is designed for position-independent code (PIC) environments, including use cases that require execution without fixed load addresses.
It is intended exclusively for the following purposes:

- Authorized security testing and penetration testing
- Academic research and educational use
- Legitimate software development requiring PIC constraints
- Defensive security research and analysis

⚠️ Any unauthorized or malicious use of this software is strictly prohibited.

---

**CPP-PIC** - Pushing the boundaries of position-independent C++23 on Windows.
