#include "CMMC_Manager.h"
#include <FS.h>

#define BUTTON_INPUT_PIN 13
CMMC_Manager manager(BUTTON_INPUT_PIN, LED_BUILTIN);

#include "CMMC_Interval.hpp"
CMMC_Interval interval;


void setup() {
    delay(100);
    Serial.begin(115200);
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println(ESP.getCoreVersion());
    Serial.println(String(system_get_sdk_version()));

    pinMode(BUTTON_INPUT_PIN, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);
    manager.start();
}

void loop() {
  interval.every_ms(5000, []() {
    digitalWrite(LED_BUILTIN, LOW);
    delay(10);
    digitalWrite(LED_BUILTIN, HIGH);
  });
}
