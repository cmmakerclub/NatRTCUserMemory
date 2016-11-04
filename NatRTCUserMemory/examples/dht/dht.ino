#include <Arduino.h>
#include "CMMC_Interval.hpp"
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <MqttConnector.h>
#include "init_mqtt.h"
#include <Ticker.h>
#include "_publish.h"
#include "_receive.h"

const char* MQTT_HOST        = "mqtt.espert.io";
const char* MQTT_USERNAME    = "";
const char* MQTT_PASSWORD    = "";
const char* MQTT_CLIENT_ID   = "";
const char* MQTT_PREFIX      = "/CMMC";
const int MQTT_PORT           = 1883;

extern void _publish_run_once();
extern void _publish_run_loop();

extern void _receive_run_once();
extern void _receive_run_loop();

MqttConnector *mqtt;


void init_hardware()
{
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println("Starting...");

  _publish_run_once();
  _receive_run_once();
}
  #include <CMMC_Manager.h>
void setup()
{
  init_hardware();

  CMMC_Manager manager(0, LED_BUILTIN);
  manager.start();
  delay(50);
  init_mqtt();
}

void loop()
{
  mqtt->loop();
  _publish_loop();
  _receive_loop();
}
