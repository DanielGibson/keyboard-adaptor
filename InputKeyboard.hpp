/*
 * An almost generic Keyboard input driver to be used with the
 * USB Host Shield Library 2.0 to get input from HID keyboards.
 * Supports both "normal" keys via USB HID Boot keyboard Protocol
 * and "multimedia" keys (HID Consumer Page) via custom
 * keyboard-specific HID reports THAT NEED ADJUSTMENTS PER DEVICE.
 *
 * Look at DetectDevice(), ParseHIDData() and Handle*MultimediaKeyReport()
 * for how to add support for new keyboards; also, with debug output enabled
 * ParseHIDData() logs the HID messages so you can see what kind of messages
 * which endpoint gets for specific (kinds of) keys. Together with the
 * device descriptor reports of the keyboard (that you can get on your PC),
 * supporting additional keyboards should be pretty simple.
 * Chances are good they use the same format for multimedia keys as keyboards
 * already supported and you just need to add a case in DetectDevice() with
 * the device's USB Vendor/Product ID and the endpoint and report IDs.
 *
 *
 * Copyright (C) 2021 Daniel Gibson
 *
 * Licensed under GPLv2, like the USB Host Shield Library used/extended by this class.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or (at
 *   your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *   See the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *   02111-1307, USA.
 */

#ifndef INPUTKEYBOARD_HPP_
#define INPUTKEYBOARD_HPP_

#include "KeyboardCommon.hpp"

#include <hidcomposite.h>
// TODO: get rid of dependency to EmulatedKeyboard (duplicate some constants, put others in common header)
#include "EmulatedKeyboard.hpp"

class InputKeyboard : public HIDComposite
{
protected:
	using EmuKB = EmulatedKeyboard;

	uint8_t curLedState = 0;

	uint8_t oldModifierState = 0;
	uint8_t oldNormalKeysState[EmuKB::NUM_BOOT_KEYS] = {};
	uint16_t oldMMKeysState[EmuKB::NUM_MM_KEYS] = {}; // as consumer page usage ID, *not* EmuKB scancode!

	// the Ducky DK2108 uses 3bytes (24bits) for the state of some hardcoded multimedia keys
	// so for that keyboard, oldDK2108MMkeyState is used instead of oldMMKeysState
	uint8_t oldDK2108MMkeyState[3] = {0, 0, 0};

	uint8_t bootKbdEP = 0; // endpoint ID (0 = no boot keyboard device)
	uint8_t bootKbdReportID = 0; // 0 = no report ID

	// for "multimedia key" consumer control reports
	uint8_t mmKeysEP = 0; // multimedia key consumer control endpoint ID (0 = no such endpoint)
	uint8_t mmKeysReportID = 0;
	uint8_t mmKeysNumKeys = 1;

	// what format the consumer control reports have
	enum : uint8_t {
		kCCreportNormal = 0, // one or more 16bit values
		kCCreportDK2108 = 1, // Like my Ducky 2108, bitmask with 24bits, each for one common multimedia key
	} mmKeyReportStyle = kCCreportNormal;

	// gets the vendor and product IDs from the connected keyboard
	void GetUSBID(uint16_t& out_vid, uint16_t& out_pid);

	void DetectDevice();

	void Reset()
	{
		curLedState = 0;
		oldModifierState = 0;
		memset(oldNormalKeysState, 0, sizeof(oldNormalKeysState));
		memset(oldMMKeysState, 0, sizeof(oldMMKeysState));
		memset(oldDK2108MMkeyState, 0, sizeof(oldDK2108MMkeyState));
		SetLEDs(0, true);
	}

	// Return true for the interface we want to hook into
	bool SelectInterface(uint8_t iface, uint8_t proto) override;

	// data has 8 bytes: the first one contains the modifier state (one bit per left/right shift,alt,ctrl,gui/win)
	// the second byte contains garbage/nothing, the following six bytes contain keyboard scancodes
	// NOTE: this expects that any Report ID is already skipped!
	void HandleBootKeyboardReport(const uint8_t* data, int len);

	// expects data[] to contain byte-pairs with 16bit values of consumer control usage IDs
	// NOTE: this expects that any Report ID is already skipped!
	void HandleNormalMultimediaKeyReport(const uint8_t* data, int len);

	// expects data[] to contain 3bytes (24bits) for some assorted multimedia keys
	// NOTE: this expects that any Report ID is already skipped!
	void HandleDK2108MultimediaKeyReport(const uint8_t* data, int len);

	// Called by the HIDComposite library
	// Will be called for all HID data received from the USB interface
	// passes it on to Handle*Report()
	void ParseHIDData(USBHID *hid, uint8_t ep, bool has_rpt_id, uint8_t len, uint8_t *buf) override;

	//
	// I think the following methods are most relevant for users of this class
	// (=> they should derive their own class from this and override them as needed)
	//

	// called when a key is pressed, with its Scancode
	// override to do something useful when a key is pressed
	virtual void OnKeyPress(uint16_t scancode);

	// called when a formerly pressed key is released, with its Scancode
	// override to do something useful when a key is released
	virtual void OnKeyRelease(uint16_t scancode);

	// called when a keyboard has been plugged in and initialization
	// on USB Host Lib's side is done
	// ! DON'T FORGET TO CALL InputKeyboard::OnInitSuccessful() in your overriding method !
	uint8_t OnInitSuccessful() override;

	// this is called when the keyboard is disconnected (plugged out)
	// and also when it's plugged (back) in before it's initialized
	// (before SelectInterface() and OnInitSuccessful())
	// Override this to do things in these situations (e.g. release all keys of emulated keyboard)
	// ! DON'T FORGET TO CALL InputKeyboard::Release() in your overriding method !
	uint8_t Release() override;


public:
	InputKeyboard(USB *p) : HIDComposite(p) {}
	virtual ~InputKeyboard(){}


	// returns true if the key for the given Scancode is currently registered as pressed,
	// i.e. OnKeyPress() has been called for it but OnKeyRelease() hasn't yet been called yet
	// might be useful if you want to detect key combinations
	bool IsKeyPressed(uint16_t scancode) const;

	// Sets the keyboards LEDs (for capslock, numlock etc)
	// ledState is a bitmask, see EmulatedKeyboard::LED_*_Bit for values
	void SetLEDs(uint8_t ledState, bool force = false)
	{
		if(ledState != curLedState || force)
		{
			SetReport(0, 0, 2, 0, 1, &ledState);
			curLedState = ledState;
		}
	}

	// Put the keyboard in suspended state so it (hopefully) saves energy
	// and turns off backlights and such.
	// For that to work you also need to stop calling USB::Task() while you want
	// the device to remain suspended.
	// See also USBDevice.isSuspended() from Arduino core
	//
	// Note that waking the host (if it's also suspended/in standby) with key presses
	// doesn't work because when the Keyboard (or USB host shield) is suspended
	// there's no way (as far as I know) to detect them.
	// USB should support that generally (and I think theoretically the MAX3421E
	// should set the bmRWUIRQ aka RSMREQIRQ IRQ), but I couldn't get it to work.
	void Suspend()
	{
		// disabling SOF (Start-of-Frame) marker generation will cause the connected
		// keyboard to suspend after a few milliseconds without traffic
		// (for that you also need to stop calling UsbHost.Task())
		uint8_t mode = pUsb->regRd(rMODE) & ~bmSOFKAENAB;
		pUsb->regWr(rMODE, mode);
	}

	// Ends the suspended state so the keyboard works again
	// (Also remember to call USB::Task() again)
	void Resume()
	{
		// re-enable SOF so the keyboard wakes up again
		uint8_t mode = pUsb->regRd(rMODE) | bmSOFKAENAB;
		pUsb->regWr(rMODE, mode);
	}
};



#endif /* INPUTKEYBOARD_HPP_ */
