#include <Arduino.h>
#include "Buzzer.h"

Buzzer::Buzzer(int pin) : pin(pin) {}

void Buzzer::tone(int freq)
{
    // setup beeper
    ledcSetup(0, 2000, 8);
    // attach beeper
    ledcAttachPin(pin, 0);
    // play tone
    ledcWriteTone(0, freq);
};

void Buzzer::noTone()
{
    tone(0);
};

void Buzzer::beep()
{
    tone(2000);
    delay(300);
    noTone();
};
