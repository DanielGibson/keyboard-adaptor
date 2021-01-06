/*
 * Some helper functions for Arduino Projects
 *
 * HexPrinter::printTo() is based on PrintHex() from the USB Host Shield Library 2.0
 * Copyright (C) 2011 Circuits At Home, LTD. All rights reserved.
 * Licensed under GPLv2 or later
 *
 * The *remaining* code in this file is
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
#if 1
		// stolen from usb host shield lib's printhex.h
		int num_nibbles = sizeof (INT_T) * 2;
		size_t ret = 0;

		do {
			char v = 48 + (((val >> (num_nibbles - 1) * 4)) & 0x0f);
			if(v > 57)
				v += 7;
			ret += p.print(v);
		} while(--num_nibbles);

		return ret;
#else
		return p.print(val, 16); // this doesn't prepend '0' nibbles
#endif
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
