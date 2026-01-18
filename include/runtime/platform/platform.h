#pragma once

#include "primitives.h"
#include "uint64.h"
#include "int64.h"
#include "double.h"
#include "embedded_double.h"
#include "embedded_string.h"

PVOID GetInstructionAddress(VOID);
PCHAR ReversePatternSearch(PCHAR ip, const CHAR *pattern, UINT32 len);

// Function to get export address from PEB modules
PVOID ResolveExportAddressFromPebModule(USIZE moduleNameHash, USIZE functionNameHash);

#define GetEnvironmentBaseAddress() (USIZE)(GetCurrentPEB()->SubSystemData)
#define SetEnvironmentBaseAddress(v) (GetCurrentPEB()->SubSystemData = (PVOID)(v))

// Environment data structure for PIC-style rebasing (used on Windows i386)
typedef struct _ENVIRONMENT_DATA
{
    PVOID BaseAddress;
    BOOL ShouldRelocate;
} ENVIRONMENT_DATA, *PENVIRONMENT_DATA;

#if defined(PLATFORM_WINDOWS_I386)
#define IMAGE_LINK_BASE ((USIZE)0x401000)
#define GetEnvironmentData() ((PENVIRONMENT_DATA)(GetCurrentPEB()->SubSystemData))

NOINLINE VOID Initialize(PENVIRONMENT_DATA envData);
PVOID PerformRelocation(PVOID p);

#else
#define PerformRelocation(p) (p)
#define Initialize(envData) ((VOID)envData)
#endif

// Entry point macro
#define ENTRYPOINT extern "C" __attribute__((noreturn))

// Cross-platform exit process function
NO_RETURN VOID ExitProcess(USIZE code);

template <USIZE Bytes>
struct UINT_OF_SIZE;
template <>
struct UINT_OF_SIZE<1>
{
    using type = UINT8;
};
template <>
struct UINT_OF_SIZE<2>
{
    using type = UINT16;
};
template <>
struct UINT_OF_SIZE<4>
{
    using type = UINT32;
};
template <>
struct UINT_OF_SIZE<8>
{
    using type = UINT64;
};

template <typename TChar, USIZE N>
class STACK_ARRAY_STORAGE
{
public:
    static constexpr USIZE Count = N;
    static constexpr USIZE SizeBytes = N * sizeof(TChar);

private:
    static constexpr USIZE WordBytes = sizeof(USIZE);
    static constexpr USIZE WordCount = (SizeBytes + WordBytes - 1) / WordBytes;

    alignas(USIZE) USIZE words[WordCount]{};

    consteval VOID SetByte(USIZE byteIndex, UINT8 v)
    {
        const USIZE wi = byteIndex / WordBytes;
        const USIZE sh = (byteIndex % WordBytes) * 8u;

        const USIZE mask = (USIZE)0xFFu << sh;
        words[wi] = (words[wi] & ~mask) | ((USIZE)v << sh);
    }

    consteval UINT8 GetByte(USIZE byteIndex) const
    {
        const USIZE wi = byteIndex / WordBytes;
        const USIZE sh = (byteIndex % WordBytes) * 8u;
        return (UINT8)((words[wi] >> sh) & (USIZE)0xFFu);
    }

public:
    consteval STACK_ARRAY_STORAGE(const TChar (&src)[N]) : words{}
    {
        using U = typename UINT_OF_SIZE<sizeof(TChar)>::type;

        for (USIZE i = 0; i < N; ++i)
        {
            const U v = (U)src[i];

            for (USIZE b = 0; b < sizeof(TChar); ++b)
            {
                const UINT8 data = (UINT8)((v >> (b * 8u)) & (U)0xFFu);
                SetByte(i * sizeof(TChar) + b, data);
            }
        }
    }

    constexpr TChar operator[](USIZE index) const
    {
        using U = typename UINT_OF_SIZE<sizeof(TChar)>::type;

        U v = 0;
        const USIZE base = index * sizeof(TChar);

        for (USIZE b = 0; b < sizeof(TChar); ++b)
            v |= (U)GetByte(base + b) << (b * 8u);

        return (TChar)v;
    }
    constexpr operator const VOID *() const
    {
        return (const VOID *)words;
    }
    constexpr const USIZE *Words() const { return words; }
    static constexpr USIZE WordsCount = WordCount;
};

template <typename TChar, USIZE N>
consteval auto MakeArrayStorage(const TChar (&arr)[N])
{
    return STACK_ARRAY_STORAGE<TChar, N>(arr);
}
