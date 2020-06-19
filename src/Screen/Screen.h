#include <Arduino.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH_PX 128
#define SCREEN_HEIGHT_PX 64

class Screen : public Adafruit_SSD1306
{
    public:
        Screen(
            int screenWidth = SCREEN_WIDTH_PX,
            int screenHeight = SCREEN_HEIGHT_PX,
            TwoWire *twoWire = &Wire,
            int resetPin = LED_BUILTIN);

        void printMessage(String message);
};
