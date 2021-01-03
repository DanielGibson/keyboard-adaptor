/*
 * Keyboard emulated by the Arduino, looks like a real keyboard to the host connected to Arduinos USB port
 * Uses USB Scancodes (kinda like SDL_Scancode), not ASCII chars!
 *
 * (C) 2021 Daniel Gibson
 *
 * Based on the Arduino Keyboard_ class (Keyboard.h)
 *
 * Copyright (c) 2015, Arduino LLC
 * Original code (pre-library): Copyright (c) 2011, Peter Barrett
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef EMULATEDKEYBOARD_HPP_
#define EMULATEDKEYBOARD_HPP_

#include "keyboardwrapper.h"

#include "HID.h"

// TODO: define scancodes?

class EmulatedKeyboard
{
public:
	enum {
		// regular keys supported and handled with the USB HID Boot Keyboard Protocol
		// See Chapter 10 "Keyboard/Keypad Page (0x07)" in hut1_12_v2.pdf ("USB - HID Usage Tables")
		NUM_BOOT_KEYS = 6,    // 6 keys at once
		MAX_NORMAL_KEY = 0xE7, // RGUI (max scancode handled via Boot Keyboard protocol)
		// Modifier keys (first byte in Boot Keyboard Protocol), also Keyboard/Keypad Page
		MIN_MODIFIER = 224,   // Left CTRL (0xE0)
		MAX_MODIFIER = 231,   // Right GUI (0xE7)

		// Multimedia keys like Play, Pause, Start Calculator, ...
		// See Chapter 15 "USB HID Consumer Page (0x0C)"
		NUM_MM_KEYS = 3, // I think we won't need more than 3 such keys pressed at the same time
		MAX_MM_KEY = 0x29C // "AC Distribute Vertically" - whatever, it's the last value in that HID Page
	};

	// standard USB HID Boot Keyboard-compatible keyreport
	typedef struct
	{
		uint8_t modifiers; // Ctrl, Shift, Alt etc
		uint8_t reserved;  // always 0
		uint8_t keys[NUM_BOOT_KEYS]; // up to 6 scancodes for keys
	} BootKeyReport;

	// custom keyreport for multimedia keys (USB HID "consumer page")
	typedef struct
	{
		uint8_t reportID;
		uint8_t padding; // no idea if this makes sense, but this way we get 8 bytes all in all
		uint16_t keys[NUM_MM_KEYS]; // three multimedia keys pressed at once should suffice..
	} ConsumerKeyReport;

private:

	HIDSubDescriptor hidNode;

	BootKeyReport standardKeys = {};
	ConsumerKeyReport mmKeys = {};

	// TODO: LEDs

public:

	EmulatedKeyboard();

	void Press(uint16_t scancode);
	void Release(uint16_t scancode);
	void ReleaseAll();

	void Send();

};



#endif /* EMULATEDKEYBOARD_HPP_ */
