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
 * - Scancodes < 255 correspond to HID Usage IDs of the HID Keyboard/Keypad Page (0x07)
 * - Scancodes >= 255 are for "Multimedia Keys" like Play/Pause, start Calculator etc
 *   and are based on HID Usage IDs of the HID Consumer Page (0x0C)
 *   !! BUT WITH 255 ADDED TO THEM !!   (to avoid clashes with normal keyboard keys).
 *   So don't forget to add/subtract 255 to/from multimedia keys when handling them.
 * - For reference, up to 231/0xE7 aka Right GUI/Windows key they're identical to
 *   SDL_Scancode - but keep in mind that Volume Up/Down and Mute on real keyboards is
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

#include "EmulatedKeyboard.hpp"

static const uint8_t _hidReportDescriptorKeyboard[] PROGMEM = {
	//  Keyboard
	0x05, 0x01,                      /* USAGE_PAGE (Generic Desktop)	  47 */
	0x09, 0x06,                      /* USAGE (Keyboard) */
	0xa1, 0x01,                      /* COLLECTION (Application) */
	0x05, 0x07,                      /*   USAGE_PAGE (Keyboard) */

	/* Keyboard Modifiers (shift, alt, ...) */
	0x19, 0xe0,                      /*   USAGE_MINIMUM (Keyboard LeftControl) */
	0x29, 0xe7,                      /*   USAGE_MAXIMUM (Keyboard Right GUI) */
	0x15, 0x00,                      /*   LOGICAL_MINIMUM (0) */
	0x25, 0x01,                      /*   LOGICAL_MAXIMUM (1) */
	0x75, 0x01,                      /*   REPORT_SIZE (1) */
	0x95, 0x08,                      /*   REPORT_COUNT (8) */
	0x81, 0x02,                      /*   INPUT (Data,Var,Abs) */

	// the dummy byte
	0x95, 0x01,                      /*   REPORT_COUNT (1) */
	0x75, 0x08,                      /*   REPORT_SIZE (8) */
	0x81, 0x03,                      /*   INPUT (Cnst,Var,Abs) */

	/* 5 LEDs for num lock etc, 3 left for advanced, custom usage */
	0x05, 0x08,                      /*   USAGE_PAGE (LEDs) */
	0x19, 0x01,                      /*   USAGE_MINIMUM (Num Lock) */
	0x29, 0x08,                      /*   USAGE_MAXIMUM (Kana + 3 custom)*/
	0x95, 0x08,                      /*   REPORT_COUNT (8) */
	0x75, 0x01,                      /*   REPORT_SIZE (1) */
	0x91, 0x02,                      /*   OUTPUT (Data,Var,Abs) */

	/* 6 Keyboard keys */
	0x05, 0x07,                      /*   USAGE_PAGE (Keyboard) */
	0x95, 0x06,                      /*   REPORT_COUNT (6) */
	0x75, 0x08,                      /*   REPORT_SIZE (8) */
	0x15, 0x00,                      /*   LOGICAL_MINIMUM (0) */
	0x26, EmulatedKeyboard::MAX_NORMAL_KEY, 0, /* LOGICAL_MAXIMUM (231, RGUI) */
	0x19, 0x00,                      /*   USAGE_MINIMUM (Reserved (no event indicated)) */
	0x29, EmulatedKeyboard::MAX_NORMAL_KEY, /* USAGE_MAXIMUM (231, RGUI) */
	0x81, 0x00,                      /*   INPUT (Data,Ary,Abs) */

	/* End */
	0xc0                            /* END_COLLECTION */
};

static constexpr uint8_t CC_MAX[2] = {
	EmulatedKeyboard::MAX_MM_KEY & 0xff,
	(EmulatedKeyboard::MAX_MM_KEY >> 8) & 0xff
};

static const uint8_t _hidReportDescriptorConsumerDevice[] PROGMEM = {
	/* Consumer Control (Sound/Media keys) */
	0x05, 0x0C,                              /* usage page (consumer device) */
	0x09, 0x01,                              /* usage -- consumer control */
	0xA1, 0x01,                              /* collection (application) */
	0x85, EmulatedKeyboard::MM_DEV_REPORTID, /* Report ID */
	/* 3 (NUM_MM_KEYS) Media Keys */
	0x15, 0x00,                              /* logical minimum */
	0x26, CC_MAX[0], CC_MAX[1],              /* logical maximum (0x514) */
	0x19, 0x00,                              /* usage minimum (0) */
	0x2A, CC_MAX[0], CC_MAX[1],              /* usage maximum (0x514) */
	0x95, EmulatedKeyboard::NUM_MM_KEYS,     /* report count (3) */
	0x75, 0x10,                              /* report size (16) */
	0x81, 0x00,                              /* input */
	0xC0 /* end collection */
};

EmulatedKeyboard::EmulatedKeyboard()
	: PluggableUSBModule(1, 1, epType),
	  consumerDeviceDescriptor(&_hidReportDescriptorConsumerDevice, sizeof(_hidReportDescriptorConsumerDevice)),
	  protocol(HID_REPORT_PROTOCOL), idle(1), leds(0), featureReport(NULL), featureLength(0)
{
	epType[0] = EP_TYPE_INTERRUPT_IN;
	PluggableUSB().plug(this);
	// first plug this, then call HID() for the first time (so it'll get constructed)
	// => this Boot keyboard will be the first subdevice (after CDC if enabled)
	// => that should help making this USB Boot protocol keyboard even more compatible
	//    (especially if CDC is disabled)
	HID().AppendDescriptor(&consumerDeviceDescriptor);

	// TODO: one could add an additional System Control report descriptor to HID()
	//       to also support system control keys (like System Sleep etc)
	//       but my keyboards don't have such keys so I didn't implement it.
	//       They are defined in the "Generic Desktop Page (0x01)" and have
	//       Usage Page 0x1 and Usage 0x80 (System Control). The Usage IDs seem to be
	//       0x81-0x8F, I think but there's also "System Display Controls" at 0xB1-0xB7
	//       but I'm not sure if those are supposed to go into the System Control "Usage".
	//       Anyway, to map them to scancodes, I'd suggest adding 2048 (or even 2500)
	//       so they're (far) behind the Consumer Control scancodes.
	// TODO: If NKRO support turns out to be needed after all, I'd suggest leaving the
	//       BootKeyboard descriptor part as it is but adding an additional Keyboard descriptor
	//       to HID() with the usual bitmask-based format (29 bytes for 232 = MAX_NORMAL_KEY+1 bits
	//       so there's one bit for every key) and only use those keyboard reports if indeed more
	//       than six non-modifier keys are pressed (otherwise use the old Boot Keyboard report).
}

int EmulatedKeyboard::getInterface(uint8_t *interfaceCount)
{
	*interfaceCount += 1; // uses 1
	HIDDescriptor hidInterface = {
		D_INTERFACE(pluggedInterface, 1, USB_DEVICE_CLASS_HUMAN_INTERFACE, HID_SUBCLASS_BOOT_INTERFACE, HID_PROTOCOL_KEYBOARD),
		D_HIDREPORT(sizeof(_hidReportDescriptorKeyboard)),
		D_ENDPOINT(USB_ENDPOINT_IN(pluggedEndpoint), USB_ENDPOINT_TYPE_INTERRUPT, USB_EP_SIZE, 0x01)
	};
	return USB_SendControl(0, &hidInterface, sizeof(hidInterface));
}

int EmulatedKeyboard::getDescriptor(USBSetup& setup)
{
	// Check if this is a HID Class Descriptor request
	if (setup.bmRequestType != REQUEST_DEVICETOHOST_STANDARD_INTERFACE) { return 0; }
	if (setup.wValueH != HID_REPORT_DESCRIPTOR_TYPE) { return 0; }

	// In a HID Class Descriptor wIndex cointains the interface number
	if (setup.wIndex != pluggedInterface) { return 0; }

	// Reset the protocol on reenumeration. Normally the host should not assume the state of the protocol
	// due to the USB specs, but Windows and Linux just assumes its in report mode.
	protocol = HID_REPORT_PROTOCOL;

	return USB_SendControl(TRANSFER_PGM, _hidReportDescriptorKeyboard, sizeof(_hidReportDescriptorKeyboard));
}

bool EmulatedKeyboard::setup(USBSetup &setup)
{
	if (pluggedInterface != setup.wIndex) {
		return false;
	}

	uint8_t request = setup.bRequest;
	uint8_t requestType = setup.bmRequestType;

	if (requestType == REQUEST_DEVICETOHOST_CLASS_INTERFACE)
	{
		if (request == HID_GET_REPORT) {
			// TODO: HID_GetReport();
			return true;
		}
		if (request == HID_GET_PROTOCOL) {
			// TODO improve
#ifdef __AVR__
			UEDATX = protocol;
#endif
			return true;
		}
		if (request == HID_GET_IDLE) {
			// TODO improve
#ifdef __AVR__
			UEDATX = idle;
#endif
			return true;
		}
	}

	if (requestType == REQUEST_HOSTTODEVICE_CLASS_INTERFACE)
	{
		if (request == HID_SET_PROTOCOL) {
			protocol = setup.wValueL;
			return true;
		}
		if (request == HID_SET_IDLE) {
			idle = setup.wValueL;
			return true;
		}
		if (request == HID_SET_REPORT)
		{
			// Check if data has the correct length afterwards
			int length = setup.wLength;

			// Feature (set feature report)
			if(setup.wValueH == HID_REPORT_TYPE_FEATURE){
				// No need to check for negative featureLength values,
				// except the host tries to send more then 32k bytes.
				// We dont have that much ram anyways.
				if (length == featureLength) {
					USB_RecvControl(featureReport, featureLength);

					// Block until data is read (make length negative)
					disableFeatureReport();
					return true;
				}
				// TODO fake clear data?
			}

			// Output (set led states)
			else if(setup.wValueH == HID_REPORT_TYPE_OUTPUT){
				if(length == sizeof(leds)){
					USB_RecvControl(&leds, length);
					return true;
				}
			}

			// Input (set HID report)
			else if(setup.wValueH == HID_REPORT_TYPE_INPUT)
			{
				if(length == sizeof(standardKeys)){
					USB_RecvControl(&standardKeys, length);
					return true;
				}
			}
		}
	}

	return false;
}

static uint8_t scancodeToModifierFlag(uint16_t scancode)
{
	if(scancode >= EmulatedKeyboard::MIN_MODIFIER && scancode <= EmulatedKeyboard::MAX_MODIFIER)
	{
		return uint8_t(1 << (scancode - EmulatedKeyboard::MIN_MODIFIER));
	}
	return 0;
}

#if 0
static uint16_t mapKey(uint16_t scancode)
{
	// TODO: if you wanna map keys to other keys, do this here (and uncomment the lines in Press() and Release())

	// See Chapter 10 "Keyboard/Keypad Page (0x07)" in hut1_21_0.pdf ("HID Usage Tables for USB")
	// for the meaning of the scancode values ("Usage  ID") of "normal keys" (not multimedia keys)
	if(scancode < 255)
	{
		switch(uint8_t(scancode))
		{
			// example: map capslock to right CTRL
			case 0x39: // capslock
				return 0xE4; // right CTRL
			// NOTE: if you want to return a multimedia key here, you need to return
			//       255 + mm_key_consumer_usage_ID; !
		}
	}
	else // "consumer page" key (multimedia key)
	{
		// See Chapter 15 "USB HID Consumer Page (0x0C)" in hut1_21_0.pdf
		uint16_t consumerUsageID = scancode - 255;
		switch(consumerUsageID)
		{
			// example: map the mute key to play/pause
			case 0xE2: // Mute
				return 255 + 0xCD; // Play/Pause
			// NOTE that the scancodes for consumer page keys are 255 + consumer_usage_ID
			//      (so they don't clash with the normal keyboard keys scancodes)
		}
	}
	return scancode;
}
#endif // 0

void EmulatedKeyboard::Press(uint16_t scancode)
{
	// scancode = mapKey(scancode); // TODO: uncomment if you want to map keys to other keys

	if(scancode >= MIN_MODIFIER && scancode <= MAX_MODIFIER)
	{
		uint8_t flag = scancodeToModifierFlag(scancode);
		if((standardKeys.modifiers & flag) == 0)
		{
			standardKeys.modifiers |= flag;
			haveUnsentNormalKey = true;
		}
	}
	else if(scancode <= MAX_NORMAL_KEY)
	{
		uint8_t insIdx = NUM_BOOT_KEYS+1;
		for(uint8_t i=0; i < NUM_BOOT_KEYS; ++i)
		{
			if(standardKeys.keys[i] == 0)
			{
				insIdx = i;
				break;
			}
			else if(standardKeys.keys[i] == scancode)
			{
				// already pressed
				insIdx = NUM_BOOT_KEYS+1;
				break;
			}
		}
		if(insIdx < NUM_BOOT_KEYS)
		{
			standardKeys.keys[insIdx] = scancode;
			haveUnsentNormalKey = true;
		}
	}
	else if(scancode > 255) // multimedia key
	{
		uint16_t consumerUsageID = scancode - 255;
		uint8_t insIdx = NUM_MM_KEYS+1;
		for(uint8_t i=0; i < NUM_MM_KEYS; ++i)
		{
			if(mmKeys.keys[i] == 0)
			{
				insIdx = i;
				break;
			}
			else if(mmKeys.keys[i] == consumerUsageID)
			{
				// already pressed
				insIdx = NUM_MM_KEYS+1;
				break;
			}
		}
		if(insIdx < NUM_MM_KEYS)
		{
			mmKeys.keys[insIdx] = consumerUsageID;
			haveUnsentMMKey = true;
		}
	}
}

void EmulatedKeyboard::Release(uint16_t scancode)
{
	// scancode = mapKey(scancode); // TODO: uncomment if you want to map keys to other keys

	if(scancode >= MIN_MODIFIER && scancode <= MAX_MODIFIER)
	{
		uint8_t flag = scancodeToModifierFlag(scancode);
		if(standardKeys.modifiers & flag)
		{
			standardKeys.modifiers &= ~flag;
			haveUnsentNormalKey = true;
		}
	}
	else if(scancode < MAX_NORMAL_KEY)
	{
		uint8_t remIdx = NUM_BOOT_KEYS+1;
		for(uint8_t i=0; i < NUM_BOOT_KEYS; ++i)
		{
			if(standardKeys.keys[i] == (uint8_t)scancode)
			{
				remIdx = i;
				break;
			}
		}
		if(remIdx < NUM_BOOT_KEYS)
		{
			// move the following elements over this so zeroes are at the end of the list
			for(uint8_t i = remIdx+1; i < NUM_BOOT_KEYS; ++i)
			{
				standardKeys.keys[i-1] = standardKeys.keys[i];
			}
			standardKeys.keys[NUM_BOOT_KEYS-1] = 0;
			haveUnsentNormalKey = true;
		}
	}
	else if(scancode > 255) // multimedia key (consumer page)
	{
		uint16_t consumerUsageID = scancode - 255;
		uint8_t remIdx = NUM_MM_KEYS+1;
		for(uint8_t i=0; i < NUM_MM_KEYS; ++i)
		{
			if(mmKeys.keys[i] == consumerUsageID)
			{
				remIdx = i;
				break;
			}
		}
		if(remIdx < NUM_MM_KEYS)
		{
			// move the following elements over this so zeroes are at the end of the list
			for(uint8_t i = remIdx+1; i < NUM_MM_KEYS; ++i)
			{
				mmKeys.keys[i-1] = mmKeys.keys[i];
			}
			mmKeys.keys[NUM_MM_KEYS-1] = 0;
			haveUnsentMMKey = true;
		}
	}
}

void EmulatedKeyboard::ReleaseAll()
{
	if(standardKeys.modifiers != 0)
	{
		standardKeys.modifiers = 0;
		haveUnsentNormalKey = true;
	}
	for(uint8_t i=0; i < NUM_BOOT_KEYS; ++i)
	{
		if(standardKeys.keys[i] != 0)
		{
			standardKeys.keys[i] = 0;
			haveUnsentNormalKey = true;
		}
	}
	for(uint8_t i=0; i < NUM_MM_KEYS; ++i)
	{
		if(mmKeys.keys[i] != 0)
		{
			mmKeys.keys[i] = 0;
			haveUnsentMMKey = true;
		}
	}
}

void EmulatedKeyboard::Send()
{
	// only send if anything has changed since lasts Send()
	if(haveUnsentNormalKey)
	{
		// kinda like HID_::SendReport() but without the Report ID, as USB Boot Keyboards don't have one
		int bootKBSendRet = USB_Send(pluggedEndpoint | TRANSFER_RELEASE, &standardKeys, sizeof(standardKeys));
		if(bootKBSendRet < 0)
		{
			// TODO: error ?
		}
		haveUnsentNormalKey = false;
	}

	if(haveUnsentMMKey)
	{
		// send multimedia keys (via normal HID device that uses a Report ID)
		int mmKeysSendRet = HID().SendReport(MM_DEV_REPORTID, &mmKeys, sizeof(mmKeys));
		if(mmKeysSendRet < 0)
		{
			// TODO: error ?
		}
		haveUnsentMMKey = false;
	}
}

void EmulatedKeyboard::WakeupHost()
{
#ifdef __AVR__
	USBDevice.wakeupHost();
#endif
}
