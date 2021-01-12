
// Do not remove the include below (says the Sloeber IDE)
#include "keyboardwrapper.h"

#ifndef CDC_DISABLED
// uncomment to log some debug info to the Serial console
//#define KBDWRAP_ENABLE_DEBUG
#endif

#include "EmulatedKeyboard.hpp"

//#include <usbhub.h>
#include <SPI.h>

#include <hidcomposite.h>

static EmulatedKeyboard emuKB;

USB     UsbHost;
// TODO: would it be hard to make this work with keyboards with integrated HUB
// (like my old IBM thinkpad-style keyboard)?
//USBHub     Hub(&Usb);


class InputKeyboard : public HIDComposite
{
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
	void GetUSBID(uint16_t& out_vid, uint16_t& out_pid)
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

	void DetectDevice()
	{
		// hopefully sane defaults:
		bootKbdEP = 1;
		bootKbdReportID = 0;
		mmKeysEP = 0; // TODO: try to find a consumer control endpoint/report?
		mmKeysReportID = 0;
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
		}

		if(devName != nullptr)
			PrintlnAll(F("Detected "), devName, F(" USB ID "), AsHex(vid), F(":"), AsHex(pid));
		else
			PrintlnAll(F("Unknown Keyboard (treating as Generic Boot Keyboard w/o Multimedia keys). USB ID "),
			           AsHex(vid), F(":"), AsHex(pid));

#undef CMB_USB_ID
	}

	void Reset()
	{
		curLedState = 0;
		oldModifierState = 0;
		memset(oldNormalKeysState, 0, sizeof(oldNormalKeysState));
		memset(oldMMKeysState, 0, sizeof(oldMMKeysState));
		memset(oldDK2108MMkeyState, 0, sizeof(oldDK2108MMkeyState));
		SetLEDs(0, true);
		emuKB.ReleaseAll();
	}

public:
	InputKeyboard(USB *p) : HIDComposite(p) {}
	virtual ~InputKeyboard(){}

	// Return true for the interface we want to hook into
	bool SelectInterface(uint8_t iface, uint8_t proto) override
	{
		(void)iface;
#ifdef KBDWRAP_ENABLE_DEBUG
		PrintlnAll(F("SelectInterface iface: "), iface, F(" proto "), proto);
#endif

		// from bInterfaceProtocol: 1 is Keyboard, 0 is unspecified
		// both are ok (this method is only called if bInterfaceClass is 3, HID)
		if (proto == 0 || proto == 1)
			return true;

		return false;
	}

	uint8_t OnInitSuccessful() override
	{
#ifdef KBDWRAP_ENABLE_DEBUG
		PrintlnAll(F("OnInitSuccessful()"));
#endif
		DetectDevice();
		Reset();
		return 0;
	}

	// data has 8 bytes: the first one contains the modifier state (one bit per left/right shift,alt,ctrl,gui/win)
	// the second byte contains garbage/nothing, the following six bytes contain keyboard scancodes
	// NOTE: this expects that any Report ID is already skipped!
	void HandleBootKeyboardReport(const uint8_t* data, int len)
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
			for(uint8_t bitIdx=0; bitIdx < EmuKB::NUM_MODIFIERS; ++bitIdx)
			{
				uint8_t bit = 1 << bitIdx;
				if(diffBits & bit)
				{
					uint16_t scancode = EmuKB::MIN_MODIFIER + bitIdx;
					bool pressed = (newModifierState & bit) != 0;
					pressed ? emuKB.Press(scancode) : emuKB.Release(scancode);
				}
			}
			oldModifierState = newModifierState;
		}

		// handle the normal keys

		// from this point on, only byte 3 and above are of interest
		len = min(len-2, EmuKB::NUM_BOOT_KEYS);
		const uint8_t* newKeys = &data[2];

		for(uint8_t i=0; i < len; ++i)
		{
			uint8_t sc = newKeys[i];
			if(sc != 0 && FindInArray(sc, oldNormalKeysState, EmuKB::NUM_BOOT_KEYS) == -1)
			{
				emuKB.Press(sc); // if this key wasn't already pressed, press it now
			}
		}

		for(uint8_t i=0; i < EmuKB::NUM_BOOT_KEYS; ++i)
		{
			uint8_t oldSc = oldNormalKeysState[i];
			if(oldSc != 0 && FindInArray(oldSc, newKeys, len) == -1)
			{
				emuKB.Release(oldSc); // if this key isn't pressed anymore, release it
			}
			oldNormalKeysState[i] = (i < len) ? newKeys[i] : 0;
		}
	}

	// expects data[] to contain byte-pairs with 16bit values of consumer control usage IDs
	// NOTE: this expects that any Report ID is already skipped!
	void HandleNormalMultimediaKeyReport(const uint8_t* data, int len)
	{
		uint8_t numNewKeys = min(len/2, EmuKB::NUM_MM_KEYS);
		uint16_t newKeys[EmuKB::NUM_MM_KEYS];
		for(uint8_t i=0; i < numNewKeys; ++i)
		{
			uint16_t usageID = data[2*i] + (uint16_t(data[2*i+1]) << 8);
			newKeys[i] = usageID;
			if(usageID != 0 && FindInArray(usageID, oldMMKeysState, EmuKB::NUM_MM_KEYS) == -1)
			{
				emuKB.Press(usageID + EmuKB::MM_SC_OFFSET); // remember: scancode is consumer key hid usageID + EmuKB::MM_SC_OFFSET
			}
		}

		for(uint8_t i=0; i < EmuKB::NUM_MM_KEYS; ++i)
		{
			uint16_t oldUsageID = oldMMKeysState[i];
			if(oldUsageID != 0 && FindInArray(oldUsageID, newKeys, numNewKeys) == -1)
			{
				emuKB.Release(oldUsageID + EmuKB::MM_SC_OFFSET);
			}
			oldMMKeysState[i] = (i<numNewKeys) ? newKeys[i] : 0;
		}
	}

	// expects data[] to contain 3bytes (24bits) for some assorted multimedia keys
	// NOTE: this expects that any Report ID is already skipped!
	void HandleDK2108MultimediaKeyReport(const uint8_t* data, int len)
	{
		if(len < 3)
			return;

		// the HID consumer control Usage IDs associated with the bits of each input byte
		static constexpr uint16_t bytesUsageIDs[3][8] = {
			{ 0x0b5, 0x0b6, 0x0b7, 0x0cd, 0x0e2, 0x0e5, 0x0e7, 0x0e9 },
			{ 0x0ea, 0x152, 0x153, 0x154, 0x155, 0x183, 0x18a, 0x192 },
			{ 0x194, 0x221, 0x223, 0x224, 0x225, 0x226, 0x227, 0x22a }
		};

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
						uint16_t scancode = bytesUsageIDs[byteIdx][i] + EmuKB::MM_SC_OFFSET;
						bool pressed = (newByte & bit) != 0;
						pressed ? emuKB.Press(scancode) : emuKB.Release(scancode);
					}
				}
				oldDK2108MMkeyState[byteIdx] = newByte;
			}
		}
	}

	// Called by the HIDComposite library
	// Will be called for all HID data received from the USB interface
	void ParseHIDData(USBHID *hid, uint8_t ep, bool has_rpt_id, uint8_t len, uint8_t *buf) override
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
							if(oldUsageID != 0)  emuKB.Release(oldUsageID + EmuKB::MM_SC_OFFSET);
							// remember: scancode is consumer key hid usage + EmuKB::MM_SC_OFFSET
							if(usageID != 0)     emuKB.Press(usageID + EmuKB::MM_SC_OFFSET);
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

	void SetLEDs(uint8_t ledState, bool force = false)
	{
		if(ledState != curLedState || force)
		{
			SetReport(0, 0/*hid->GetIface()*/, 2, 0, 1, &ledState);
			curLedState = ledState;
		}
	}

	void Suspend()
	{
		// disabling SOF (Start-of-Frame) marker generation will cause the connected
		// keyboard to suspend after a few milliseconds without traffic
		// (for that you also need to stop calling UsbHost.Task())
		uint8_t mode = pUsb->regRd(rMODE) & ~bmSOFKAENAB;
		pUsb->regWr(rMODE, mode);

		emuKB.ReleaseAll();
	}

	void Resume()
	{
		// re-enable SOF so the keyboard wakes up again
		uint8_t mode = pUsb->regRd(rMODE) | bmSOFKAENAB;
		pUsb->regWr(rMODE, mode);

		SetLEDs(emuKB.GetLeds(), true); // force syncing LEDs on resume
	}
};

static InputKeyboard inputKB(&UsbHost);

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
		isSuspended ? inputKB.Suspend() : inputKB.Resume();

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
