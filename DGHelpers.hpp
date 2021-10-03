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

namespace dg_impl
{

template<typename INT_T>
struct HexPrinter final : public Printable
{
	INT_T val;

	HexPrinter(const INT_T& x) : val(x){}
	virtual ~HexPrinter(){}

	size_t printTo(Print& p) const override
	{
		size_t ret = 0;

		// Number of nibbles
		size_t numnibbles = sizeof(INT_T) * 2;

		// Assume that we'll get 64 bits at max.
		uint64_t converted = (uint64_t)val;

		// For little endian only!
		for (int n = numnibbles; n != 0; n--) {
			// Extract nibble...
			uint32_t nibble = (converted >> (4 * (n - 1))) & 0x0F;

			// ...and print it.
			if (nibble <= 9) {
				ret += p.print((char)nibble + '0');
			} else {
				ret += p.print((char)nibble - 10 + 'A');
			}
		}

		return ret;
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
