
#ifndef DGHELPERS_HPP_
#define DGHELPERS_HPP_

#include "keyboardwrapper.h"

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
		return p.print(val, 16); // this doesn't prepend 0 nibbles
#endif
	}
};

inline void _PrintHelper()
{}

template<typename T1, typename... Ts>
inline void _PrintHelper(const T1& arg1, const Ts&... args)
{
	Serial.print(arg1);
	_PrintHelper(args...);
}

} //namespace dg_impl

// returns a class derived from Printable that will print x (which should be an integer type) as hex
// Serial.print(AsHex(42)); will print 42 in hex (without 0x prefix!)
// more useful for PrintAll("fourtytwo = ", AsHex(42), " var = ", AsHex(var));
template<typename INT_T>
inline dg_impl::HexPrinter<INT_T> AsHex(INT_T x)
{
	return dg_impl::HexPrinter<INT_T>(x);
}

// prints all arguments to the serial console
// each argument must be valid for Serial.print()
template<typename... T>
inline void PrintAll(const T&... args)
{
	dg_impl::_PrintHelper(args...);
}

// prints all arguments to the serial console, ending with a newline
// each argument must be valid for Serial.print()
template<typename... T>
inline void PrintlnAll(const T&... args)
{
	dg_impl::_PrintHelper(args...);
	Serial.println();
}



#endif /* DGHELPERS_HPP_ */
