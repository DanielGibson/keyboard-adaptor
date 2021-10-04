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

#include "InputKeyboard.hpp"

// gets the vendor and product IDs from the connected keyboard
void InputKeyboard::GetUSBID(uint16_t& out_vid, uint16_t& out_pid)
{
	USB_DEVICE_DESCRIPTOR udd = {};

	uint8_t rcode = pUsb->getDevDescr(bAddress, 0, sizeof(udd), (uint8_t*)&udd);
	if(rcode)
	{
		PrintlnAll(F("Failed to get USB device descriptor"));
		out_vid = VID; // better than nothing..
		out_pid = PID;
	}
	else
	{
		out_vid = udd.idVendor;
		out_pid = udd.idProduct;
	}
}

void InputKeyboard::DetectDevice()
{
	// hopefully sane defaults:
	bootKbdEP = 1;
	bootKbdReportID = 0;
	mmKeysEP = 0; // TODO: try to find a consumer control endpoint/report?
	mmKeysReportID = 0;
	// most keyboards seem to use the "normal" multimedia/consumer-control key
	// format (16bit int keycodes), with exactly one key per report,
	// i.e. only one multimedia key can be pressed at the same time
	mmKeysNumKeys = 1;
	mmKeyReportStyle = kCCreportNormal;

	const __FlashStringHelper* devName = nullptr;

	// for some reason, with the QPad MK-75 this->PID and this->VID are wrong
	// but fetching them again gives the correct results.. so fetch them again.
	uint16_t vid, pid;
	GetUSBID(vid, pid);

// creates a combined ID of idVendor and idProduct of the connected keyboard
// (having a combined ID is useful for switch/case)
#define CMB_USB_ID(VENDOR_ID, PRODUCT_ID) \
		(uint32_t(VENDOR_ID & 0xFFFF) << 16 | (PRODUCT_ID & 0xFFFF))

	switch(CMB_USB_ID(vid, pid))
	{
		// Holtek - Gaming keyboard, like KM-Gaming K-GK2 (and probably other keyboards with same chip)
		case CMB_USB_ID(0x04d9, 0xa1cd):
			devName = F("Holtek/KM-Gaming K-GK2");
			// boot keyboard is 1, 0 (EndPoint 1, Report ID 0)
			mmKeysEP = 2;
			mmKeysReportID = 2;
			// the remaining settings can use the default value

			// Just as additional information (this project doesn't support NKRO anyway):
			// This keyboard has a kinda clever NKRO format: the normal boot-keyboard-style
			// reports have 13 additional bytes (after the six bytes for normal keycodes)
			// which are used as a bitmask for 104 keys: 0 to incl. 103 (Keypad Equal)
			// and are only set if six keys (+modifiers) aren't enough.
			// I /think/ that this does not violate the USB Boot Keyboard protocol, so
			// compliant devices should be able to work with those reports just fine
			// (unless > 6 keys are pressed).
			// Problem is, devices that only implement the USB Boot Keyboard protocol
			// usually aren't *that* compliant and many will be confused...
			// Luckily you can work around such issues with an Arduino + USB Host Shield
			// and all the beautiful (*coughs*) code of this project.
			break;

		// TG3 Electronics (Ducky) - DK2108
		case CMB_USB_ID(0x0f39, 0x1083):
			devName = F("Ducky DK2108");
			// boot keyboard is 1, 0
			mmKeysEP = 2;
			mmKeysReportID = 3;
			mmKeysNumKeys = 24; // 24 bits
			mmKeyReportStyle = kCCreportDK2108;

			// TODO: detect ducky NKRO (which is switchable with Fn+F12 and reconnects keyboard)?
			// There doesn't seem to be a proper way (except for parsing descriptor reports),
			// because (IMHO wrongly) both have bInterfaceSubClass 1 "Boot Interface Subclass"
			// and bInterfaceProtocol 1 (Keyboard), even though in NKRO mode it's
			// *not* a boot procotol keyboard.. however, in NKRO mode, endpoints
			// wMaxPacketSize is 1x 14 instead of 1x 8 and device descriptors and
			// wDescriptorLength is 53 instead of 63 so that *could* be used.
			// But as my EmulatedKeyboard doesn't support NKRO and as "6KRO" is the
			// DK2108's default mode, supporting this is pointless (at least right now)
			break;

		// Darfon Electronics Corps - "QPAD Wired Keyboard" (tested with MK-75)
		case CMB_USB_ID(0x0d62, 0xd99e):
			devName = F("QPad Wired Keyboard (like MK-75)");
			// boot keyboard is 1, 0
			mmKeysEP = 2;
			mmKeysReportID = 6;
			// the remaining settings can use the default value

			// Here NKRO uses EP 2 Report ID 5 - but only if more than 6 keys are pressed
			// otherwise the boot keyboard device is used (which just works).
			// There seem to be 31 bytes all in all: Report ID, 1 byte of constant 0, then
			// 29 bytes (232bits) representing keys 0 up to 231 (GUI Right), though it seems
			// like the modifiers are always communicated with boot keyboard reports.
			// Example data (incl. Report ID!) when pressing 6 keys and then 'A':
			// 05 00 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
			//       ^- that's the bit for 'A' (bit 5 for HID Usage ID 4, as bit 0 aka 1 is used for usage ID 0)
			// (+ additional, independent, keyboard boot protocol report for the other 6 keys that's left out here)
			break;
		
		// SONiX (SPC Gear) GK630 Gaming Keyboard
		case CMB_USB_ID(0x3299, 0x4e61):
			devName = F("SPC Gear/SONiX GK630");
			// boot keyboard is 1, 0
			mmKeysEP = 2;
			mmKeysReportID = 3;
			
			// For NKRO it uses EP 2 Report ID 1 - but only if more than 6 (non-modifier) keys
			// are pressed, otherwise the boot keyboard device is used (which just works).
			// There seem to be 16 bytes all in all: The Report ID and then 15 bytes (120 bits)
			// representing keys A (0x04) to F21 (0x70 == 112) - yes, that should only be 108 bits, no idea.
			// Example data (incl. Report ID!) when pressing 6 keys and then 'A':
			// 01 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00
			//     ^- that's the bit for 'A' (bit 1 for HID Usage ID 4, which is the "Usage Minimum" for that report)
			// ^- Report ID
			// (+ additional, independent, keyboard boot protocol report for the other 6 keys that's left out here)
			break;
		
		// Cherry MX 10.0N
		case CMB_USB_ID(0x046a, 0x00df):
			devName = F("Cherry MX 10.0N");
			// interestingly, except for the names and vendor/product IDs, from a USB HID point of view
			// this is identical to the GK630 - physically it's different (not TKL, different Fn-Key combinations)
			// maybe Cherry also uses some SONiX chip?
			mmKeysEP = 2;
			mmKeysReportID = 3;
			// NKRO also is handled in the same way as the GK630 does
			break;
	}

	if(devName != nullptr)
		PrintlnAll(F("Detected "), devName, F(" USB ID 0x"), AsHex(vid), F(" : 0x"), AsHex(pid));
	else
		PrintlnAll(F("Unknown Keyboard (treating as Generic Boot Keyboard w/o Multimedia keys). USB ID 0x"),
		           AsHex(vid), F(" : 0x"), AsHex(pid));

#undef CMB_USB_ID
}

// Return true for the interface we want to hook into
bool InputKeyboard::SelectInterface(uint8_t iface, uint8_t proto)
{
	(void)iface;
#ifdef KBDWRAP_ENABLE_DEBUG
	PrintlnAll(F("SelectInterface iface: "), iface, F(" proto "), proto);
#endif

	// from bInterfaceProtocol: 1 is Keyboard, 0 is unspecified, 2 is Mouse
	// both are ok (this method is only called if bInterfaceClass is 3, HID)
	// the GK630 uses the Mouse endpoint for Consumer Control
	if (proto == 0 || proto == 1 || proto == 2)
		return true;

	return false;
}

// data has 8 bytes: the first one contains the modifier state (one bit per left/right shift,alt,ctrl,gui/win)
// the second byte contains garbage/nothing, the following six bytes contain keyboard scancodes
// NOTE: this expects that any Report ID is already skipped!
void InputKeyboard::HandleBootKeyboardReport(const uint8_t* data, int len)
{
#ifdef KBDWRAP_ENABLE_DEBUG
	PrintAll(F("HandleBootKeyboardReport: "), data[0]);
	for(uint8_t i=1; i<8; ++i)
		PrintAll(F(" "), data[i]);
	PrintlnAll();
#endif

	// handle the modifier keys
	uint8_t newModifierState = data[0];
	uint8_t diffBits = newModifierState ^ oldModifierState;
	if(diffBits != 0)
	{
		for(uint8_t bitIdx=0; bitIdx < KBCommon::NUM_MODIFIERS; ++bitIdx)
		{
			uint8_t bit = 1 << bitIdx;
			if(diffBits & bit)
			{
				uint16_t scancode = KBCommon::MIN_MODIFIER + bitIdx;
				bool pressed = (newModifierState & bit) != 0;
				pressed ? OnKeyPress(scancode) : OnKeyRelease(scancode);
			}
		}
		oldModifierState = newModifierState;
	}

	// handle the normal keys

	// from this point on, only byte 3 and above are of interest
	len = min(len-2, KBCommon::NUM_BOOT_KEYS);
	const uint8_t* newKeys = &data[2];

	for(uint8_t i=0; i < len; ++i)
	{
		uint8_t sc = newKeys[i];
		if(sc != 0 && FindInArray(sc, oldNormalKeysState, KBCommon::NUM_BOOT_KEYS) == -1)
		{
			OnKeyPress(sc); // if this key wasn't already pressed, press it now
		}
	}

	for(uint8_t i=0; i < KBCommon::NUM_BOOT_KEYS; ++i)
	{
		uint8_t oldSc = oldNormalKeysState[i];
		if(oldSc != 0 && FindInArray(oldSc, newKeys, len) == -1)
		{
			OnKeyRelease(oldSc); // if this key isn't pressed anymore, release it
		}
		oldNormalKeysState[i] = (i < len) ? newKeys[i] : 0;
	}
}

// expects data[] to contain byte-pairs with 16bit values of consumer control usage IDs
// NOTE: this expects that any Report ID is already skipped!
void InputKeyboard::HandleNormalMultimediaKeyReport(const uint8_t* data, int len)
{
	uint8_t numNewKeys = min(len/2, NUM_MM_KEYS);
	uint16_t newKeys[NUM_MM_KEYS];
	for(uint8_t i=0; i < numNewKeys; ++i)
	{
		uint16_t usageID = data[2*i] + (uint16_t(data[2*i+1]) << 8);
		newKeys[i] = usageID;
		if(usageID != 0 && FindInArray(usageID, oldMMKeysState, NUM_MM_KEYS) == -1)
		{
			OnKeyPress(usageID + KBCommon::MM_SC_OFFSET); // remember: scancode is consumer key hid usageID + KBCommon::MM_SC_OFFSET
		}
	}

	for(uint8_t i=0; i < NUM_MM_KEYS; ++i)
	{
		uint16_t oldUsageID = oldMMKeysState[i];
		if(oldUsageID != 0 && FindInArray(oldUsageID, newKeys, numNewKeys) == -1)
		{
			OnKeyRelease(oldUsageID + KBCommon::MM_SC_OFFSET);
		}
		oldMMKeysState[i] = (i<numNewKeys) ? newKeys[i] : 0;
	}
}

// the HID consumer control Usage IDs associated with the bits of each input byte
// in Ducky DK2108 multimedia key reports
static constexpr uint16_t DK2108BytesUsageIDs[3][8] = {
	{ 0x0b5, 0x0b6, 0x0b7, 0x0cd, 0x0e2, 0x0e5, 0x0e7, 0x0e9 },
	{ 0x0ea, 0x152, 0x153, 0x154, 0x155, 0x183, 0x18a, 0x192 },
	{ 0x194, 0x221, 0x223, 0x224, 0x225, 0x226, 0x227, 0x22a }
};

// expects data[] to contain 3bytes (24bits) for some assorted multimedia keys
// NOTE: this expects that any Report ID is already skipped!
void InputKeyboard::HandleDK2108MultimediaKeyReport(const uint8_t* data, int len)
{
	if(len < 3)
		return;

	for(uint8_t byteIdx=0; byteIdx < 3; ++byteIdx)
	{
		uint8_t newByte = data[byteIdx];
		uint8_t diffBits = newByte ^ oldDK2108MMkeyState[byteIdx];
		if(diffBits != 0)
		{
			for(uint8_t i=0; i < 8; ++i)
			{
				uint8_t bit = 1 << i;
				if(diffBits & bit)
				{
					uint16_t scancode = DK2108BytesUsageIDs[byteIdx][i] + KBCommon::MM_SC_OFFSET;
					bool pressed = (newByte & bit) != 0;
					pressed ? OnKeyPress(scancode) : OnKeyRelease(scancode);
				}
			}
			oldDK2108MMkeyState[byteIdx] = newByte;
		}
	}
}

// Called by the HIDComposite library
// Will be called for all HID data received from the USB interface
void InputKeyboard::ParseHIDData(USBHID *hid, uint8_t ep, bool has_rpt_id, uint8_t len, uint8_t *buf)
{
	(void)hid;
	(void)has_rpt_id; // doesn't work anyway (never set, would have to be per interface or endpoint but is HIDComposite class field)
	if (len > 0 && buf)
	{
#ifdef KBDWRAP_ENABLE_DEBUG
		PrintAll(F("\r\n ep: "), ep, F(" has_rpt_id: "), (uint8_t)has_rpt_id, F(" data: "));
		for (uint8_t i = 0; i < len; i++) {
			PrintAll(AsHex(buf[i]), F(" "));
		}
		PrintlnAll();
#endif

		if( ep == bootKbdEP && (bootKbdReportID == 0 || bootKbdReportID == buf[0]) )
		{
			if(bootKbdReportID != 0)
			{
				++buf; // skip the Report ID before passing the data to HandleBootKeyboardReport()
				--len; // adjust length accordingly
			}
			HandleBootKeyboardReport(buf, len);
		}
		else if(ep == mmKeysEP && (mmKeysReportID == 0 || mmKeysReportID == buf[0]))
		{
			if(mmKeysReportID != 0)
			{
				// again, skip the report ID before further processing
				++buf;
				--len;
			}

			if(mmKeyReportStyle == kCCreportNormal && len >= 2)
			{
				if(mmKeysNumKeys == 1) // simple code for simple case
				{
					// one multimedia key (16 bit consumer control usage ID in two bytes)
					uint16_t usageID = buf[0] + (uint16_t(buf[1]) << 8);
					uint16_t oldUsageID = oldMMKeysState[0];
					if(usageID != oldUsageID)
					{
						if(oldUsageID != 0)  OnKeyRelease(oldUsageID + KBCommon::MM_SC_OFFSET);
						// remember: scancode is consumer key hid usage + KBCommon::MM_SC_OFFSET
						if(usageID != 0)     OnKeyPress(usageID + KBCommon::MM_SC_OFFSET);
						oldMMKeysState[0] = usageID;
					}
				}
				else
				{
					HandleNormalMultimediaKeyReport(buf, len);
				}
			}
			else if(mmKeyReportStyle == kCCreportDK2108 && len == 3)
			{
				HandleDK2108MultimediaKeyReport(buf, len);
			}
		}
	}
}


// called when a key is pressed, with its Scancode
// override to do something useful when a key is pressed
void InputKeyboard::OnKeyPress(uint16_t scancode)
{
#ifdef KBDWRAP_ENABLE_DEBUG
	PrintlnAll(F("InputKeyboard::OnKeyPress( "), AsHex(scancode), F(" )") );
#else
	(void)scancode; // shut up about unused parameter
#endif
}

// called when a formerly pressed key is released, with its Scancode
// override to do something useful when a key is released
void InputKeyboard::OnKeyRelease(uint16_t scancode)
{
#ifdef KBDWRAP_ENABLE_DEBUG
	PrintlnAll(F("InputKeyboard::OnKeyRelease( "), AsHex(scancode), F(" )") );
#else
	(void)scancode; // shut up about unused parameter
#endif
}

// called when a keyboard has been plugged in and initialization
// on USB Host Lib's side is done
// ! DON'T FORGET TO CALL InputKeyboard::OnInitSuccessful() in your overriding method !
uint8_t InputKeyboard::OnInitSuccessful()
{
#ifdef KBDWRAP_ENABLE_DEBUG
	PrintlnAll(F("OnInitSuccessful()"));
#endif
	DetectDevice();
	Reset();
	return 0;
}

// this is called when the keyboard is disconnected (plugged out)
// and also when it's plugged (back) in before it's initialized
// (before SelectInterface() and OnInitSuccessful())
// Override this to do things in these situations (e.g. release all keys of emulated keyboard)
// ! DON'T FORGET TO CALL InputKeyboard::Release() in your overriding method !
uint8_t InputKeyboard::Release()
{
	Reset();
	return HIDComposite::Release();
}

// returns true if the key for the given Scancode is currently registered as pressed,
// i.e. OnKeyPress() has been called for it but OnKeyRelease() hasn't yet been called yet
// might be useful if you want to detect key combinations
bool InputKeyboard::IsKeyPressed(uint16_t scancode) const
{
	if(scancode != 0)
	{
		if(scancode >= KBCommon::MIN_MODIFIER && scancode <= KBCommon::MAX_MODIFIER )
		{
			uint8_t bit = uint8_t(1 << (scancode - KBCommon::MIN_MODIFIER));
			return (oldModifierState & bit) != 0;
		}
		else if(scancode < KBCommon::MM_SC_OFFSET) // normal key
		{
			return FindInArray(uint8_t(scancode), oldNormalKeysState, KBCommon::NUM_BOOT_KEYS) != -1;
		}
		else if(mmKeysEP != 0) // multimedia key (and the keyboard has multimedia keys we know of)
		{
			uint16_t usageID = scancode - KBCommon::MM_SC_OFFSET;
			if(mmKeyReportStyle == kCCreportNormal)
			{
				return FindInArray(usageID, oldMMKeysState, NUM_MM_KEYS) != -1;
			}
			else if(mmKeyReportStyle == kCCreportDK2108)
			{
				for(uint8_t byteIdx=0; byteIdx < 3; ++byteIdx)
				{
					for(uint8_t i=0; i < 8; ++i)
					{
						if(DK2108BytesUsageIDs[byteIdx][i] == usageID)
						{
							uint8_t bit = uint8_t(1 << i);
							return (oldDK2108MMkeyState[byteIdx] & bit) != 0;
						}
					}
				}
				return false; // not found => scancode isn't supported by DK2108
			}
		}
	}
	return false;
}
