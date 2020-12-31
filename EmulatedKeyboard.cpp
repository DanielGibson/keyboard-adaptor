/*
 *
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
}

static uint8_t scancodeToModifierFlag(uint16_t scancode)
{
	if(scancode >= EmulatedKeyboard::MIN_MODIFIER && scancode <= EmulatedKeyboard::MAX_MODIFIER)
	{
		return uint8_t(1 << (scancode-EmulatedKeyboard::MIN_MODIFIER));
	}
	return 0;
}

void EmulatedKeyboard::Press(uint16_t scancode)
{
#if 0
    SDL_SCANCODE_LCTRL = 224,
    SDL_SCANCODE_LSHIFT = 225,
    SDL_SCANCODE_LALT = 226, /**< alt, option */
    SDL_SCANCODE_LGUI = 227, /**< windows, command (apple), meta */
    SDL_SCANCODE_RCTRL = 228,
    SDL_SCANCODE_RSHIFT = 229,
    SDL_SCANCODE_RALT = 230, /**< alt gr, option */
    SDL_SCANCODE_RGUI = 231, /**< windows, command (apple), meta */
#endif
	if(scancode >= MIN_MODIFIER && scancode <= MAX_MODIFIER)
	{
		uint8_t flag = scancodeToModifierFlag(scancode);
		standardKeys.modifiers |= flag;
	}
	else if(scancode <= MAX_NORMAL_KEY)
	{
		for(uint8_t i=0; i < NUM_BOOT_KEYS; ++i)
		{
			if(standardKeys.keys[i] == 0)
			{
				standardKeys.keys[i] = (uint8_t)scancode;
				break;
			}
		}
	}
	else
	{
		// TODO: multimedia keys (consumer page)
	}
}

void EmulatedKeyboard::Release(uint16_t scancode)
{
	if(scancode >= MIN_MODIFIER && scancode <= MAX_MODIFIER)
	{
		uint8_t flag = scancodeToModifierFlag(scancode);
		standardKeys.modifiers &= ~flag;
	}
	else if(scancode < MAX_NORMAL_KEY)
	{
		for(uint8_t i=0; i < NUM_BOOT_KEYS; ++i)
		{
			if(standardKeys.keys[i] == (uint8_t)scancode)
			{
				standardKeys.keys[i] = 0;
			}
		}
	}
	else
	{
		// TODO: multimedia keys (consumer page)
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

	// TODO: multimedia keys
}
