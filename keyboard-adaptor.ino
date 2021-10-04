/*
 * A USB-to-USB keyboard adaptor for Arduino (-likes) with USB support
 * (like the Leonardo or Pro Micro) and USB Host support using a USB host shield
 * compatible with the USB Host Shield Library 2.0 library.
 * It uses the InputKeyboard class for input from a real keyboard via USB host shield
 * and the EmulatedKeyboard class to make the Arduino pretend to be a USB-keyboard
 * to the device (e.g. PC or KVM switch) it's connected to.
 * By un-commenting and adapting the mapKey() function you can map keys to other keys,
 * for example if you want the caps-lock key to behave like the right CTRL key.
 *
 * Note that you might have to also adjust InputKeyboard.cpp to support your keyboard,
 * esp. for multimedia/"consumer control" keys - see the comment at top of that file.
 *
 * ### Notes on compatibility with "difficult" devices ###
 *
 * A main goal of this adaptor is to improve compatibility with finicky devices that
 * don't support "fancy" multimedia keyboards too well, like many KVM switches.
 * For that it might be necessary to deactivate the "CDC" (USB serial console) device
 * of your Arduino. https://github.com/arduino/ArduinoCore-avr/pull/383 shows
 * a way to do this; however, even though this change has been merged it hasn't
 * been part of a release yet so you might have to apply that patch manually.
 *
 * Unfortunately, even with that patch, disabling isn't as easy as it should be,
 * because the Arduino IDE doesn't support setting compiler flags (it'd be -DCDC_DISABLED)
 * So if you're building with the Arduino IDE, you need to edit USBDesc.h
 * in your-arduino-install-dir/hardware/arduino/avr/cores/arduino/
 * and remove the "//" before "#define CDC_DISABLED".
 * NOTE THAT THIS WILL DISABLE THE SERIAL CONSOLE FOR ALL PROJECTS YOU BUILD!
 *
 * Also note that a disabled CDC makes flashing the Arduino a bit harder.
 * Once it's disabled to flash you need to first start the flashing process
 * and then quickly reset the Arduino (by grounding the Reset pin) so it gets
 * into bootloader mode (which allows flashing) for a few seconds.
 * The Pro Micro must be reset twice in a row (quickly) to enter that mode.
 *
 * ### Copyright ###
 *
 * The files in this project are under different licenses:
 * - InputKeyboard.* is under GPLv2, like the USB Host Shield Library 2.0,
 *   because it's based on code from that library.
 * - EmulatedKeyboard.* is under MIT license, like NicoHood's keyboard code,
 *   because it's based on it.
 * - The rest (including this file) is under a stb-like public-domain license
 *   If in doubt check the license header of each source file.
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


#include "KeyboardCommon.hpp"

#include "EmulatedKeyboard.hpp"

#include "InputKeyboard.hpp"

//#include <usbhub.h>


static EmulatedKeyboard emuKB;

USB     UsbHost;
// TODO: would it be hard to make this work with keyboards with integrated HUB
// (like my old IBM thinkpad-style keyboard)?
//USBHub     Hub(&Usb);

#if 0 // TODO: if you wanna map keys to other keys, do it here (and uncomment the lines in OnKeyPress() and OnKeyRelease())
static uint16_t mapKey(uint16_t scancode)
{
	// See Chapter 10 "Keyboard/Keypad Page (0x07)" in hut1_21_0.pdf ("HID Usage Tables for USB")
	// for the meaning of the scancode values ("Usage  ID") of "normal keys" (not multimedia keys)
	if(scancode < KBCommon::MM_SC_OFFSET)
	{
		switch(uint8_t(scancode))
		{
			// example: map capslock to right CTRL
			case 0x39: // capslock
				return 0xE4; // right CTRL
			// NOTE: if you want to return a multimedia key here, you need to return
			//       KBCommon::MM_SC_OFFSET + mm_key_consumer_usage_ID; !
		}
	}
	else // "consumer page" key (multimedia key)
	{
		// See Chapter 15 "USB HID Consumer Page (0x0C)" in hut1_21_0.pdf
		uint16_t consumerUsageID = scancode - KBCommon::MM_SC_OFFSET;
		switch(consumerUsageID)
		{
			// example: map the mute key to play/pause
			case 0xE2: // Mute
				return KBCommon::MM_SC_OFFSET + 0xCD; // Play/Pause
			// NOTE that the scancodes for consumer page keys are
			//      KBCommon::MM_SC_OFFSET + consumer_usage_ID
			//      (so they don't clash with the normal keyboard keys scancodes)
		}
	}
	return scancode;
}
#endif // 0

class PassthroughKeyboard final : public InputKeyboard
{
public:
	PassthroughKeyboard(USB *p) : InputKeyboard(p) {}

	// called when a key is pressed, with its Scancode
	void OnKeyPress(uint16_t scancode) override
	{
#ifdef KBDWRAP_ENABLE_DEBUG
		PrintlnAll(F("PassthroughKeyboard::OnKeyPress( "), AsHex(scancode), F(" )") );
#endif

		// scancode = mapKey(scancode); // TODO: uncomment if you want to map keys to other keys

		emuKB.Press(scancode);
	}

	// called when a formerly pressed key is released, with its Scancode
	void OnKeyRelease(uint16_t scancode) override
	{
#ifdef KBDWRAP_ENABLE_DEBUG
		PrintlnAll(F("PassthroughKeyboard::OnKeyRelease( "), AsHex(scancode), F(" )") );
#endif

		// scancode = mapKey(scancode); // TODO: uncomment if you want to map keys to other keys

		emuKB.Release(scancode);
	}

	// called when a keyboard has been plugged in and initialization
	// on USB Host Lib's side is done
	uint8_t OnInitSuccessful() override
	{
		emuKB.ReleaseAll();
		return InputKeyboard::OnInitSuccessful();
	}

	// called when Keyboard is unplugged from the USB host shield
	// *also* called after start or plugging in, before other initialization steps
	uint8_t Release() override
	{
		// make sure no keys remain pressed when keyboard is unplugged
		emuKB.ReleaseAll();
		return InputKeyboard::Release();
	}

};

static PassthroughKeyboard inputKB(&UsbHost);

enum { ButtonPin = 4 };
void setup()
{
	pinMode(ButtonPin, INPUT_PULLUP);

#ifndef CDC_DISABLED
	Serial.begin( 115200 );
#ifdef KBDWRAP_ENABLE_DEBUG
	// Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
	// but as this will wait until the user has attached a serial console/"serial monitor", only do the waiting in debug mode
	while (!Serial){}
#endif
	Serial.println(F("Start"));
#endif // !CDC_DISABLED

	if (UsbHost.Init() == -1)
		PrintlnAll(F("OSC did not start."));

	// Set this to higher values to enable more debug information
	// minimum 0x00, maximum 0xff, default 0x80
	//UsbDEBUGlvl = 0xff;
#ifdef KBDWRAP_ENABLE_DEBUG
	UsbDEBUGlvl = 0x80;
#else
	UsbDEBUGlvl = 0;
#endif

	delay( 200 );
}

static int buttonState = 0;
void loop()
{

	// some test code to emulate a keypress when an arduino button is pressed (pin is LOW)
	int newButtonState = digitalRead(ButtonPin);
	if(newButtonState != buttonState)
	{
		buttonState = newButtonState;
		if(newButtonState == LOW) {
			//emuKB.Press(225); // LSHIFT
			emuKB.Press(7); // 'd' => 'D'
			//emuKB.Press(255+0xCD); // Play/Pause

		} else {
			emuKB.Release(7);
			//emuKB.Release(225); // LSHIFT
			//emuKB.Release(255+0xCD); // Play/Pause
		}

		PrintlnAll(F("LED state: "), emuKB.GetLeds());
	}

	// from here on: the actual adaptor code

	static bool wasSuspended = false;
	bool isSuspended = USBDevice.isSuspended();
	if(isSuspended != wasSuspended)
	{
		if(isSuspended)
		{
			inputKB.Suspend();
			emuKB.ReleaseAll();
		}
		else
		{
			inputKB.Resume();
			inputKB.SetLEDs(emuKB.GetLeds(), true); // force syncing LEDs on resume
		}

		wasSuspended = isSuspended;
	}

	if(!isSuspended)
	{
		UsbHost.Task(); // USB Host shield event processing or whatever
		inputKB.SetLEDs(emuKB.GetLeds());
		emuKB.Send();
	}
	else // currently suspended
	{
		delay(250); // sleep a bit
	}
}
