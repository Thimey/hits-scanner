#include <string>
#include <Arduino.h>
#include "Screen.h"

Screen::Screen(int screenWidth, int screenHeight, TwoWire *twoWire, int resetPin)
    : Adafruit_SSD1306(screenWidth, screenHeight, twoWire, resetPin)
{}

void Screen::printMessage(String message) {
    clearDisplay();
    setTextSize(1);
    setTextColor(SSD1306_WHITE);
    setCursor(0,8);
    println(message);
    display();
};
