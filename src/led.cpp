#include <Arduino.h>
#include "led.h"

#define LED_BUILTIN 2

void initLED() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
}

void ledOn() {
    digitalWrite(LED_BUILTIN, LOW);
}

void ledOff() {
    digitalWrite(LED_BUILTIN, HIGH);
}