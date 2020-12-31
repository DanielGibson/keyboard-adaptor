// Do not remove the include below
#include "keyboardwrapper.h"

#include <Keyboard.h>

enum { ButtonPin = 4 };

void setup()
{
	pinMode(ButtonPin, INPUT_PULLUP);
	Keyboard.begin();
}

static constexpr uint8_t key = 'b';
static int buttonState = 0;
void loop()
{
	int newButtonState = digitalRead(ButtonPin);
	if(newButtonState != buttonState)
	{
		buttonState = newButtonState;
		if(newButtonState == LOW)
			Keyboard.press(key);
		else
			Keyboard.release(key);
	}
}
