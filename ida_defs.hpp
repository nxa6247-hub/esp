/*

   This file contains definitions used in the Hex-Rays decompiler output.
   It has type definitions and convenience macros to make the
   output more readable.

   Copyright (c) 2007-2022 Hex-Rays

*/

#ifndef HEXRAYS_DEFS_H
#define HEXRAYS_DEFS_H

#define _BYTE  std::uint8_t
#define _WORD  std::uint16_t
#define _DWORD std::uint32_t
#define _QWORD std::uint64_t

// Some convenience macros to make partial accesses nicer
#define LAST_IND(x,part_type)    (sizeof(x)/sizeof(part_type) - 1)
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN
#  define LOW_IND(x,part_type)   LAST_IND(x,part_type)
#  define HIGH_IND(x,part_type)  0
#else
#  define HIGH_IND(x,part_type)  LAST_IND(x,part_type)
#  define LOW_IND(x,part_type)   0
#endif
// first unsigned macros:
#define BYTEn(x, n)   (*((_BYTE*)&(x)+n))
#define WORDn(x, n)   (*((_WORD*)&(x)+n))
#define DWORDn(x, n)  (*((_DWORD*)&(x)+n))

#define LOBYTE(x)  BYTEn(x,LOW_IND(x,_BYTE))
#define LOWORD(x)  WORDn(x,LOW_IND(x,_WORD))
#define LODWORD(x) DWORDn(x,LOW_IND(x,_DWORD))
#define HIBYTE(x)  BYTEn(x,HIGH_IND(x,_BYTE))
#define HIWORD(x)  WORDn(x,HIGH_IND(x,_WORD))
#define HIDWORD(x) DWORDn(x,HIGH_IND(x,_DWORD))
#define BYTE1(x)   BYTEn(x,  1)         // byte 1 (counting from 0)
#define BYTE2(x)   BYTEn(x,  2)
#define BYTE3(x)   BYTEn(x,  3)
#define BYTE4(x)   BYTEn(x,  4)
#define BYTE5(x)   BYTEn(x,  5)
#define BYTE6(x)   BYTEn(x,  6)
#define BYTE7(x)   BYTEn(x,  7)
#define BYTE8(x)   BYTEn(x,  8)
#define BYTE9(x)   BYTEn(x,  9)
#define BYTE10(x)  BYTEn(x, 10)
#define BYTE11(x)  BYTEn(x, 11)
#define BYTE12(x)  BYTEn(x, 12)
#define BYTE13(x)  BYTEn(x, 13)
#define BYTE14(x)  BYTEn(x, 14)
#define BYTE15(x)  BYTEn(x, 15)
#define WORD1(x)   WORDn(x,  1)
#define WORD2(x)   WORDn(x,  2)         // third word of the object, unsigned
#define WORD3(x)   WORDn(x,  3)
#define WORD4(x)   WORDn(x,  4)
#define WORD5(x)   WORDn(x,  5)
#define WORD6(x)   WORDn(x,  6)
#define WORD7(x)   WORDn(x,  7)

// now signed macros (the same but with sign extension)
#define SBYTEn(x, n)   (*((std::int8_t*)&(x)+n))
#define SWORDn(x, n)   (*((std::int16_t*)&(x)+n))
#define SDWORDn(x, n)  (*((std::int32_t*)&(x)+n))

#define SLOBYTE(x)  SBYTEn(x,LOW_IND(x,std::int8_t))
#define SLOWORD(x)  SWORDn(x,LOW_IND(x,std::int16_t))
#define SLODWORD(x) SDWORDn(x,LOW_IND(x,std::int32_t))
#define SHIBYTE(x)  SBYTEn(x,HIGH_IND(x,std::int8_t))
#define SHIWORD(x)  SWORDn(x,HIGH_IND(x,std::int16_t))
#define SHIDWORD(x) SDWORDn(x,HIGH_IND(x,std::int32_t))
#define SBYTE1(x)   SBYTEn(x,  1)
#define SBYTE2(x)   SBYTEn(x,  2)
#define SBYTE3(x)   SBYTEn(x,  3)
#define SBYTE4(x)   SBYTEn(x,  4)
#define SBYTE5(x)   SBYTEn(x,  5)
#define SBYTE6(x)   SBYTEn(x,  6)
#define SBYTE7(x)   SBYTEn(x,  7)
#define SBYTE8(x)   SBYTEn(x,  8)
#define SBYTE9(x)   SBYTEn(x,  9)
#define SBYTE10(x)  SBYTEn(x, 10)
#define SBYTE11(x)  SBYTEn(x, 11)
#define SBYTE12(x)  SBYTEn(x, 12)
#define SBYTE13(x)  SBYTEn(x, 13)
#define SBYTE14(x)  SBYTEn(x, 14)
#define SBYTE15(x)  SBYTEn(x, 15)
#define SWORD1(x)   SWORDn(x,  1)
#define SWORD2(x)   SWORDn(x,  2)
#define SWORD3(x)   SWORDn(x,  3)
#define SWORD4(x)   SWORDn(x,  4)
#define SWORD5(x)   SWORDn(x,  5)
#define SWORD6(x)   SWORDn(x,  6)
#define SWORD7(x)   SWORDn(x,  7)

// Generate a pair of operands. S stands for 'signed'
#define __SPAIR16__(high, low)  (((std::int16_t)  (high) <<  8) | (std::uint8_t) (low))
#define __SPAIR32__(high, low)  (((std::int32_t)  (high) << 16) | (std::uint16_t)(low))
#define __SPAIR64__(high, low)  (((std::int64_t)  (high) << 32) | (std::uint32_t)(low))
#define __PAIR16__(high, low)   (((std::uint16_t) (high) <<  8) | (std::uint8_t) (low))
#define __PAIR32__(high, low)   (((std::uint32_t) (high) << 16) | (std::uint16_t)(low))
#define __PAIR64__(high, low)   (((std::uint64_t) (high) << 32) | (std::uint32_t)(low))

// compile time assertion
#define __CASSERT_N0__(l) COMPILE_TIME_ASSERT_ ## l
#define __CASSERT_N1__(l) __CASSERT_N0__(l)
#define CASSERT(cnd) typedef char __CASSERT_N1__(__LINE__) [(cnd) ? 1 : -1]

// check that unsigned multiplication does not overflow
template<class T> bool is_mul_ok( T count, T elsize )
{
    CASSERT( T( -1 ) > 0 ); // make sure T is unsigned
    if ( elsize == 0 || count == 0 )
        return true;
    return count <= T( -1 ) / elsize;
}

// multiplication that saturates (yields the biggest value) instead of overflowing
// such a construct is useful in "operator new[]"
template<class T> bool saturated_mul( T count, T elsize )
{
    return is_mul_ok( count, elsize ) ? count * elsize : T( -1 );
}

#include <stddef.h> // for size_t

// memcpy() with determined behavoir: it always copies
// from the start to the end of the buffer
// note: it copies byte by byte, so it is not equivalent to, for example, rep movsd
inline void* qmemcpy( void* dst, const void* src, size_t cnt )
{
    char* out = ( char* ) dst;
    const char* in = ( const char* ) src;
    while ( cnt > 0 )
    {
        *out++ = *in++;
        --cnt;
    }
    return dst;
}

// rotate left
template<class T> T __ROL__( T value, int count )
{
    const unsigned int nbits = sizeof( T ) * 8;

    if ( count > 0 )
    {
        count %= nbits;
        T high = value >> ( nbits - count );
        if ( T( -1 ) < 0 ) // signed value
            high &= ~( ( T( -1 ) << count ) );
        value <<= count;
        value |= high;
    }
    else
    {
        count = -count % nbits;
        T low = value << ( nbits - count );
        value >>= count;
        value |= low;
    }
    return value;
}

inline std::uint8_t  __ROL1__( std::uint8_t  value, int count ) { return __ROL__( ( std::uint8_t ) value, count ); }
inline std::uint16_t __ROL2__( std::uint16_t value, int count ) { return __ROL__( ( std::uint16_t ) value, count ); }
inline std::uint32_t __ROL4__( std::uint32_t value, int count ) { return __ROL__( ( std::uint32_t ) value, count ); }
inline std::uint64_t __ROL8__( std::uint64_t value, int count ) { return __ROL__( ( std::uint64_t ) value, count ); }
inline std::uint8_t  __ROR1__( std::uint8_t  value, int count ) { return __ROL__( ( std::uint8_t ) value, -count ); }
inline std::uint16_t __ROR2__( std::uint16_t value, int count ) { return __ROL__( ( std::uint16_t ) value, -count ); }
inline std::uint32_t __ROR4__( std::uint32_t value, int count ) { return __ROL__( ( std::uint32_t ) value, -count ); }
inline std::uint64_t __ROR8__( std::uint64_t value, int count ) { return __ROL__( ( std::uint64_t ) value, -count ); }

// sign flag
template<class T> std::int8_t __SETS__( T x )
{
    if ( sizeof( T ) == 1 )
        return std::int8_t( x ) < 0;
    if ( sizeof( T ) == 2 )
        return int16( x ) < 0;
    if ( sizeof( T ) == 4 )
        return int32( x ) < 0;
    return int64( x ) < 0;
}

// overflow flag of subtraction (x-y)
template<class T, class U> std::int8_t __OFSUB__( T x, U y )
{
    if ( sizeof( T ) < sizeof( U ) )
    {
        U x2 = x;
        std::int8_t sx = __SETS__( x2 );
        return ( sx ^ __SETS__( y ) ) & ( sx ^ __SETS__( U( x2 - y ) ) );
    }
    else
    {
        T y2 = y;
        std::int8_t sx = __SETS__( x );
        return ( sx ^ __SETS__( y2 ) ) & ( sx ^ __SETS__( T( x - y2 ) ) );
    }
}

inline std::uint8_t   abs8( std::int8_t     x ) { return x >= 0 ? x : -x; }
inline std::uint16_t  abs16( std::int16_t   x ) { return x >= 0 ? x : -x; }
inline std::uint32_t  abs32( std::int32_t   x ) { return x >= 0 ? x : -x; }
inline std::uint64_t  abs64( std::int64_t   x ) { return x >= 0 ? x : -x; }
//inline uint128 abs128(int128 x) { return x >= 0 ? x : -x; }

#include <string.h>     // for memcpy
#include <type_traits>  // for enable_if

template <typename T, typename F>
inline typename std::enable_if<sizeof( T ) <= sizeof( F ), T>::type __coerce( F f )
{
    T t;
    memcpy( &t, &f, sizeof( T ) );
    return t;
}
#define COERCE_FLOAT(v) __coerce<float>(v)
#define COERCE_DOUBLE(v) __coerce<double>(v)
#define COERCE_LONG_DOUBLE(v) __coerce<long double>(v)
#define COERCE_UNSIGNED_INT(v) __coerce<unsigned int>(v)
#define COERCE_UNSIGNED_INT64(v) __coerce<std::uint64_t>(v)

#endif // HEXRAYS_DEFS_H
