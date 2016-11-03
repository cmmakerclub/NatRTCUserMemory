#include <Arduino.h>
#include <MqttConnector.h>
#include <ArduinoJson.h>
#include <CMMC_Manager.h>


MqttConnector *mqtt;

#define MQTT_HOST         "mqtt.espert.io"
#define MQTT_PORT         1883
#define MQTT_USERNAME     ""
#define MQTT_PASSWORD     ""
#define MQTT_CLIENT_ID    ""
#define MQTT_PREFIX       "/CMMC"
#define PUBLISH_EVERY     (5000)// every 5 seconds

/* DEVICE DATA & FREQUENCY */
#define DEVICE_NAME       "CMMC-MANAGER-001"

#include "_publish.h"
#include "_receive.h"
#include "init_mqtt.h"

void init_hardware()
{
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println("Serial port initialized.");
}

void setup()
{
  init_hardware();

  CMMC_Manager manager(0, LED_BUILTIN);
  manager.start();
  init_mqtt();
}

void loop()
{
  mqtt->loop();
}
