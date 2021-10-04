/*
 * Some helper functions for Arduino Projects
 *
 * Copyright (C) 2021 Daniel Gibson
 *
 *  This software is dual-licensed to the public domain and under the following
 *  license: you are granted a perpetual, irrevocable license to copy, modify,
 *  publish, and distribute this file as you see fit.
 *  No warranty implied; use at your own risk.
 *
 * So you can do whatever you want with this code (except for the HexPrinter::printTo()),
 * including copying it (or parts of it) into your own source.
 * No need to mention me or this "license" in your code or docs, even though
 * it would be appreciated, of course.
 */

#ifndef DGHELPERS_HPP_
#define DGHELPERS_HPP_

#include "Arduino.h"

// the stuff in this namespace is just implementation details
// below it are the more generally useful functions
namespace dg_impl
{

// UnsignedForSize<sizeof(MyIntegerType)>::type is the unsigned integer type
// for (with the same size as) MyIntegerType
template<int size> struct UnsignedForSize;
template<> struct UnsignedForSize<1> { typedef uint8_t type; };
template<> struct UnsignedForSize<2> { typedef uint16_t type; };
template<> struct UnsignedForSize<4> { typedef uint32_t type; };
template<> struct UnsignedForSize<8> { typedef uint64_t type; };

template<typename INT_T>
struct HexPrinter final : public Printable
{
	INT_T val;

	HexPrinter(const INT_T& x) : val(x){}
	virtual ~HexPrinter(){}

	size_t printTo(Print& p) const override
	{
		// Number of nibbles
		int numnibbles = sizeof(INT_T) * 2;

		// always print value as unsigned (like printf's %x)
		typedef typename UnsignedForSize<sizeof(INT_T)>::type UINT_T;
		UINT_T converted = (UINT_T)val;

		for (int n = numnibbles; n != 0; n--) {
			// Extract nibble...
			char nibble = (converted >> (4 * (n - 1))) & 0x0F;

			// ...and print it.
			if (nibble <= 9) {
				p.print(nibble + '0');
			} else {
				p.print(nibble - 10 + 'A');
			}
		}

		return numnibbles;
	}
};

#ifndef CDC_DISABLED
inline void _PrintHelper()
{}

template<typename T1, typename... Ts>
inline void _PrintHelper(const T1& arg1, const Ts&... args)
{
	Serial.print(arg1);
	_PrintHelper(args...);
}
#endif // !CDC_DISABLED

} //namespace dg_impl



// returns a class derived from Printable that will print x (which should be an integer type) as hex
// Serial.print(AsHex(42)); will print 42 in hex (without 0x prefix!)
// And in contrast to Serial.print(var, 16), it will always print all bytes of an int,
// even leading 0-bytes, so (int16)1 will be printed as "0001" instead of just "01"
// more useful for PrintAll("fourtytwo = ", AsHex(42), " var = ", AsHex(var));
template<typename INT_T>
inline dg_impl::HexPrinter<INT_T> AsHex(INT_T x)
{
	return dg_impl::HexPrinter<INT_T>(x);
}

// suppress warnings about unused parameters in Print(ln)All() when CDC is disabled
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

// prints all arguments to the serial console
// each argument must be valid for Serial.print()
template<typename... T>
inline void PrintAll(const T&... args)
{
#ifndef CDC_DISABLED
	dg_impl::_PrintHelper(args...);
#endif
}

// prints all arguments to the serial console, ending with a newline
// each argument must be valid for Serial.print()
template<typename... T>
inline void PrintlnAll(const T&... args)
{
#ifndef CDC_DISABLED
	dg_impl::_PrintHelper(args...);
	Serial.println();
#endif
}

// re-enable unused parameter warnings
#pragma GCC diagnostic pop

// returns index of elemToSearch in arr
// returns -1 if not found
template<typename T>
int FindInArray(T elemToSearch, const T* arr, int len)
{
	for(int i=0; i<len; ++i)
	{
		if(arr[i] == elemToSearch)
			return i;
	}
	return -1;
}


#endif /* DGHELPERS_HPP_ */
