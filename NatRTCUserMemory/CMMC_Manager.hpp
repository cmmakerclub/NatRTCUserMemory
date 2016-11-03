#ifndef CMMC_MANAGER_H
#define CMMC_MANAGER_H

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
#include "CMMC_Blink.hpp"
CMMC_Blink blinker;
CMMC_OTA ota;
#define CMMC_RTC_MODE_AP 0x01

const char * const OP_MODE_NAMES[]
{
    "NULL_MODE",
    "STATION_MODE",
    "SOFTAP_MODE",
    "STATIONAP_MODE"
};

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
  String ota_port = String("cmmc-ota-");
  ota_port += String(ESP.getChipId(), HEX);
  ArduinoOTA.setHostname(ota_port.c_str());
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

void init_filesystem() {
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
}

bool isConfigExists(String path) {
  bool configFile = SPIFFS.exists(path);
  #if DEBUG
    if (!configFile) {
      Serial.println("Failed to open config file.");
    }
    else {
      Serial.println("Config file loaded.");
    }
  #endif
  return configFile;
}

void config_define(const char* p, const JsonObject &json) {
  char tmp[30];
  strcpy(tmp, p);
  strcat(tmp, ".json");
  String path = String("/") + String(tmp);

  if (!isConfigExists(path)) {
    Serial.printf("%s is not exists\r\n", path.c_str());
    File configFile = SPIFFS.open(path.c_str(), "w");
    json.printTo(configFile);
    configFile.close();
  }
  else {
    File f = SPIFFS.open(path.c_str(), "r");
    Serial.printf("%s is existing \r\n content: \r\n", path.c_str());
    while(f.available()) {
      String str = f.readString();
      Serial.print(str);
    }
    Serial.println("\r\n/content");
    f.close();
  }
}

void webserver_forever() {
  File configFile = SPIFFS.open("/api_wifi_ap.json", "r");
  size_t size = configFile.size();
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because  ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());
  const char* _ssid = json["ssid"].asString();
  char* ssid = jsonBuffer.strdup(_ssid);

  Serial.println("ENTER SETUP MODE... forEVER!");
  Serial.println("LOADED...");
  if (String(ssid).length() == 0) {
  String apName = String("CMMC-") + String(ESP.getChipId(), HEX);
    WiFi.softAP(apName.c_str()); 
  }
  else {
    WiFi.softAP(ssid);
  }
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

void define_configurations() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  // DEFINE CONFIG_1
  String apName = String("CMMC-") + String(ESP.getChipId(), HEX);
  json.set("ssid", "");
  json.set("password", "");
  config_define("api_wifi_ap", json);

  // DEFINE CONFIG_2
  json.set("ssid", "");
  json.set("password", "");
  config_define("api_wifi_sta", json);
}




#include <Arduino.h>

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
    Serial.println("wrote RTC Memory..");
    // Serial.println("Write: ");
    // printMemory();
    // Serial.println();
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

class CMMC_Manager
{
  private:
    uint8_t _button_pin;
    uint8_t _led_pin;
    void _connect_wifi() {
      WiFi.disconnect();
      delay(20);
      WiFi.softAPdisconnect();
      delay(20);
      WiFi.mode(WIFI_STA);
      Serial.println("BYE...");

      File configFile = SPIFFS.open("/api_wifi_sta.json", "r");
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);

      // We don't use String here because  ArduinoJson library requires the input
      // buffer to be mutable. If you don't use ArduinoJson, you may as well
      // use configFile.readString instead.
      configFile.readBytes(buf.get(), size);

      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buf.get());
      const char* _ssid = json["ssid"].asString();
      const char* _password = json["password"].asString();
      char* ssid = jsonBuffer.strdup(_ssid);
      char* password = jsonBuffer.strdup(_password);

      blinker.blink(100, _led_pin);
      Serial.printf("Connecting to %s:%s\r\n", ssid, password);
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.println("We're online :)");
    }
    void _check_boot_mode() {
      Serial.printf("RST_INFO = %lu\r\n ", rsti->reason);
      Serial.printf("RESET REASON => %s \r\n", ESP.getResetReason().c_str());
      Serial.printf("WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
      // Serial.printf("WiFi credentials => %s:%s \r\n", WiFi.SSID().c_str(), WiFi.psk().c_str());
      if (rsti->reason == REASON_DEFAULT_RST || rsti->reason == REASON_EXT_SYS_RST) {
        initRTCMemory();
        Serial.printf(">>WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
      }
      else {
        blinker.detach(HIGH);
        restoreRTCDataFromRTCMemory();
        if (rtcData.data[0] == CMMC_RTC_MODE_AP) {
          webserver_forever();
        }
      }
    }

    void _wait_config_signal(uint8_t gpio) {
      Serial.println("WAITING... CONFIG PIN");
      unsigned long _c = millis();
      Serial.println(digitalRead(gpio));
      while(digitalRead(gpio) == LOW) {
        if((millis() - _c) >= 1000) {
          blinker.blink(200, _led_pin);
          rtcData.data[0] = CMMC_RTC_MODE_AP;
          WiFi.disconnect();
          WiFi.mode(WIFI_AP_STA);
          Serial.println("Release to take an effect.");
          while(digitalRead(gpio) == LOW) {
            yield();
          }
          Serial.println("Restarting...");
          writeRTCMemory();
          ESP.reset();
        }
        else {
          yield();
        }
      }
      Serial.println("/NORMAL");
  }

  public:
  CMMC_Manager(uint8_t button_pin, uint8_t led_pin)
    : _button_pin(button_pin), _led_pin(led_pin) {
  }
  void start() {
    rsti = ESP.getResetInfoPtr();
    Serial.println();
    pinMode(_button_pin, INPUT_PULLUP);
    pinMode(_led_pin, OUTPUT);
    digitalWrite(_led_pin, LOW);
    blinker.init();

    init_filesystem();
    define_configurations();
    _check_boot_mode();

    blinker.blink(50, _led_pin);
    delay(2000);
    _wait_config_signal(_button_pin);
    _connect_wifi();
    blinker.detach(HIGH);
  }

};
#endif
