#pragma once

#include "uint64.h"
#include "int64_common.h"

/**
 * INT64 - Position-independent 64-bit signed integer class
 *
 * Prevents compiler from using native 64-bit types that could generate
 * .rdata section references. All operations are implemented manually
 * using the UINT64 class for unsigned operations and handling sign properly.
 */
class INT64
{
private:
    UINT32 low;   // Lower 32 bits
    INT32 high;   // Upper 32 bits (signed for proper sign extension)

public:
    // Maximum value for INT64: 0x7FFFFFFFFFFFFFFF (9,223,372,036,854,775,807)
    static constexpr INT64 MAX() noexcept
    {
        return INT64((INT32)0x7FFFFFFF, (UINT32)0xFFFFFFFF);
    }
    static constexpr INT64 MIN() noexcept
    {
        return INT64((INT32)0x80000000, (UINT32)0x00000000);
    }
    // Default constructor
    constexpr INT64() noexcept : low(0), high(0) {}

    // Copy constructor (default is fine, but explicit to avoid warnings)
    constexpr INT64(const INT64 &) noexcept = default;

    // Constructor from 32-bit values
    constexpr INT64(INT32 h, UINT32 l) noexcept : low(l), high(h) {}

    // Constructor from single 32-bit value
    constexpr INT64(INT32 val) noexcept : low((UINT32)val), high(val < 0 ? -1 : 0) {}

    // Constructor from UINT32
    constexpr explicit INT64(UINT32 val) noexcept : low(val), high(0) {}

    // Constructor from native signed long long (for compatibility)
    constexpr INT64(signed long long val) noexcept
        : low((UINT32)(val & 0xFFFFFFFFLL)),
          high((INT32)((val >> 32) & 0xFFFFFFFFLL))
    {
    }

    // Get low 32 bits
    constexpr UINT32 Low() const noexcept { return low; }

    // Get high 32 bits
    constexpr INT32 High() const noexcept { return high; }

    // Conversion to signed long long
    constexpr operator signed long long() const noexcept
    {
        return ((signed long long)high << 32) | (signed long long)low;
    }

    // Conversion to UINT64
    constexpr operator UINT64() const noexcept
    {
        return UINT64((UINT32)high, low);
    }

    // Assignment operators
    constexpr INT64 &operator=(const INT64 &other) noexcept
    {
        low = other.low;
        high = other.high;
        return *this;
    }

    constexpr INT64 &operator=(INT32 val) noexcept
    {
        low = (UINT32)val;
        high = val < 0 ? -1 : 0;
        return *this;
    }

    // =========================================================================
    // COMMON OPERATIONS (shared with UINT64 via macros - no code duplication)
    // =========================================================================

    // Comparison operators: ==, !=, <, <=, >, >= (36 lines → 1 macro)
    DEFINE_INT64_COMPARISON_OPERATORS(INT64)

    // Additional comparison operators with INT32 (type-specific)
    constexpr bool operator<(INT32 val) const noexcept
    {
        return *this < INT64(val);
    }

    constexpr bool operator<=(INT32 val) const noexcept
    {
        INT32 valHigh = val < 0 ? -1 : 0;
        if (high != valHigh)
            return high < valHigh;
        return low <= (UINT32)val;
    }

    constexpr bool operator>(INT32 val) const noexcept
    {
        INT32 valHigh = val < 0 ? -1 : 0;
        if (high != valHigh)
            return high > valHigh;
        return low > (UINT32)val;
    }

    constexpr bool operator>=(INT32 val) const noexcept
    {
        INT32 valHigh = val < 0 ? -1 : 0;
        if (high != valHigh)
            return high > valHigh;
        return low >= (UINT32)val;
    }

    constexpr bool operator==(INT32 val) const noexcept
    {
        INT32 valHigh = val < 0 ? -1 : 0;
        return (high == valHigh) && (low == (UINT32)val);
    }

    constexpr bool operator!=(INT32 val) const noexcept
    {
        INT32 valHigh = val < 0 ? -1 : 0;
        return (high != valHigh) || (low != (UINT32)val);
    }

    // =========================================================================
    // TYPE-SPECIFIC OPERATIONS (arithmetic - unique to INT64)
    // =========================================================================

    // Arithmetic operators
    constexpr INT64 operator+(const INT64 &other) const noexcept
    {
        UINT32 newLow = low + other.low;
        UINT32 carry = (newLow < low) ? 1 : 0;
        INT32 newHigh = high + other.high + (INT32)carry;
        return INT64(newHigh, newLow);
    }

    constexpr INT64 operator-(const INT64 &other) const noexcept
    {
        UINT32 newLow = low - other.low;
        UINT32 borrow = (low < other.low) ? 1 : 0;
        INT32 newHigh = high - other.high - (INT32)borrow;
        return INT64(newHigh, newLow);
    }

    constexpr INT64 operator-() const noexcept // Unary minus
    {
        return INT64(0, 0) - *this;
    }

    constexpr INT64 operator*(const INT64 &other) const noexcept
    {
        // Convert to unsigned for multiplication, then cast back
        UINT64 a = UINT64((UINT32)high, low);
        UINT64 b = UINT64((UINT32)other.high, other.low);
        UINT64 result = a * b;

        return INT64((INT32)result.High(), result.Low());
    }

    constexpr INT64 operator/(const INT64 &other) const noexcept
    {
        if (other.high == 0 && other.low == 0)
            return INT64(0, 0); // Division by zero

        // Handle signs
        bool negResult = (high < 0) != (other.high < 0);

        // Get absolute values
        INT64 absThis = (high < 0) ? -(*this) : *this;
        INT64 absOther = (other.high < 0) ? -other : other;

        // Convert to unsigned and divide
        UINT64 dividend = UINT64((UINT32)absThis.high, absThis.low);
        UINT64 divisor = UINT64((UINT32)absOther.high, absOther.low);
        UINT64 quotient = dividend / divisor;

        INT64 result = INT64((INT32)quotient.High(), quotient.Low());
        return negResult ? -result : result;
    }

    constexpr INT64 operator%(const INT64 &other) const noexcept
    {
        if (other.high == 0 && other.low == 0)
            return INT64(0, 0);

        INT64 quotient = *this / other;
        return *this - (quotient * other);
    }

    constexpr INT64 operator%(INT32 val) const noexcept
    {
        return *this % INT64(val);
    }

    constexpr INT64 operator+(INT32 val) const noexcept
    {
        return *this + INT64(val);
    }

    constexpr INT64 operator-(INT32 val) const noexcept
    {
        return *this - INT64(val);
    }

    constexpr INT64 operator*(INT32 val) const noexcept
    {
        return *this * INT64(val);
    }

    constexpr INT64 operator/(INT32 val) const noexcept
    {
        return *this / INT64(val);
    }

    // Bitwise operators: &, |, ^, ~ (20 lines → 1 macro)
    DEFINE_INT64_BITWISE_OPERATORS(INT64)

    // Shift operators (signed-specific - different from UINT64 for right shift)
    constexpr INT64 operator<<(int shift) const noexcept
    {
        if (shift < 0 || shift >= 64)
            return INT64(0, 0);
        if (shift == 0)
            return *this;
        if (shift >= 32)
            return INT64((INT32)(low << (shift - 32)), 0);

        return INT64((high << shift) | (INT32)(low >> (32 - shift)), low << shift);
    }

    constexpr INT64 operator>>(int shift) const noexcept
    {
        if (shift < 0)
            return *this;
        if (shift >= 64)
            return INT64(high < 0 ? -1 : 0, high < 0 ? 0xFFFFFFFF : 0);
        if (shift == 0)
            return *this;
        if (shift >= 32)
            return INT64(high < 0 ? -1 : 0, (UINT32)(high >> (shift - 32)));

        return INT64(high >> shift, (low >> shift) | ((UINT32)high << (32 - shift)));
    }

    // Compound assignment operators
    constexpr INT64 &operator+=(const INT64 &other) noexcept
    {
        UINT32 newLow = low + other.low;
        UINT32 carry = (newLow < low) ? 1 : 0;
        low = newLow;
        high = high + other.high + (INT32)carry;
        return *this;
    }

    constexpr INT64 &operator-=(const INT64 &other) noexcept
    {
        UINT32 newLow = low - other.low;
        UINT32 borrow = (low < other.low) ? 1 : 0;
        low = newLow;
        high = high - other.high - (INT32)borrow;
        return *this;
    }

    constexpr INT64 &operator*=(const INT64 &other) noexcept
    {
        *this = *this * other;
        return *this;
    }

    constexpr INT64 &operator/=(const INT64 &other) noexcept
    {
        *this = *this / other;
        return *this;
    }

    constexpr INT64 &operator%=(const INT64 &other) noexcept
    {
        *this = *this % other;
        return *this;
    }

    // Compound bitwise assignments: &=, |=, ^= (20 lines → 1 macro)
    DEFINE_INT64_BITWISE_ASSIGNMENTS(INT64)

    // Shift assignment operators (signed-specific)
    constexpr INT64 &operator<<=(int shift) noexcept
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
            high = (INT32)(low << (shift - 32));
            low = 0;
        }
        else
        {
            high = (high << shift) | (INT32)(low >> (32 - shift));
            low = low << shift;
        }
        return *this;
    }

    constexpr INT64 &operator>>=(int shift) noexcept
    {
        if (shift < 0)
        {
            // Nothing to do
        }
        else if (shift >= 64)
        {
            high = high < 0 ? -1 : 0;
            low = high < 0 ? 0xFFFFFFFF : 0;
        }
        else if (shift == 0)
        {
            // Nothing to do
        }
        else if (shift >= 32)
        {
            low = (UINT32)(high >> (shift - 32));
            high = high < 0 ? -1 : 0;
        }
        else
        {
            low = (low >> shift) | ((UINT32)high << (32 - shift));
            high = high >> shift;
        }
        return *this;
    }

    // Increment/Decrement operators: ++, -- (28 lines → 1 macro)
    DEFINE_INT64_INCREMENT_DECREMENT(INT64)
};

// Pointer types
typedef INT64 *PINT64;
typedef INT64 **PPINT64;
