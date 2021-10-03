
#include "KeyboardCommon.hpp"

#include "EmulatedKeyboard.hpp"

#include "InputKeyboard.hpp"

//#include <usbhub.h>


static EmulatedKeyboard emuKB;

USB     UsbHost;
// TODO: would it be hard to make this work with keyboards with integrated HUB
// (like my old IBM thinkpad-style keyboard)?
//USBHub     Hub(&Usb);

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

		emuKB.Press(scancode);
	}

	// called when a formerly pressed key is released, with its Scancode
	void OnKeyRelease(uint16_t scancode) override
	{
#ifdef KBDWRAP_ENABLE_DEBUG
		PrintlnAll(F("PassthroughKeyboard::OnKeyRelease( "), AsHex(scancode), F(" )") );
#endif

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
