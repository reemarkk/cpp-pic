/**
 * runtime.h - Unified Runtime API Header
 *
 * This header exposes all runtime APIs for position-independent code (PIC).
 * Include this single header to access the complete runtime functionality.
 *
 * RUNTIME MODULES:
 *   Platform   - Platform abstraction, initialization, relocation
 *   Console    - Console I/O and formatted output
 *   Logger     - Structured logging with ANSI colors
 *   Memory     - Memory operations (copy, set, compare, zero)
 *   String     - String utilities and conversions
 *   Allocator  - Low-level memory allocation
 *   Djb2       - Hash functions for strings
 *
 * USAGE:
 *   #include "runtime/runtime.h"
 *
 * DESIGN PHILOSOPHY:
 *   - Zero CRT dependencies
 *   - Position-independent code support
 *   - Windows platform support
 *   - No heap allocations required
 *   - Direct syscall implementations
 */

#pragma once

// Core platform primitives and types
#include "primitives.h"
#include "uint64.h"
#include "int64.h"
#include "double.h"
#include "embedded_double.h"
#include "embedded_string.h"

// Platform abstraction layer
#include "platform.h"
#include "allocator.h"

// String utilities
#include "string.h"
#include "string_formatter.h"
#include "djb2.h"

// Memory operations
#include "memory.h"

// Console and logging
#include "console.h"
#include "logger.h"
