#pragma once

#include "runtime.h"

class DoubleTests
{
public:
	static BOOL RunAll()
	{
		BOOL allPassed = TRUE;

		Logger::Info<WCHAR>(L"Running DOUBLE Tests..."_embed);

		// Test 1: Construction
		if (!TestConstruction())
		{
			allPassed = FALSE;
			Logger::Error<WCHAR>(L"  FAILED: Construction"_embed);
		}
		else
		{
			Logger::Info<WCHAR>(L"  PASSED: Construction"_embed);
		}

		// Test 2: Integer to DOUBLE conversion
		if (!TestIntToDouble())
		{
			allPassed = FALSE;
			Logger::Error<WCHAR>(L"  FAILED: Integer to DOUBLE"_embed);
		}
		else
		{
			Logger::Info<WCHAR>(L"  PASSED: Integer to DOUBLE"_embed);
		}

		// Test 3: DOUBLE to integer conversion
		if (!TestDoubleToInt())
		{
			allPassed = FALSE;
			Logger::Error<WCHAR>(L"  FAILED: DOUBLE to integer"_embed);
		}
		else
		{
			Logger::Info<WCHAR>(L"  PASSED: DOUBLE to integer"_embed);
		}

		// Test 4: Arithmetic operations
		if (!TestArithmetic())
		{
			allPassed = FALSE;
			Logger::Error<WCHAR>(L"  FAILED: Arithmetic"_embed);
		}
		else
		{
			Logger::Info<WCHAR>(L"  PASSED: Arithmetic"_embed);
		}

		// Test 5: Comparisons
		if (!TestComparisons())
		{
			allPassed = FALSE;
			Logger::Error<WCHAR>(L"  FAILED: Comparisons"_embed);
		}
		else
		{
			Logger::Info<WCHAR>(L"  PASSED: Comparisons"_embed);
		}

		// Test 6: Unary negation
		if (!TestNegation())
		{
			allPassed = FALSE;
			Logger::Error<WCHAR>(L"  FAILED: Negation"_embed);
		}
		else
		{
			Logger::Info<WCHAR>(L"  PASSED: Negation"_embed);
		}

		// Test 7: Embedded double literals
		if (!TestEmbeddedLiterals())
		{
			allPassed = FALSE;
			Logger::Error<WCHAR>(L"  FAILED: Embedded literals"_embed);
		}
		else
		{
			Logger::Info<WCHAR>(L"  PASSED: Embedded literals"_embed);
		}

		// Test 8: Edge cases (zero, small values)
		if (!TestEdgeCases())
		{
			allPassed = FALSE;
			Logger::Error<WCHAR>(L"  FAILED: Edge cases"_embed);
		}
		else
		{
			Logger::Info<WCHAR>(L"  PASSED: Edge cases"_embed);
		}

		// Test 9: Array initialization and formatting
		if (!TestArrayFormatting())
		{
			allPassed = FALSE;
			Logger::Error<WCHAR>(L"  FAILED: Array formatting"_embed);
		}
		else
		{
			Logger::Info<WCHAR>(L"  PASSED: Array formatting"_embed);
		}

		if (allPassed)
		{
			Logger::Info<WCHAR>(L"All DOUBLE tests passed!"_embed);
		}
		else
		{
			Logger::Error<WCHAR>(L"Some DOUBLE tests failed!"_embed);
		}

		return allPassed;
	}

private:
	static BOOL TestConstruction()
	{
		// Default constructor (zero)
		DOUBLE a;
		if (a.Bits().High() != 0 || a.Bits().Low() != 0)
			return FALSE;

		// Construction from embedded double
		DOUBLE b = 1.0;
		// IEEE-754: 1.0 = 0x3FF0000000000000
		if (b.Bits().High() != 0x3FF00000 || b.Bits().Low() != 0x00000000)
			return FALSE;

		// Construction from bit pattern
		DOUBLE c(UINT64(0x40000000, 0x00000000)); // 2.0
		double native_c = (double)c;
		if (native_c != (double)2.0_embed)
			return FALSE;

		// Construction from two 32-bit values
		DOUBLE d(0x3FF00000, 0x00000000); // 1.0
		double native_d = (double)d;
		if (native_d != (double)1.0_embed)
			return FALSE;

		return TRUE;
	}

	static BOOL TestIntToDouble()
	{
		// Zero
		DOUBLE zero(INT32(0));
		if (zero.Bits().High() != 0 || zero.Bits().Low() != 0)
			return FALSE;

		// Positive integer
		DOUBLE one(INT32(1));
		double native_one = (double)one;
		if (native_one != (double)1.0_embed)
			return FALSE;

		// Larger positive integer
		DOUBLE hundred(INT32(100));
		double native_hundred = (double)hundred;
		if (native_hundred != (double)100.0_embed)
			return FALSE;

		// Negative integer
		DOUBLE neg_one(INT32(-1));
		double native_neg = (double)neg_one;
		if (native_neg != (double)-1.0_embed)
			return FALSE;

		// Power of 2
		DOUBLE pow2(INT32(1024));
		double native_pow2 = (double)pow2;
		if (native_pow2 != (double)1024.0_embed)
			return FALSE;

		return TRUE;
	}

	static BOOL TestDoubleToInt()
	{
		// 1.0 -> 1
		DOUBLE one = 1.0;
		INT32 int_one = (INT32)one;
		if (int_one != 1)
			return FALSE;

		// 1.9 -> 1 (truncation)
		DOUBLE one_nine = 1.9;
		INT32 int_one_nine = (INT32)one_nine;
		if (int_one_nine != 1)
			return FALSE;

		// 100.5 -> 100
		DOUBLE hundred = 100.5;
		INT32 int_hundred = (INT32)hundred;
		if (int_hundred != 100)
			return FALSE;

		// -1.0 -> -1
		DOUBLE neg_one = -1.0;
		INT32 int_neg_one = (INT32)neg_one;
		if (int_neg_one != -1)
			return FALSE;

		// 0.5 -> 0
		DOUBLE half = 0.5;
		INT32 int_half = (INT32)half;
		if (int_half != 0)
			return FALSE;

		return TRUE;
	}

	static BOOL TestArithmetic()
	{
		// Addition
		DOUBLE a = 2.0;
		DOUBLE b = 3.0;
		DOUBLE c = a + b;
		double native_c = (double)c;
		if (native_c != (double)5.0_embed)
			return FALSE;

		// Subtraction
		DOUBLE d = b - a;
		double native_d = (double)d;
		if (native_d != (double)1.0_embed)
			return FALSE;

		// Multiplication
		DOUBLE e = a * b;
		double native_e = (double)e;
		if (native_e != (double)6.0_embed)
			return FALSE;

		// Division
		DOUBLE six = 6.0;
		DOUBLE f = six / a;
		double native_f = (double)f;
		if (native_f != (double)3.0_embed)
			return FALSE;

		// Compound assignment +=
		DOUBLE g = 10.0;
		g += a;
		if ((double)g != (double)12.0_embed)
			return FALSE;

		// Compound assignment -=
		g -= a;
		if ((double)g != (double)10.0_embed)
			return FALSE;

		// Compound assignment *=
		g *= a;
		if ((double)g != (double)20.0_embed)
			return FALSE;

		// Compound assignment /=
		g /= a;
		if ((double)g != (double)10.0_embed)
			return FALSE;

		return TRUE;
	}

	static BOOL TestComparisons()
	{
		DOUBLE a = 1.0;
		DOUBLE b = 2.0;
		DOUBLE c = 1.0;

		// Equality
		if (!(a == c))
			return FALSE;
		if (a == b)
			return FALSE;

		// Not equal
		if (a != c)
			return FALSE;
		if (!(a != b))
			return FALSE;

		// Less than
		if (!(a < b))
			return FALSE;
		if (b < a)
			return FALSE;
		if (a < c)
			return FALSE;

		// Less than or equal
		if (!(a <= b))
			return FALSE;
		if (!(a <= c))
			return FALSE;
		if (b <= a)
			return FALSE;

		// Greater than
		if (!(b > a))
			return FALSE;
		if (a > b)
			return FALSE;
		if (a > c)
			return FALSE;

		// Greater than or equal
		if (!(b >= a))
			return FALSE;
		if (!(a >= c))
			return FALSE;
		if (a >= b)
			return FALSE;

		return TRUE;
	}

	static BOOL TestNegation()
	{
		// Negate positive
		DOUBLE pos = 5.0;
		DOUBLE neg = -pos;
		double native_neg = (double)neg;
		if (native_neg != (double)-5.0_embed)
			return FALSE;

		// Negate negative
		DOUBLE neg2 = -3.0;
		DOUBLE pos2 = -neg2;
		double native_pos2 = (double)pos2;
		if (native_pos2 != (double)3.0_embed)
			return FALSE;

		// Double negation
		DOUBLE val = 7.0;
		DOUBLE dbl_neg = -(-val);
		if ((double)dbl_neg != (double)7.0_embed)
			return FALSE;

		return TRUE;
	}

	static BOOL TestEmbeddedLiterals()
	{
		// Test _embed suffix for double literals
		DOUBLE a = 1.5;
		if ((double)a != (double)1.5_embed)
			return FALSE;

		DOUBLE b = 3.14159;
		double native_b = (double)b;
		// Allow small tolerance for floating point
		if (native_b < (double)3.14158_embed || native_b > (double)3.14160_embed)
			return FALSE;

		DOUBLE c = 0.5;
		if ((double)c != (double)0.5_embed)
			return FALSE;

		DOUBLE d = 100.0;
		if ((double)d != (double)100.0_embed)
			return FALSE;

		// Negative embedded
		DOUBLE e = -2.5;
		if ((double)e != (double)-2.5_embed)
			return FALSE;

		return TRUE;
	}

	static BOOL TestEdgeCases()
	{
		// Zero
		DOUBLE zero = 0.0;
		if ((double)zero != (double)0.0_embed)
			return FALSE;

		// Adding zero
		DOUBLE val = 5.0;
		DOUBLE result = val + zero;
		if ((double)result != (double)5.0_embed)
			return FALSE;

		// Multiplying by zero
		result = val * zero;
		if ((double)result != (double)0.0_embed)
			return FALSE;

		// Multiplying by one
		DOUBLE one = 1.0;
		result = val * one;
		if ((double)result != (double)5.0_embed)
			return FALSE;

		// Small values
		DOUBLE small = 0.001;
		DOUBLE thousand = 1000.0;
		result = small * thousand;
		double native_result = (double)result;
		// Should be approximately 1.0
		if (native_result < (double)0.999_embed || native_result > (double)1.001_embed)
			return FALSE;

		return TRUE;
	}

	static BOOL TestArrayFormatting()
	{
		// Test that DOUBLE arrays can be properly initialized and formatted
		// This ensures the varargs casting works correctly with Logger
		DOUBLE testArray[] = { 1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9, 10.1 };

		// Verify array initialization by checking that values are non-zero
		// We can't do exact comparisons without generating .rdata, so just verify they're initialized
		for (INT64 i = 0; i < 10; i++)
		{
			INT64 index = i;
			DOUBLE val = testArray[(signed long long)index];
			// Just verify non-zero (all values are > 1.0)
			if (val.Bits().High() == 0 && val.Bits().Low() == 0)
				return FALSE;
		}

		// Test formatting output (this also tests that the values are properly passed through varargs)
		for (INT32 i = 0; i < 10; i++)
		{
			// The Logger::Info call exercises the varargs casting
			DOUBLE val = testArray[i];
			double native_val = (double)val;
			Logger::Info<WCHAR>(L"    DOUBLE Array Value [%d]: %f"_embed, i, native_val);
		}

		return TRUE;
	}
};
