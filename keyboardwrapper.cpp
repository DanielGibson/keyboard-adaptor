// Do not remove the include below
#include "keyboardwrapper.h"

#include "EmulatedKeyboard.hpp"


#include <usbhub.h>

#include <SPI.h>

#include <hidescriptorparser.h>

enum { ButtonPin = 4 };

USB     Usb;
//USBHub     Hub(&Usb);

#if 01

#include <hidcomposite.h>

// Override HIDComposite to be able to select which interface we want to hook into
class HIDSelector : public HIDComposite
{
public:
    HIDSelector(USB *p) : HIDComposite(p) {};

protected:
    // Called by the HIDComposite library
    void ParseHIDData(USBHID *hid, uint8_t ep, bool is_rpt_id, uint8_t len, uint8_t *buf);

    bool SelectInterface(uint8_t iface, uint8_t proto);

    uint8_t OnInitSuccessful() override;
};

// Return true for the interface we want to hook into
bool HIDSelector::SelectInterface(uint8_t iface, uint8_t proto)
{
	PrintlnAll(F("SelectInterface iface: "), iface, F(" proto "), proto);

	//if (proto != 0)
	if(iface < 2)
		return true;

	return false;
}

uint8_t HIDSelector::OnInitSuccessful()
{
	//ReportDescParser rpt;
	//GetReportDescr(0, &rpt);
	//GetReport
	return 0;
}

// Will be called for all HID data received from the USB interface
void HIDSelector::ParseHIDData(USBHID *hid, uint8_t ep, bool has_rpt_id, uint8_t len, uint8_t *buf) {
#if 1
	(void)hid;
	if (len && buf)  {
		PrintAll(F("\r\n ep: "), ep, F(" is_rpt_id: "), (uint8_t)has_rpt_id, F(" data: "));
		for (uint8_t i = 0; i < len; i++) {
			PrintAll(AsHex(buf[i]), F(" "));
		}
		PrintlnAll();
	}
#endif
}
HIDSelector    hidSelector(&Usb);

#elif 0 //se

#include <hiduniversal.h>

HIDUniversal hidUniversal(&Usb);
#endif

static EmulatedKeyboard emuKB;

void setup()
{
	pinMode(ButtonPin, INPUT_PULLUP);

	Serial.begin( 115200 );
	while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
	Serial.println("Start");

	if (Usb.Init() == -1)
		Serial.println("OSC did not start.");

	// Set this to higher values to enable more debug information
	// minimum 0x00, maximum 0xff, default 0x80
	UsbDEBUGlvl = 0xff;

	delay( 200 );
}

static int buttonState = 0;
void loop()
{
	Usb.Task(); // USB Host shield event processing or whatever

	int newButtonState = digitalRead(ButtonPin);
	if(newButtonState != buttonState)
	{
		buttonState = newButtonState;
		if(newButtonState == LOW) {
			//emuKB.Press(225); // LSHIFT
			//emuKB.Press(7); // 'd' => 'D'
			emuKB.Press(255+0xCD); // Play/Pause

		} else {
			//emuKB.Release(7);
			//emuKB.Release(225); // LSHIFT
			emuKB.Release(255+0xCD); // Play/Pause
		}
		emuKB.Send();
		PrintlnAll("LED state: ", emuKB.GetLeds());
	}

}
