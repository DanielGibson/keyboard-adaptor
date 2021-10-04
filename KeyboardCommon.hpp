/*
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

#ifndef _DGKeyboardCommon_H_
#define _DGKeyboardCommon_H_
#include "Arduino.h"

#ifndef CDC_DISABLED
// uncomment to log some debug info to the Serial console
//#define KBDWRAP_ENABLE_DEBUG
#endif

#include "DGHelpers.hpp"

// this struct only exists to give the constants a scope, like KBCommon::NUM_BOOT_KEYS
struct KBCommon {
	enum KeyboardConstants {
		// regular keys supported and handled with the USB HID Boot Keyboard Protocol
		// See Chapter 10 "Keyboard/Keypad Page (0x07)" in hut1_21_0.pdf ("HID Usage Tables for USB")
		NUM_BOOT_KEYS = 6,    // 6 keys at once
		MAX_NORMAL_KEY = 0xE7, // RGUI (max scancode handled via Boot Keyboard protocol) - TODO: move into EmulatedKeyboard?
		// Modifier keys (first byte in Boot Keyboard Protocol), also Keyboard/Keypad Page
		MIN_MODIFIER = 224,   // Left CTRL (0xE0)
		MAX_MODIFIER = 231,   // Right GUI (0xE7)
		NUM_MODIFIERS = MAX_MODIFIER - MIN_MODIFIER + 1,
		// Multimedia keys like Play, Pause, Start Calculator, ...
		// See Chapter 15 "USB HID Consumer Page (0x0C)"
		MM_SC_OFFSET = 256,  // Added to multimedia consumer control Usage IDs to get our Scancode

		// Values for LEDs Bitmask (it can hold 8 bits, even though the higher ones are a bit obscure)
		// (Based on "LED Page (0x08)" in the HID Usage Tables document,
		//  but keep in mind that this is a bitmask, so it's 1 << (hid_usage-1))
		LED_NumLock_Bit      = 1 << 0, //   1 0x01
		LED_CapsLock_Bit     = 1 << 1, //   2 0x02
		LED_ScrollLock_Bit   = 1 << 2, //   4 0x04
		LED_Compose_Bit      = 1 << 3, //   8 0x08
		LED_Kana_Bit         = 1 << 4, //  16 0x10
		LED_Power_Bit        = 1 << 5, //  32 0x20
		LED_Shift_Bit        = 1 << 6, //  64 0x40
		LED_DoNotDisturb_Bit = 1 << 7, // 128 0x80
	};
};

#endif /* _DGKeyboardCommon_H_ */
