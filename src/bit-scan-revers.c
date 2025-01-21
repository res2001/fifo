#include "bit-scan-revers.h"

#define DeBruijn64                  0x03F79D71B4CB0A89ULL

static inline uint64_t swar64(uint64_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x;
}

/**
 * https://www.chessprogramming.org/index.php?title=BitScan#De_Bruijn_Multiplication_2
 * bitScanReverse
 * @authors Kim Walisch, Mark Dickinson
 * @param bb bitboard to scan
 * @precondition bb != 0
 * @return index (0..63) of most significant one bit. If bb == 0 - return -1.
 */
int8_t bitScanReverse(uint64_t bb)
{
    static const int8_t index64[64] = {
        0, 47,  1, 56, 48, 27,  2, 60,
        57, 49, 41, 37, 28, 16,  3, 61,
        54, 58, 35, 52, 50, 42, 21, 44,
        38, 32, 29, 23, 17, 11,  4, 62,
        46, 55, 26, 59, 40, 36, 15, 53,
        34, 51, 20, 43, 31, 22, 10, 45,
        25, 39, 14, 33, 19, 30,  9, 24,
        13, 18,  8, 12,  7,  6,  5, 63
    };
    if(bb == 0) return -1;
    bb = swar64(bb);
    return index64[(bb * DeBruijn64) >> 58];
}

/*
 * http://aggregate.org/MAGIC/#Most%20Significant%201%20Bit
 * Return: Most Significant 1 Bit
 */
uint64_t msb64(uint64_t x)
{
    x = swar64(x);
    return x & ~(x >> 1);
}
