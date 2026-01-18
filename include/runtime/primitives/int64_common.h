/**
 * int64_common.h - Shared Macros for 64-bit Integer Types
 *
 * This header provides macro definitions for operations that are identical
 * between UINT64 and INT64, eliminating ~400 lines of code duplication.
 *
 * DESIGN RATIONALE:
 *
 * We CANNOT use inheritance (even CRTP) because:
 *   - Virtual tables would be created
 *   - Vtables reside in .rdata section
 *   - This violates position-independent code requirements
 *
 * Instead, we use C preprocessor macros to generate identical code in both
 * UINT64 and INT64 classes. Macros expand at compile-time with zero runtime
 * overhead and no .rdata dependencies.
 *
 * USAGE:
 *   1. Include this header in uint64.h and int64.h
 *   2. Invoke macros inside class definitions
 *   3. Pass class name and word type as parameters
 *
 * MACRO PARAMETERS:
 *   CLASS_NAME - The derived class name (UINT64 or INT64)
 *   WORD_TYPE  - Type of high word (UINT32 for unsigned, INT32 for signed)
 */

#pragma once

/**
 * DEFINE_INT64_COMPARISON_OPERATORS
 *
 * Generates all six comparison operators: ==, !=, <, <=, >, >=
 *
 * These operators are identical for signed and unsigned types because:
 *   - They compare high word first (handles sign correctly via WORD_TYPE)
 *   - Then compare low word if high words are equal
 *
 * Parameters:
 *   CLASS_NAME - UINT64 or INT64
 */
#define DEFINE_INT64_COMPARISON_OPERATORS(CLASS_NAME)                                    \
    constexpr bool operator==(const CLASS_NAME &other) const noexcept                    \
    {                                                                                     \
        return (low == other.low) && (high == other.high);                               \
    }                                                                                     \
                                                                                          \
    constexpr bool operator!=(const CLASS_NAME &other) const noexcept                    \
    {                                                                                     \
        return !(*this == other);                                                        \
    }                                                                                     \
                                                                                          \
    constexpr bool operator<(const CLASS_NAME &other) const noexcept                     \
    {                                                                                     \
        if (high != other.high)                                                          \
            return high < other.high;                                                    \
        return low < other.low;                                                          \
    }                                                                                     \
                                                                                          \
    constexpr bool operator<=(const CLASS_NAME &other) const noexcept                    \
    {                                                                                     \
        if (high != other.high)                                                          \
            return high < other.high;                                                    \
        return low <= other.low;                                                         \
    }                                                                                     \
                                                                                          \
    constexpr bool operator>(const CLASS_NAME &other) const noexcept                     \
    {                                                                                     \
        if (high != other.high)                                                          \
            return high > other.high;                                                    \
        return low > other.low;                                                          \
    }                                                                                     \
                                                                                          \
    constexpr bool operator>=(const CLASS_NAME &other) const noexcept                    \
    {                                                                                     \
        if (high != other.high)                                                          \
            return high > other.high;                                                    \
        return low >= other.low;                                                         \
    }

/**
 * DEFINE_INT64_BITWISE_OPERATORS
 *
 * Generates bitwise operators: &, |, ^, ~
 *
 * These operators work identically on both signed and unsigned types,
 * operating independently on high and low words.
 *
 * Parameters:
 *   CLASS_NAME - UINT64 or INT64
 */
#define DEFINE_INT64_BITWISE_OPERATORS(CLASS_NAME)                                       \
    constexpr CLASS_NAME operator&(const CLASS_NAME &other) const noexcept              \
    {                                                                                     \
        return CLASS_NAME(high & other.high, low & other.low);                           \
    }                                                                                     \
                                                                                          \
    constexpr CLASS_NAME operator|(const CLASS_NAME &other) const noexcept              \
    {                                                                                     \
        return CLASS_NAME(high | other.high, low | other.low);                           \
    }                                                                                     \
                                                                                          \
    constexpr CLASS_NAME operator^(const CLASS_NAME &other) const noexcept              \
    {                                                                                     \
        return CLASS_NAME(high ^ other.high, low ^ other.low);                           \
    }                                                                                     \
                                                                                          \
    constexpr CLASS_NAME operator~() const noexcept                                      \
    {                                                                                     \
        return CLASS_NAME(~high, ~low);                                                  \
    }

/**
 * DEFINE_INT64_BITWISE_ASSIGNMENTS
 *
 * Generates compound bitwise assignment operators: &=, |=, ^=
 *
 * Parameters:
 *   CLASS_NAME - UINT64 or INT64
 */
#define DEFINE_INT64_BITWISE_ASSIGNMENTS(CLASS_NAME)                                     \
    constexpr CLASS_NAME &operator&=(const CLASS_NAME &other) noexcept                   \
    {                                                                                     \
        high &= other.high;                                                              \
        low &= other.low;                                                                \
        return *this;                                                                     \
    }                                                                                     \
                                                                                          \
    constexpr CLASS_NAME &operator|=(const CLASS_NAME &other) noexcept                   \
    {                                                                                     \
        high |= other.high;                                                              \
        low |= other.low;                                                                \
        return *this;                                                                     \
    }                                                                                     \
                                                                                          \
    constexpr CLASS_NAME &operator^=(const CLASS_NAME &other) noexcept                   \
    {                                                                                     \
        high ^= other.high;                                                              \
        low ^= other.low;                                                                \
        return *this;                                                                     \
    }

/**
 * DEFINE_INT64_INCREMENT_DECREMENT
 *
 * Generates increment and decrement operators: ++, --  (prefix and postfix)
 *
 * Identical for signed and unsigned:
 *   - Increment: increment low, if wrapped to 0, increment high
 *   - Decrement: decrement low, if was 0 before decrement, decrement high
 *
 * Parameters:
 *   CLASS_NAME - UINT64 or INT64
 */
#define DEFINE_INT64_INCREMENT_DECREMENT(CLASS_NAME)                                     \
    constexpr CLASS_NAME &operator++() noexcept                                          \
    {                                                                                     \
        if (++low == 0)                                                                  \
            ++high;                                                                      \
        return *this;                                                                     \
    }                                                                                     \
                                                                                          \
    constexpr CLASS_NAME operator++(int) noexcept                                        \
    {                                                                                     \
        CLASS_NAME temp = *this;                                                         \
        ++(*this);                                                                       \
        return temp;                                                                     \
    }                                                                                     \
                                                                                          \
    constexpr CLASS_NAME &operator--() noexcept                                          \
    {                                                                                     \
        if (low-- == 0)                                                                  \
            --high;                                                                      \
        return *this;                                                                     \
    }                                                                                     \
                                                                                          \
    constexpr CLASS_NAME operator--(int) noexcept                                        \
    {                                                                                     \
        CLASS_NAME temp = *this;                                                         \
        --(*this);                                                                       \
        return temp;                                                                     \
    }

/**
 * DEFINE_INT64_COMMON_OPERATIONS
 *
 * Master macro that expands all common operations.
 * Use this single macro in both UINT64 and INT64 class definitions.
 *
 * Parameters:
 *   CLASS_NAME - UINT64 or INT64
 */
#define DEFINE_INT64_COMMON_OPERATIONS(CLASS_NAME)                                       \
    DEFINE_INT64_COMPARISON_OPERATORS(CLASS_NAME)                                        \
    DEFINE_INT64_BITWISE_OPERATORS(CLASS_NAME)                                           \
    DEFINE_INT64_BITWISE_ASSIGNMENTS(CLASS_NAME)                                         \
    DEFINE_INT64_INCREMENT_DECREMENT(CLASS_NAME)
