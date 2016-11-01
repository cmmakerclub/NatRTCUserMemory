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
#include "CMMC_Interval.hpp"
CMMC_Blink blinker;
CMMC_Interval interval;


#define CMMC_RTC_MODE_AP 0x01

CMMC_OTA ota;

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
  // String apName = String("CMMC-") + String(ESP.getChipId(), HEX);
  WiFi.softAP(ssid);
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

void define_setup() {
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

void setup() {
  rsti = ESP.getResetInfoPtr();
  Serial.begin(115200);
  Serial.println();

  pinMode(0, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  blinker.init();

  initWiFiConfig();
  define_setup();

  Serial.printf("RST_INFO = %lu\r\n ", rsti->reason);
  Serial.printf("RESET REASON => %s \r\n", ESP.getResetReason().c_str());
  Serial.printf("WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
  // Serial.printf("WiFi credentials => %s:%s \r\n", WiFi.SSID().c_str(), WiFi.psk().c_str());
  if (rsti->reason == REASON_DEFAULT_RST || rsti->reason == REASON_EXT_SYS_RST) {
    initRTCMemory();
    Serial.printf(">>WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
    Serial.printf(">>WiFi.mode() => %s \r\n", OP_MODE_NAMES[WiFi.getMode()]);
  }
  else {
    blinker.detach(HIGH);
    restoreRTCDataFromRTCMemory();
    if (rtcData.data[0] == CMMC_RTC_MODE_AP) {
      webserver_forever();
    }
  }

  blinker.blink(50, LED_BUILTIN);
  delay(2000);
  unsigned long _c = millis();
  while(digitalRead(0) == LOW) {
    if((millis() - _c) >= 2000) {
      blinker.blink(200, LED_BUILTIN);
      rtcData.data[0] = CMMC_RTC_MODE_AP;
      WiFi.disconnect();
      WiFi.mode(WIFI_AP_STA);
      Serial.println("Release to take an effect.");
      while(digitalRead(0) == LOW) {
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

  WiFi.disconnect();
  delay(40);
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

  blinker.blink(100, LED_BUILTIN);
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
  blinker.detach(HIGH);
}

void loop() {
  interval.every_ms(5000, []() {
    digitalWrite(LED_BUILTIN, LOW);
    delay(10);
    digitalWrite(LED_BUILTIN, HIGH);     
  });
}

#include "fn.h"
