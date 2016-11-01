#include <Arduino.h>
#ifdef ESP8266
extern "C" {
#include "user_interface.h"
}
#endif
#include "FS.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "CMMC_OTA.h"
#include "WebServer.hpp"
#include "Utils.hpp"

CMMC_OTA ota;

const char * const OP_MODE_NAMES[]
{
    "NULL_MODE",
    "STATION_MODE",
    "SOFTAP_MODE",
    "STATIONAP_MODE"
};

// Example: Storing struct data in RTC user rtcDataory
//
// Struct data with the maximum size of 512 bytes can be stored
// in the RTC user rtcDataory using the ESP-specifc APIs.
// The stored data can be retained between deep sleep cycles.
// However, the data might be lost after power cycling the ESP8266.
//
// This example uses deep sleep mode, so connect GPIO16 and RST
// pins before running it.
//
// Created Mar 30, 2016 by Macro Yau.
//
// This example code is in the public domain.

// CRC function used to ensure data validity
uint32_t calculateCRC32(const uint8_t *data, size_t length);

// helper function to dump memory contents as hex
void printMemory();
void writeRTCMemory();
void restoreRTCDataFromRTCMemory();
void initRTCMemory();
void setupOTA() {
  ota.on_start([]() {

  });

  ota.on_end([]() {

  });

  ota.on_progress([](unsigned int progress, unsigned int total) {
      Serial.printf("_CALLBACK_ Progress: %u/%u\r\n", progress,  total);
  });

  ota.on_error([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
  });

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("cmmc-ota-esp8266");
}

// Structure which will be stored in RTC memory.
// First field is CRC32, which is calculated based on the
// rest of structure contents.
// Any fields can go after CRC32.
// We use byte array as an example.
struct {
  uint32_t crc32;
  byte data[508];
} rtcData;

static rst_info *rsti = NULL;
byte BYTE_MODE;
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

static Utils *u;

void initWiFiConfig() {
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }

  String path = "/wifi.json";
  if (u->isConfigExists(path)) {
    Serial.println("Config exists.");
    u->loadConfig(path);
  }
  else {
    Serial.println("Configuration..");
    Serial.println("Initialising....");
    if (u->initConfiguration(path)) {
      Serial.println("...DONE");
      u->loadConfig(path);
    }
    else {
      Serial.println("...FAILED");
    }
  }

}

void setup() {
  u = new Utils;
  rsti = ESP.getResetInfoPtr();
  Serial.begin(115200);
  Serial.println();
  initWiFiConfig();

  pinMode(0, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  delay(1000);
  Serial.printf("RST_INFO = %lu\r\n ", rsti->reason);
  Serial.printf("RESET REASON => %s \r\n", ESP.getResetReason().c_str());
  Serial.printf("WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
  Serial.printf("WiFi credentials => %s:%s \r\n", WiFi.SSID().c_str(), WiFi.psk().c_str());

  if (rsti->reason == REASON_DEFAULT_RST || rsti->reason == REASON_EXT_SYS_RST) {
    initRTCMemory();
    Serial.printf(">>WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
    WiFi.mode(WIFI_STA);
    Serial.printf(">>WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
  }
  else {
    restoreRTCDataFromRTCMemory();
    if (rtcData.data[0] == 0x01) {
      Serial.println("ENTER SETUP MODE... forEVER!");
      u->loadConfig("/wifi.json");
      WiFi.disconnect();
      Serial.println("LOADED...");

      // wifiConfigJson->printTo(Serial);
      // Serial.println("PRINTED...");
      // Serial.printf("WiFi Ap SSID = %s \r\n", wifiConfigJson->get("ap_ssid"));
      // Serial.printf("WiFi Ap SSID = %s \r\n", (*wifiConfigJson)["ap_ssid"]);
      // wifiConfigJson->printTo(Serial);
      Serial.println();
      Serial.println("X: ");
      Serial.println("Y: ");
      String apName = String("CMMC-") + String(ESP.getChipId(), HEX);
      WiFi.softAP(apName.c_str());
      delay(100);
      IPAddress myIP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(myIP);
      Serial.printf("WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
      static ESP8266WebServer server(80);
      JustPresso_WebServer webserver(&server);
      Serial.println("HTTP server started");
      setupOTA();
      ota.init();
      while(1) {
        webserver.handleClient();
        ota.loop();
        yield();
      };
    }
  }

  Serial.println("WAITING...");
  delay(2000);

  // GOING TO SETUP MODE...
  if (digitalRead(0) == LOW) {
    bool first = 1;
    while(digitalRead(0) == LOW) {
      if (first) {
          first = 0;
          digitalWrite(LED_BUILTIN, LOW);
          rtcData.data[0] = 0x01;
          writeRTCMemory();
          Serial.println("Release the button to take an effect.");
      }
      yield();
    }
    Serial.println("Restarting...");
    ESP.reset();
  }

  Serial.println("BYE...");
}

void loop() {
}

void restoreRTCDataFromRTCMemory() {
    if (ESP.rtcUserMemoryRead(0, (uint32_t*) &rtcData, sizeof(rtcData))) {
      uint32_t crcOfData = calculateCRC32(((uint8_t*) &rtcData) + 4, sizeof(rtcData) - 4);
      Serial.print("CRC32 of data: ");
      Serial.println(crcOfData, HEX);
      Serial.print("CRC32 read from RTC: ");
      Serial.println(rtcData.crc32, HEX);
      if (crcOfData != rtcData.crc32) {
        Serial.println("CRC32 in RTC memory doesn't match CRC32 of data. Data is probably invalid!");
      }
      else {
        Serial.println("CRC32 check ok, data is probably valid.");
      }
    }
}

void writeRTCMemory() {
  // Update CRC32 of data
  rtcData.crc32 = calculateCRC32(((uint8_t*) &rtcData) + 4, sizeof(rtcData) - 4);
  // Write struct to RTC memory
  if (ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtcData, sizeof(rtcData))) {
    Serial.println("Write: ");
    printMemory();
    Serial.println();
  }
}

uint32_t calculateCRC32(const uint8_t *data, size_t length)
{
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}

void printMemory() {
  char buf[3];
  for (int i = 0; i < sizeof(rtcData); i++) {
    sprintf(buf, "%02X", rtcData.data[i]);
    Serial.print(buf);
    if ((i + 1) % 32 == 0) {
      Serial.println();
    }
    else {
      Serial.print(" ");
    }
  }
  Serial.println();
}


void initRTCMemory() {
  // Generate new data set for the struct
  for (int i = 0; i < sizeof(rtcData); i++) {
    rtcData.data[i] = 0x00;
  }
  writeRTCMemory();
}
