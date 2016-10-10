#include <Arduino.h>
#ifdef ESP8266
extern "C" {
#include "user_interface.h"
}
#endif
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>


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

void setup() {
  rsti = ESP.getResetInfoPtr();
  Serial.begin(115200);
  Serial.println();
  pinMode(0, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  delay(1000);

  Serial.printf("RST_INFO = %lu\r\n ", rsti->reason);
  Serial.printf("RESET REASON => %s \r\n", ESP.getResetReason().c_str());
  Serial.printf("WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);

  if (rsti->reason == REASON_DEFAULT_RST || rsti->reason == REASON_EXT_SYS_RST) {
    initRTCMemory();
    Serial.printf(">>WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
    WiFi.mode(WIFI_STA);
    Serial.printf(">>WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
  }
  else {
    restoreRTCDataFromRTCMemory();
    if (rtcData.data[0] == 0x01) {
      static ESP8266WebServer server(80);
      Serial.println("ENTER SETUP MODE... forEVER!");

      WiFi.softAP("NAT-HELLO-WORLD");
      IPAddress myIP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(myIP);
      server.on("/inline", [&](){
        server.send(200, "text/plain", "this works as well");
      });
      Serial.printf("WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
      server.begin();
      Serial.println("HTTP server started");
      while(1) {
        server.handleClient();
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
