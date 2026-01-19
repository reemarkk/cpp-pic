# Windows Platform Implementation Guide

This guide provides detailed information about the Windows-specific implementation in CPP-PIC.

## Table of Contents

1. [Architecture Support](#architecture-support)
2. [Initialization Sequence](#initialization-sequence)
3. [Key Components](#key-components)
4. [Memory Management](#memory-management)
5. [Console Output](#console-output)
6. [Low-Level Native Interfaces](#low-level-native-interfaces)
7. [Linker Configuration](#linker-configuration)
8. [Best Practices](#best-practices)

---

## Architecture Support

| Architecture | Target Triple | Status |
|-------------|---------------|--------|
| i386 | `i386-pc-windows-gnu` | ✅ Supported |
| x86_64 | `x86_64-pc-windows-gnu` | ✅ Supported |
| armv7a | `armv7a-pc-windows-gnu` | ✅ Supported |
| aarch64 | `aarch64-pc-windows-gnu` | ✅ Supported |

All architectures support both debug and release builds.

---

## Initialization Sequence

The Windows platform initialization follows these steps:

1. **Entry Point** - `_start()` in [src/start.cc](../src/start.cc)
2. **Platform Init** - `Initialize()` in [src/runtime/platform/windows/platform.windows.cc](../src/runtime/platform/windows/platform.windows.cc)
3. **PEB Location** - `GetPEB()` in [src/runtime/platform/windows/peb.cc](../src/runtime/platform/windows/peb.cc)
4. **Module Enumeration** - Walk `PEB->Ldr->InMemoryOrderModuleList`
5. **API Resolution** - Parse PE exports, hash-based lookup using DJB2

---

## Key Components

### PEB Walking ([peb.cc](../src/runtime/platform/windows/peb.cc))

The Process Environment Block (PEB) is the starting point for all Windows runtime initialization:

```cpp
// Locate Process Environment Block
PEB* peb = GetPEB();

// Access loaded modules
PEB_LDR_DATA* ldr = peb->Ldr;
LIST_ENTRY* moduleList = &ldr->InMemoryOrderModuleList;
```

**PEB Structure:**
- **ImageBaseAddress** - Base address of current executable
- **Ldr** - Loader data containing loaded modules list
- **ProcessParameters** - Command line arguments and environment variables

**Accessing PEB:**
- **x64**: Read from `GS:[0x60]`
- **x86**: Read from `FS:[0x30]`
- **ARM64**: Platform-specific TEB access

### PE Parsing ([pe.cc](../src/runtime/platform/windows/pe.cc))

Export tables are parsed to resolve API functions dynamically:

```cpp
// Find export by DJB2 hash
FARPROC GetExportByHash(HMODULE module, UINT32 hash);

// Example usage
auto ntAllocate = GetExportByHash(ntdllBase, 0x12345678);
```

**Process:**
1. Read DOS header (`IMAGE_DOS_HEADER`) → locate PE header
2. Read PE header (`IMAGE_NT_HEADERS`) → locate Export Directory
3. Iterate through export names, compute DJB2 hash
4. Return function RVA when hash matches
5. Convert RVA to absolute address

**Benefits:**
- Import-Free Execution – No import table or GetProcAddress required
- Hash-Based API Resolution – Avoids string-based function references
- Fully Position-Independent – Can execute at arbitrary memory addresses


### API Resolution

Two primary DLLs are resolved for core functionality:

#### ntdll.dll ([ntdll.cc](../src/runtime/platform/windows/ntdll.cc))

System-level APIs for direct kernel interaction:

| Function | Hash | Purpose |
|----------|------|---------|
| `NtAllocateVirtualMemory` | Computed at runtime | Allocates virtual memory pages |
| `NtFreeVirtualMemory` | Computed at runtime | Releases virtual memory pages |
| `NtTerminateProcess` | Computed at runtime | Terminates the current process |

#### kernel32.dll ([kernel32.cc](../src/runtime/platform/windows/kernel32.cc))

Higher-level Windows APIs:

| Function | Hash | Purpose |
|----------|------|---------|
| `WriteConsoleA` | Computed at runtime | Writes ANSI (narrow) characters output to the console |
| `WriteConsoleW` | Computed at runtime | Writes wide-character output to the console |

---

## Memory Management

CPP-PIC manages memory via native Windows interfaces, avoiding the standard heap and runtime libraries.

### Allocation

```cpp
// Direct syscall to ntdll.NtAllocateVirtualMemory
PVOID baseAddress = NULL;
SIZE_T size = 4096;  // Page size

NTSTATUS status = NtAllocateVirtualMemory(
    GetCurrentProcess(),        // Process handle (-1)
    &baseAddress,               // Base address (NULL = let kernel choose)
    0,                          // Zero bits
    &size,                      // Size (in/out parameter)
    MEM_COMMIT | MEM_RESERVE,   // Allocation type
    PAGE_READWRITE              // Protection
);
```

**Parameters:**
- `MEM_COMMIT` - Commit physical storage
- `MEM_RESERVE` - Reserve address space
- `PAGE_READWRITE` - Read/write access (no execute)

### Deallocation

```cpp
// Direct syscall to ntdll.NtFreeVirtualMemory
SIZE_T size = 0;  // 0 = free entire region

NTSTATUS status = NtFreeVirtualMemory(
    GetCurrentProcess(),
    &baseAddress,
    &size,
    MEM_RELEASE
);
```

### Why Native Interfaces?

- **Standalone Runtime**: No reliance on CRT or standard heap
- **Position-Independent**: Operates without import table entries
- **Low Overhead**: Direct kernel interaction
- **Precise Memory Control**: Allocate, protect, and manage memory manually

---

## Console Output

CPP-PIC implements console output using `WriteConsoleW` for wide character support:

### Basic Output

```cpp
// Get stdout handle
HANDLE stdOut = pPeb->ProcessParameters->StandardOutput;

// Write wide string
WCHAR buffer[] = L"Hello, World!\n";
DWORD written;
WriteConsoleW(stdOut, buffer, wcslen(buffer), &written, NULL);
```

### Formatted Output

Printf-style formatting without CRT:

```cpp
Console::WriteFormatted<WCHAR>(L"Integer: %d\n"_embed, 42);
Console::WriteFormatted<WCHAR>(L"Float: %.5f\n"_embed, 3.14159_embed);
Console::WriteFormatted<WCHAR>(L"Hex: 0x%X\n"_embed, 255);
Console::WriteFormatted<WCHAR>(L"String: %ls\n"_embed, L"Hello");
```

### Supported Format Specifiers

| Specifier | Type | Example |
|-----------|------|---------|
| `%d` | Signed integer | `-42` |
| `%u` | Unsigned integer | `42` |
| `%ld` | Long integer | `1234567890` |
| `%X` | Uppercase hex | `DEADBEEF` |
| `%x` | Lowercase hex | `deadbeef` |
| `%f` | Float (default precision) | `3.141590` |
| `%.Nf` | Float (N decimals) | `3.14159` |
| `%c` | Character | `A` |
| `%s` | Narrow string | `Hello` |
| `%ls` | Wide string | `L"Hello"` |
| `%p` | Pointer | `0x00007FF6A1B2C3D4` |

---

## Low-Level Native Interfaces

### Why Not Standard APIs?

Traditional Windows programs use:
- `VirtualAlloc` → requires `kernel32.dll` import
- `printf` → requires CRT initialization
- `GetProcAddress` → still requires imports

CPP-PIC avoids all of this by:
1. Walking the PEB to locate `ntdll.dll` base address
2. Parsing PE exports to find function addresses
3. Calling functions directly via function pointers

### Syscall Flow

```
Application
    ↓
Direct Function Pointer Call
    ↓
ntdll.dll Export
    ↓
syscall instruction (x64) / int 2Eh (x86)
    ↓
Windows Kernel (ntoskrnl.exe)
```

**Traditional Flow (avoided):**
```
Application
    ↓
Import Table Lookup
    ↓
kernel32.dll/ntdll.dll Thunk
    ↓
syscall
    ↓
Kernel
```

---

## Linker Configuration

Critical linker flags for Windows position-independence:

### Base Linker Flags

```cmake
-fuse-ld=lld                    # Use LLVM LLD linker
-nostdlib                       # No standard libraries
-Wl,/Entry:_start              # Custom entry point (no CRT)
-Wl,/SUBSYSTEM:CONSOLE         # Console application
-Wl,/MERGE:.rdata=.text        # CRITICAL: Merge .rdata into .text
-Wl,/ORDER:@orderfile.txt      # Function ordering (i386 only)
```

### i386-Specific Flags

```cmake
-Wl,/BASE:0x400000             # Preferred load address
-Wl,/FILEALIGN:0x1000          # File section alignment (4KB)
-Wl,/SAFESEH:NO                # Disable SafeSEH
```

### Debug Build Flags

```cmake
-Wl,/DEBUG                     # Include debug info
-Wl,/MAP:output.map.txt        # Generate linker map
```

### Release Build Flags

```cmake
-Wl,--strip-all                # Remove all symbols
-Wl,/OPT:REF                   # Remove unreferenced code/data
-Wl,/OPT:ICF                   # Fold identical code
-Wl,/RELEASE                   # Set release flag in PE header
-Wl,/LTCG                      # Link-time code generation
-Wl,/MAP:output.map.txt        # Generate linker map
```

### Why /MERGE:.rdata=.text is Critical

The `/MERGE:.rdata=.text` flag is **essential** for true position-independence:

- LTO (Link-Time Optimization) may generate `.rdata` constants
- Without merging, these constants break PIC
- Merging ensures all read-only data becomes part of executable code section
- Result: Single `.text` section with no external data dependencies

---

## Best Practices

### 1. Hash Function Selection

Use DJB2 for API name hashing:

```cpp
constexpr UINT32 ComputeHash(const char* str) {
    UINT32 hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}
```

**Pre-compute hashes at compile-time:**
```cpp
constexpr UINT32 HASH_NtAllocateVirtualMemory = ComputeHash("NtAllocateVirtualMemory");
```

### 2. Error Handling

Always check NTSTATUS return values:

```cpp
NTSTATUS status = NtAllocateVirtualMemory(...);
if (!NT_SUCCESS(status)) {
    // Handle error
    ExitProcess(status);
}
```

### 3. String Embedding

Always use `_embed` suffix for strings:

```cpp
// GOOD: Embedded in code
auto msg = L"Error code: 0x%X\n"_embed;

// BAD: Would go to .rdata
const wchar_t* msg = L"Error code: 0x%X\n";
```

### 4. Memory Alignment

Ensure proper alignment for architecture:

```cpp
// x64 requires 16-byte stack alignment
// x86 requires 4-byte stack alignment
// ARM64 requires 16-byte stack alignment
```

### 5. Testing Position Independence

Verify no .rdata dependencies:

```powershell
# Check .rdata section (should be minimal)
llvm-objdump -h output.exe | findstr .rdata

# Verify section merge worked
llvm-objdump -h output.exe | findstr ".text"

# Check for string literals (should be empty or minimal)
llvm-strings output.exe | findstr "your_string"
```

---

## Troubleshooting

### Problem: API Resolution Fails

**Symptoms**: Crash on first API call, access violation

**Solutions**:
1. Verify DJB2 hash matches export name exactly
2. Check module base address is valid
3. Enable debug output to print export names
4. Verify PE parsing logic for architecture

**Debug Code**:
```cpp
// Print all exports
for (DWORD i = 0; i < numberOfNames; i++) {
    const char* name = (const char*)(moduleBase + nameRVAs[i]);
    Console::Write<char>(name);
    Console::Write<char>("\n"_embed);
}
```

### Problem: Linker Errors About .rdata

**Symptoms**: Build succeeds but binary has .rdata references

**Solutions**:
1. Ensure `/MERGE:.rdata=.text` flag is set in `CMakeLists.txt`
2. Check LTO is enabled for release builds (`-flto=full`)
3. Verify no external constants are referenced
4. Check compiler flags include `-fno-jump-tables`, `-fno-rtti`

### Problem: Import Table Not Empty

**Symptoms**: Binary has import directory

**Solutions**:
1. Ensure `-nostdlib` flag is set
2. Check no CRT functions are being called
3. Verify all API calls go through hash-based resolution
4. Review linker map file for unexpected dependencies

### Problem: Stack Corruption on x64

**Symptoms**: Random crashes, invalid pointers

**Solutions**:
1. Ensure stack is 16-byte aligned before function calls
2. Check shadow space allocation for x64 calling convention
3. Verify `_start` function sets up stack correctly
4. Use debug build with frame pointers (`-fno-omit-frame-pointer`)

---

## Performance Considerations

### API Resolution Overhead

- Hash computation: O(n) where n = string length
- PE export iteration: O(m) where m = number of exports
- Typically < 1000 exports per DLL
- Resolution happens once at startup
- Cached in global variables

### Memory Allocation Performance

Direct `NtAllocateVirtualMemory` calls:
- **Advantages**: - Avoids heap fragmentation
                  - No heap metadata overhead
- **Disadvantage**: Allocations are page-aligned (minimum 4 KB)
- **Use case**: Best suited for larger memory allocations (greater than 4 KB)

### Console Output Performance

`WriteConsoleW` is slower than `WriteFile`:
- **Why**: Console subsystem overhead
- **Mitigation**: Buffer output when possible
- **Alternative**: Use `WriteFile` to stdout for better performance

---

## References

- [Windows PE Format](https://docs.microsoft.com/en-us/windows/win32/debug/pe-format)
- [Native API Reference](https://undocumented.ntinternals.net/)
- [PEB Structure](https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/pebteb/peb/index.htm)
- [Architecture Documentation](architecture.md)
- [Main README](../README.md)
