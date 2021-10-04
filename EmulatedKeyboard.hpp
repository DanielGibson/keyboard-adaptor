/*
 * (Multimedia) Keyboard emulated by the Arduino, looks like a real keyboard
 * to the host the Arduino is connected to.
 * It supports both "normal" keyboard keys by using the USB Boot Keyboard protocol
 * (which should ensure max. compatibility) and "multimedia keys" like Play/Pause,
 * start Calculator etc by providing an additional HID Device that uses the
 * "HID Consumer Page (0x07)".
 * Controlling the Caps/Scroll/Num-Lock LEDs is also supported.
 *
 * Uses 16bit (uint16_t) Scancodes (kinda like SDL_Scancode), not ASCII chars!
 *
 * - Scancodes <= 255 correspond to HID Usage IDs of the HID Keyboard/Keypad Page (0x07)
 * - Scancodes >= 256 (MM_SC_OFFSET) are for "Multimedia Keys" like Play/Pause,
 *   start Calculator etc and are based on HID Usage IDs of the HID Consumer Page (0x0C)
 *   !! BUT WITH 256 ADDED TO THEM !!   (to avoid clashes with normal keyboard keys).
 *   So don't forget to add/subtract 256 (MM_SC_OFFSET) when handling multimedia keys.
 * - For reference, up to 231/0xE7 aka Right GUI/Windows key our Scancodes are identical
 *   to SDL_Scancode - but keep in mind that Volume Up/Down and Mute on real keyboards is
 *   usually done via Consumer Page "Audio Control" keys and *not* Scancodes 127-129.
 *   The Multimedia/Consumer Page scancodes do *not* correspond to SDL2 scancodes, as
 *   SDL2 doesn't have a simple mapping to those keys (and only supports a subset).
 * - This makes this keyboard class especially useful for USB-to-USB keyboard adapters
 *   that take input from a real USB keyboard (using a USB Host Shield) and passing
 *   this on to a PC, possibly after remapping some keys to others.
 * - Having unified Scancodes for multimedia keys and normal keys makes it easy to
 *   map one kind of key to the other, see mapKey().
 *
 * Scancodes refer to the key position on the keyboard and *not* their
 * layout-specific keycodes (which are printed on the keys).
 * So e.g. on German QWERTZ-keyboards, the key that prints 'z' has Scancode 0x1C with
 * the usage name "Keyboard y" (the names are based on the US-QWERTY layout).
 *
 * The HID Usage IDs and pages can be found in the official HID Usage Tables
 * document from the USB Implementors Forum (currently hut1_21_0.pdf)
 * which you can download at https://www.usb.org/hid
 *
 *
 * Copyright (C) 2021 Daniel Gibson
 *
 * Based on NicoHood's BootKeyboard_ and Consumer_ code
 * See https://github.com/NicoHood/HID/
 * Copyright (c) 2014-2015 NicoHood
 *
 * License for both my and NicoHood's code in this file:
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */

#ifndef EMULATEDKEYBOARD_HPP_
#define EMULATEDKEYBOARD_HPP_

#include "KeyboardCommon.hpp"

#include "HID.h"

// TODO: define scancodes?


class EmulatedKeyboard : public PluggableUSBModule
{
public:
	enum {
		// regular keys supported and handled with the USB HID Boot Keyboard Protocol
		// See Chapter 10 "Keyboard/Keypad Page (0x07)" in hut1_21_0.pdf ("HID Usage Tables for USB")
		// all the needed constants are in KBCommon::KeyboardConstants (see KeyboardCommon.hpp)
		
		// "Consumer Control" Multimedia keys like Play, Pause, Start Calculator, ...
		// (see Chapter 15 "USB HID Consumer Page (0x0C)")
		// have their own HID endpoint and HID interface
		MAX_MM_KEY = 0x514,  // "Contact Misc." - whatever, it's the last value in that HID Page
		NUM_MM_KEYS = 3,     // I think we won't need more than 3 such keys pressed at the same time
		MM_DEV_REPORTID = 3, // Report ID for the multimedia keys/consumer control device
	};

protected:

	// duplicate some constants to keep the code of this class shorter
	enum {
		MIN_MODIFIER   = KBCommon::MIN_MODIFIER,
		MAX_MODIFIER   = KBCommon::MAX_MODIFIER,
		MAX_NORMAL_KEY = KBCommon::MAX_NORMAL_KEY,
		NUM_BOOT_KEYS  = KBCommon::NUM_BOOT_KEYS,
		MM_SC_OFFSET   = KBCommon::MM_SC_OFFSET,
	};

	// standard USB HID Boot Keyboard-compatible keyreport
	struct BootKeyReport
	{
		uint8_t modifiers; // Ctrl, Shift, Alt etc
		uint8_t reserved;  // always 0
		uint8_t keys[NUM_BOOT_KEYS]; // up to 6 scancodes for keys
	};

	// custom keyreport for multimedia keys (USB HID "consumer page")
	struct ConsumerKeyReport
	{
		// 3 multimedia keys pressed at once should suffice
		// and with this the whole key report (incl. ReportID) will be 7 bytes
		// which should help compatibility (reportedly some devices don't like >8 bytes)
		uint16_t keys[NUM_MM_KEYS];
	};

	BootKeyReport standardKeys = {};
	ConsumerKeyReport mmKeys = {};

	HIDSubDescriptor consumerDeviceDescriptor;

	uint8_t epType[1];
	uint8_t protocol;
	uint8_t idle;

	// keyboard LED bits, see LED_NumLock_Bit etc for values
	uint8_t leds;

	uint8_t* featureReport;
	int featureLength;

	// true if a normal key has been pressed/released
	// and Send() hasn't been called since
	bool haveUnsentNormalKey = false;
	// same for multimedia key
	bool haveUnsentMMKey = false;


	int getInterface(uint8_t* interfaceCount) override;
	int getDescriptor(USBSetup& setup) override;
	bool setup(USBSetup& setup) override;

	void setFeatureReport(void* report, int length)
	{
		if(length > 0)
		{
			featureReport = (uint8_t*)report;
			featureLength = length;

			// Disable feature report by default
			disableFeatureReport();
		}
	}

	int availableFeatureReport(void)
	{
		if(featureLength < 0){
			return featureLength & ~0x8000;
		}
		return 0;
	}

	void enableFeatureReport(void)
	{
		featureLength &= ~0x8000;
	}

	void disableFeatureReport(void)
	{
		featureLength |= 0x8000;
	}

public:

	EmulatedKeyboard();
	virtual ~EmulatedKeyboard(){}

	void Press(uint16_t scancode);
	void Release(uint16_t scancode);
	void ReleaseAll();

	void Send();

	void WakeupHost();

	// Keyboard LED bits, see LED_NumLock_Bit etc for values
	// Example:
	//  if(emuKB.GetLeds() & EmulatedKeyboard::LED_CapsLock_Bit) { print("CapsLock is active"); }
	uint8_t GetLeds() const
	{
		return leds;
	}

	uint8_t GetProtocol() const
	{
		return protocol;
	}
};

#endif /* EMULATEDKEYBOARD_HPP_ */
