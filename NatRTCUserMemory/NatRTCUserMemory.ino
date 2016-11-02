#include "CMMC_Manager.hpp"

CMMC_Manager manager(GPIO_ID_PIN0, LED_BUILTIN);
#include "CMMC_Interval.hpp"
CMMC_Interval interval;

void setup() {
    Serial.begin(115200);
    pinMode(GPIO_ID_PIN0, INPUT_PULLUP);
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
