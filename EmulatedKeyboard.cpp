/*
 * Keyboard emulated by the Arduino, looks like a real keyboard to the host connected to Arduinos USB port
 * Uses USB Scancodes (kinda like SDL_Scancode), not ASCII chars!
 *
 * (C) 2021 Daniel Gibson
 */

#include "EmulatedKeyboard.hpp"

static const uint8_t _hidReportDescriptor[] PROGMEM = {
    //  Keyboard
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)  // 47
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x85, 0x02,                    //   REPORT_ID (2)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    // the modifiers byte (one bit for left/right ctrl, shift, Alt, Win)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
	0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    // the dummy byte
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
    // 6 normal keys
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, EmulatedKeyboard::MAX_NORMAL_KEY, //   LOGICAL_MAXIMUM (164)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
	0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, EmulatedKeyboard::MAX_NORMAL_KEY, //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    // TODO: LEDs
    0xc0,                          // END_COLLECTION
};

EmulatedKeyboard::EmulatedKeyboard()
  : hidNode(_hidReportDescriptor, sizeof(_hidReportDescriptor))
{
	HID().AppendDescriptor(&hidNode);

	// TODO: set mmKeys.reportID;
	// TODO: HID descriptor for mmKeys etc
}

static uint8_t scancodeToModifierFlag(uint16_t scancode)
{
	if(scancode >= EmulatedKeyboard::MIN_MODIFIER && scancode <= EmulatedKeyboard::MAX_MODIFIER)
	{
		return uint8_t(1 << (scancode-EmulatedKeyboard::MIN_MODIFIER));
	}
	return 0;
}

#if 0
static uint16_t mapKey(uint16_t scancode)
{
	// TODO: if you wanna map keys to other keys, do this here (and uncomment the lines in Press() and Release())

	// See Chapter 10 "Keyboard/Keypad Page (0x07)" in hut1_12_v2.pdf ("USB - HID Usage Tables")
	// for the meaning of the scancode values ("Usage  ID") of "normal keys" (not multimedia keys)
	if(scancode < 255)
	{
		switch(uint8_t(scancode))
		{
			// example: map capslock to right CTRL
			case 0x39: // capslock
				return 0xE4; // right CTRL
		}
	}
	else // "consumer page" key (multimedia key)
	{
		// See Chapter 15 "USB HID Consumer Page (0x0C)" in hut1_12_v2.pdf
		uint16_t consumerUsageID = scancode - 255;
		switch(consumerUsageID)
		{
			// example: map the mute key to play/pause
			case 0xE2: // Mute
				return 0xCD; // Play/Pause
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
		standardKeys.modifiers |= flag;
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
		}
	}
}

void EmulatedKeyboard::Release(uint16_t scancode)
{
	// scancode = mapKey(scancode); // TODO: uncomment if you want to map keys to other keys

	if(scancode >= MIN_MODIFIER && scancode <= MAX_MODIFIER)
	{
		uint8_t flag = scancodeToModifierFlag(scancode);
		standardKeys.modifiers &= ~flag;
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
		}
	}
}

void EmulatedKeyboard::ReleaseAll()
{
	standardKeys.modifiers = 0;
	for(uint8_t i=0; i < NUM_BOOT_KEYS; ++i)
	{
		standardKeys.keys[i] = 0;
	}
}

void EmulatedKeyboard::Send()
{
	HID().SendReport(2, &standardKeys, sizeof(BootKeyReport));

	// TODO: send multimedia keys

	// TODO: only send if anything has changed since lasts Send()
}
