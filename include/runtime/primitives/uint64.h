/**
 * uint64.h - Position-Independent 64-bit Unsigned Integer Implementation
 *
 * This file implements a custom 64-bit unsigned integer type that operates
 * entirely in position-independent code without .rdata dependencies.
 *
 * WHY THIS EXISTS:
 *
 * On many platforms, 64-bit integer operations can trigger compiler-generated
 * constants in the .rdata section, especially for:
 *   - Division by constants
 *   - Multiplication by large constants
 *   - Bit rotation operations
 *   - Some shift operations
 *
 * By implementing 64-bit arithmetic using only 32-bit operations, we maintain
 * complete control over code generation and guarantee no .rdata references.
 *
 * DESIGN APPROACH:
 *
 * The UINT64 class stores a 64-bit value as two 32-bit words:
 *   - high: Upper 32 bits [63:32]
 *   - low:  Lower 32 bits [31:0]
 *
 * All operations are decomposed into 32-bit arithmetic with manual carry handling:
 *   - Addition:       Add low words, propagate carry to high word
 *   - Subtraction:    Subtract with borrow handling
 *   - Multiplication: Four partial products (a_lo * b_lo, a_lo * b_hi, etc.)
 *   - Division:       Long division algorithm with bit-by-bit quotient
 *   - Shifts:         Combine shifts across word boundary
 *
 * PERFORMANCE CONSIDERATIONS:
 *
 * This implementation is SLOWER than native 64-bit operations on 64-bit CPUs:
 *   - Native:  Single instruction (e.g., `add rax, rbx`)
 *   - UINT64:  Multiple instructions with carry handling
 *
 * However, it provides critical guarantees:
 *   ✓ No .rdata section dependencies
 *   ✓ Predictable code generation
 *   ✓ Works on 32-bit and 64-bit architectures
 *   ✓ No hidden compiler intrinsics
 *
 * USE CASES:
 *   - Position-independent code (shellcode, injected code)
 *   - Embedded systems with strict memory layout requirements
 *   - Kernel drivers requiring non-paged code
 *   - Bootloaders and firmware
 *   - Any environment where .rdata is forbidden
 */

#pragma once

#include "primitives.h"
#include "int64_common.h"

/**
 * UINT64 - Position-independent 64-bit unsigned integer class
 *
 * Stores a 64-bit unsigned integer as two 32-bit words (high and low).
 * All arithmetic operations are implemented using 32-bit operations only,
 * ensuring no compiler-generated constants appear in .rdata.
 *
 * STORAGE LAYOUT:
 *   Bits [63:32] → high (UINT32)
 *   Bits [31:0]  → low  (UINT32)
 *
 * SUPPORTED OPERATIONS:
 *   - Arithmetic: +, -, *, /, %
 *   - Bitwise:    &, |, ^, ~, <<, >>
 *   - Comparison: ==, !=, <, <=, >, >=
 *   - Assignment: =, +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>=
 *   - Increment:  ++, --
 *
 * CONSTEXPR SUPPORT:
 *   Most operations are constexpr, enabling compile-time computation.
 *   This allows the compiler to optimize constant expressions at compile-time
 *   while still maintaining .rdata-free code generation.
 */
class UINT64
{
private:
    UINT32 low;   // Lower 32 bits
    UINT32 high;  // Upper 32 bits

public:
    // Maximum value for UINT64: 0xFFFFFFFFFFFFFFFF (18,446,744,073,709,551,615)
    static constexpr UINT64 MAX() noexcept
    {
        return UINT64((UINT32)-1, (UINT32)-1);
    }
    // Default constructor
    constexpr UINT64() noexcept : low(0), high(0) {}

    // Copy constructor (default is fine, but explicit to avoid warnings)
    constexpr UINT64(const UINT64 &) noexcept = default;

    // Constructor from 32-bit values
    constexpr UINT64(UINT32 h, UINT32 l) noexcept : low(l), high(h) {}

    // Constructor from single 32-bit value - explicit to avoid ambiguity
    constexpr explicit UINT64(UINT32 val) noexcept : low(val), high(0) {}

    // Constructor from native unsigned long long (for compatibility)
    constexpr UINT64(unsigned long long val) noexcept
        : low((UINT32)(val & 0xFFFFFFFFULL)),
          high((UINT32)((val >> 32) & 0xFFFFFFFFULL))
    {
    }

    // Get low 32 bits
    constexpr UINT32 Low() const noexcept { return low; }

    // Get high 32 bits
    constexpr UINT32 High() const noexcept { return high; }

    // Conversion to unsigned long long
    constexpr operator unsigned long long() const noexcept
    {
        return ((unsigned long long)high << 32) | (unsigned long long)low;
    }

    // Assignment operators
    constexpr UINT64 &operator=(const UINT64 &other) noexcept
    {
        low = other.low;
        high = other.high;
        return *this;
    }

    constexpr UINT64 &operator=(UINT32 val) noexcept
    {
        low = val;
        high = 0;
        return *this;
    }

    // =========================================================================
    // COMMON OPERATIONS (shared with INT64 via macros - no code duplication)
    // =========================================================================

    // Comparison operators: ==, !=, <, <=, >, >= (36 lines → 1 macro)
    DEFINE_INT64_COMPARISON_OPERATORS(UINT64)

    // =========================================================================
    // TYPE-SPECIFIC OPERATIONS (arithmetic - unique to UINT64)
    // =========================================================================

    // Arithmetic operators
    constexpr UINT64 operator+(const UINT64 &other) const noexcept
    {
        UINT32 newLow = low + other.low;
        UINT32 carry = (newLow < low) ? 1 : 0;
        UINT32 newHigh = high + other.high + carry;
        return UINT64(newHigh, newLow);
    }

    constexpr UINT64 operator-(const UINT64 &other) const noexcept
    {
        UINT32 newLow = low - other.low;
        UINT32 borrow = (low < other.low) ? 1 : 0;
        UINT32 newHigh = high - other.high - borrow;
        return UINT64(newHigh, newLow);
    }

    constexpr UINT64 operator*(const UINT64 &other) const noexcept
    {
        // 64-bit multiplication using 32-bit operations only
        // (a*2^32 + b) * (c*2^32 + d) = a*c*2^64 + (a*d + b*c)*2^32 + b*d
        // We only keep lower 64 bits, so a*c*2^64 is discarded

        // Split into 16-bit parts to avoid overflow in 32-bit multiplication
        UINT32 a0 = low & 0xFFFF;
        UINT32 a1 = low >> 16;
        UINT32 a2 = high & 0xFFFF;
        UINT32 a3 = high >> 16;

        UINT32 b0 = other.low & 0xFFFF;
        UINT32 b1 = other.low >> 16;
        UINT32 b2 = other.high & 0xFFFF;
        UINT32 b3 = other.high >> 16;

        // Result accumulator in four 32-bit parts (r0 is low 16, r1 is next 16, etc.)
        // We need to compute: sum of ai * bj * 2^(16*(i+j)) for all i,j where i+j < 4

        // Partial products for result bits [0:31] (contributes to low word)
        UINT32 p0 = a0 * b0;  // bits [0:31], contributes to r0, r1

        // Partial products for result bits [16:47]
        UINT32 p1 = a1 * b0;  // contributes to r1, r2
        UINT32 p2 = a0 * b1;  // contributes to r1, r2

        // Partial products for result bits [32:63] (contributes to high word)
        UINT32 p3 = a2 * b0;  // contributes to r2, r3
        UINT32 p4 = a1 * b1;  // contributes to r2, r3
        UINT32 p5 = a0 * b2;  // contributes to r2, r3

        // Partial products for result bits [48:63]
        UINT32 p6 = a3 * b0;  // contributes to r3
        UINT32 p7 = a2 * b1;  // contributes to r3
        UINT32 p8 = a1 * b2;  // contributes to r3
        UINT32 p9 = a0 * b3;  // contributes to r3

        // Accumulate with carry propagation
        // Start from low bits and propagate carries upward

        UINT32 r0 = p0 & 0xFFFF;
        UINT32 carry = p0 >> 16;

        // r1 = (p0 >> 16) + (p1 & 0xFFFF) + (p2 & 0xFFFF)
        UINT32 sum1 = carry + (p1 & 0xFFFF) + (p2 & 0xFFFF);
        UINT32 r1 = sum1 & 0xFFFF;
        carry = sum1 >> 16;

        // r2 = carry + (p1 >> 16) + (p2 >> 16) + (p3 & 0xFFFF) + (p4 & 0xFFFF) + (p5 & 0xFFFF)
        UINT32 sum2 = carry + (p1 >> 16) + (p2 >> 16) + (p3 & 0xFFFF) + (p4 & 0xFFFF) + (p5 & 0xFFFF);
        UINT32 r2 = sum2 & 0xFFFF;
        carry = sum2 >> 16;

        // r3 = carry + (p3 >> 16) + (p4 >> 16) + (p5 >> 16) + (p6 & 0xFFFF) + (p7 & 0xFFFF) + (p8 & 0xFFFF) + (p9 & 0xFFFF)
        UINT32 sum3 = carry + (p3 >> 16) + (p4 >> 16) + (p5 >> 16) + (p6 & 0xFFFF) + (p7 & 0xFFFF) + (p8 & 0xFFFF) + (p9 & 0xFFFF);
        UINT32 r3 = sum3 & 0xFFFF;

        UINT32 resultLow = r0 | (r1 << 16);
        UINT32 resultHigh = r2 | (r3 << 16);

        return UINT64(resultHigh, resultLow);
    }

    constexpr UINT64 operator/(const UINT64 &other) const noexcept
    {
        if (other.high == 0 && other.low == 0)
            return UINT64(0, 0); // Division by zero

        // Long division algorithm
        UINT64 quotient(0, 0);
        UINT64 remainder(0, 0);

        for (int i = 63; i >= 0; i--)
        {
            // Shift remainder left by 1
            remainder = UINT64((remainder.high << 1) | (remainder.low >> 31),
                               remainder.low << 1);

            // Set the lowest bit of remainder to bit i of this
            if (i >= 32)
            {
                if ((high >> (i - 32)) & 1)
                    remainder.low |= 1;
            }
            else
            {
                if ((low >> i) & 1)
                    remainder.low |= 1;
            }

            if (remainder >= other)
            {
                remainder = remainder - other;
                if (i >= 32)
                    quotient.high |= (UINT32)1 << (i - 32);
                else
                    quotient.low |= (UINT32)1 << i;
            }
        }

        return quotient;
    }

    constexpr UINT64 operator%(const UINT64 &other) const noexcept
    {
        if (other.high == 0 && other.low == 0)
            return UINT64(0, 0);

        // Long division algorithm - only compute remainder
        UINT64 remainder(0, 0);

        for (int i = 63; i >= 0; i--)
        {
            // Shift remainder left by 1
            remainder = UINT64((remainder.high << 1) | (remainder.low >> 31),
                               remainder.low << 1);

            // Set the lowest bit of remainder to bit i of this
            if (i >= 32)
            {
                if ((high >> (i - 32)) & 1)
                    remainder.low |= 1;
            }
            else
            {
                if ((low >> i) & 1)
                    remainder.low |= 1;
            }

            if (remainder >= other)
            {
                remainder = remainder - other;
            }
        }

        return remainder;
    }

    constexpr UINT64 operator%(UINT32 val) const noexcept
    {
        return *this % UINT64(val);
    }

    constexpr UINT64 operator%(int val) const noexcept
    {
        return *this % UINT64((unsigned long long)val);
    }

    constexpr UINT64 operator+(UINT32 val) const noexcept
    {
        UINT32 newLow = low + val;
        UINT32 carry = (newLow < low) ? 1 : 0;
        return UINT64(high + carry, newLow);
    }

    constexpr UINT64 operator-(UINT32 val) const noexcept
    {
        UINT32 newLow = low - val;
        UINT32 borrow = (low < val) ? 1 : 0;
        return UINT64(high - borrow, newLow);
    }

    constexpr UINT64 operator*(UINT32 val) const noexcept
    {
        // Multiply 64-bit by 32-bit
        // (high*2^32 + low) * val = high*val*2^32 + low*val

        // low * val
        UINT32 a0 = low & 0xFFFF;
        UINT32 a1 = low >> 16;
        UINT32 b0 = val & 0xFFFF;
        UINT32 b1 = val >> 16;

        UINT32 p0 = a0 * b0;
        UINT32 p1 = a1 * b0;
        UINT32 p2 = a0 * b1;
        UINT32 p3 = a1 * b1;

        UINT32 r0 = p0 & 0xFFFF;
        UINT32 carry = p0 >> 16;

        UINT32 sum1 = carry + (p1 & 0xFFFF) + (p2 & 0xFFFF);
        UINT32 r1 = sum1 & 0xFFFF;
        carry = sum1 >> 16;

        UINT32 sum2 = carry + (p1 >> 16) + (p2 >> 16) + (p3 & 0xFFFF);
        UINT32 r2 = sum2 & 0xFFFF;
        carry = sum2 >> 16;

        UINT32 r3 = carry + (p3 >> 16);

        UINT32 resultLow = r0 | (r1 << 16);
        UINT32 lowHigh = r2 | (r3 << 16);

        // high * val (only need lower 32 bits of result)
        UINT32 highPart = high * val + lowHigh;

        return UINT64(highPart, resultLow);
    }

    constexpr UINT64 operator/(UINT32 val) const noexcept
    {
        if (val == 0)
            return UINT64(0, 0);
        return *this / UINT64(val);
    }

    constexpr UINT64 operator/(int val) const noexcept
    {
        return *this / UINT64((unsigned long long)val);
    }

    constexpr bool operator<(UINT32 val) const noexcept
    {
        return (high == 0) ? (low < val) : false;
    }

    constexpr bool operator<=(UINT32 val) const noexcept
    {
        return (high == 0) ? (low <= val) : false;
    }

    constexpr bool operator>(UINT32 val) const noexcept
    {
        return (high != 0) ? true : (low > val);
    }

    constexpr bool operator>=(UINT32 val) const noexcept
    {
        return (high != 0) ? true : (low >= val);
    }

    constexpr bool operator==(UINT32 val) const noexcept
    {
        return (high == 0) && (low == val);
    }

    constexpr bool operator!=(UINT32 val) const noexcept
    {
        return (high != 0) || (low != val);
    }

    constexpr bool operator==(int val) const noexcept
    {
        return (high == 0) && (low == (UINT32)(unsigned long long)val);
    }

    constexpr bool operator!=(int val) const noexcept
    {
        return (high != 0) || (low != (UINT32)(unsigned long long)val);
    }

    // Bitwise operators with integer literals
    constexpr UINT64 operator&(UINT32 val) const noexcept
    {
        return *this & UINT64(val);
    }

    constexpr UINT64 operator&(int val) const noexcept
    {
        return *this & UINT64((unsigned long long)val);
    }

    constexpr UINT64 operator|(UINT32 val) const noexcept
    {
        return *this | UINT64(val);
    }

    constexpr UINT64 operator^(UINT32 val) const noexcept
    {
        return *this ^ UINT64(val);
    }

    // Bitwise operators: &, |, ^, ~ (20 lines → 1 macro)
    DEFINE_INT64_BITWISE_OPERATORS(UINT64)

    // Shift operators (unsigned-specific - not shared with INT64)
    constexpr UINT64 operator<<(int shift) const noexcept
    {
        if (shift < 0 || shift >= 64)
            return UINT64(0, 0);
        if (shift == 0)
            return *this;
        if (shift >= 32)
            return UINT64(low << (shift - 32), 0);

        return UINT64((high << shift) | (low >> (32 - shift)), low << shift);
    }

    constexpr UINT64 operator>>(int shift) const noexcept
    {
        if (shift < 0 || shift >= 64)
            return UINT64(0, 0);
        if (shift == 0)
            return *this;
        if (shift >= 32)
            return UINT64(0, high >> (shift - 32));

        return UINT64(high >> shift, (low >> shift) | (high << (32 - shift)));
    }

    // Compound assignment operators
    constexpr UINT64 &operator+=(const UINT64 &other) noexcept
    {
        UINT32 newLow = low + other.low;
        UINT32 carry = (newLow < low) ? 1 : 0;
        low = newLow;
        high = high + other.high + carry;
        return *this;
    }

    constexpr UINT64 &operator-=(const UINT64 &other) noexcept
    {
        UINT32 newLow = low - other.low;
        UINT32 borrow = (low < other.low) ? 1 : 0;
        low = newLow;
        high = high - other.high - borrow;
        return *this;
    }

    constexpr UINT64 &operator*=(const UINT64 &other) noexcept
    {
        *this = *this * other;
        return *this;
    }

    constexpr UINT64 &operator/=(const UINT64 &other) noexcept
    {
        *this = *this / other;
        return *this;
    }

    constexpr UINT64 &operator%=(const UINT64 &other) noexcept
    {
        *this = *this % other;
        return *this;
    }

    // Compound bitwise assignments: &=, |=, ^= (20 lines → 1 macro)
    DEFINE_INT64_BITWISE_ASSIGNMENTS(UINT64)

    // Shift assignment operators (unsigned-specific)
    constexpr UINT64 &operator<<=(int shift) noexcept
    {
        if (shift < 0 || shift >= 64)
        {
            high = 0;
            low = 0;
        }
        else if (shift == 0)
        {
            // Nothing to do
        }
        else if (shift >= 32)
        {
            high = low << (shift - 32);
            low = 0;
        }
        else
        {
            high = (high << shift) | (low >> (32 - shift));
            low = low << shift;
        }
        return *this;
    }

    constexpr UINT64 &operator>>=(int shift) noexcept
    {
        if (shift < 0 || shift >= 64)
        {
            high = 0;
            low = 0;
        }
        else if (shift == 0)
        {
            // Nothing to do
        }
        else if (shift >= 32)
        {
            low = high >> (shift - 32);
            high = 0;
        }
        else
        {
            low = (low >> shift) | (high << (32 - shift));
            high = high >> shift;
        }
        return *this;
    }

    // Increment/Decrement operators: ++, -- (28 lines → 1 macro)
    DEFINE_INT64_INCREMENT_DECREMENT(UINT64)
};

// Pointer types
typedef UINT64 *PUINT64;
typedef UINT64 **PPUINT64;
