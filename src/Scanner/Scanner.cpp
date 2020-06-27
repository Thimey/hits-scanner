#include <Arduino.h>
#include <MFRC522.h>
#include <SPI.h>
#include "Scanner.h"

Scanner::Scanner(byte ssPin, byte rstPin)
    : MFRC522(ssPin, rstPin)
{}