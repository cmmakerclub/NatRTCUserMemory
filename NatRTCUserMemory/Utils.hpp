#ifndef NAT_RTC_UTILS_H
#define NAT_RTC_UTILS_H

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>
extern "C" {
  #include "user_interface.h"
}

#define DEBUG 1
class Utils {
  private:
    StaticJsonBuffer<400> jsonBufferUtils ;
    struct station_config  _station_config;
    // struct station_config  _station_config;
  public:
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

  bool loadConfig(String path) {
    File configFile = SPIFFS.open(path, "r");
    size_t size = configFile.size();

    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size]);

    // We don't use String here because ArduinoJson library requires the input
    // buffer to be mutable. If you don't use ArduinoJson, you may as well
    // use configFile.readString instead.
    configFile.readBytes(buf.get(), size);

    JsonObject &mJson = jsonBufferUtils.parseObject(buf.get());

    // this->_station_config
    memset(this->_station_config.ssid, 0, sizeof(this->_station_config.ssid));
    strcpy(reinterpret_cast<char*>(this->_station_config.ssid), mJson.get("ap_ssid"));

    memset(this->_station_config.password, 0, sizeof(this->_station_config.password));
    strcpy(reinterpret_cast<char*>(this->_station_config.ssid), mJson.get("ap_password"));


    if (!mJson.success()) {
      Serial.println("Failed to parse config file");
      mJson.printTo(Serial);
      configFile.close();
      return false;
    }
    else {
      Serial.println("SUCCESS LOAD JSON...");
      mJson.printTo(Serial);
      configFile.close();
      return true;
    }
  }

  bool initConfiguration(String path) {
    Serial.println("SETTING UP FIRST CONFIG..");
    JsonObject& json = jsonBufferUtils.createObject();
    String default_ap_ssid = String("CMMC-" ) + String(ESP.getChipId(), HEX);
    json["sta_ssid"]             = "";
    json["sta_password"]         = "";
    json["ap_ssid"]              = default_ap_ssid;
    json["ap_password"]          = "";
    wdt_disable();
    wdt_enable(WDTO_8S);
    return saveConfig(path, json);
  }

  bool saveConfig(String path, JsonObject& json) {
    File configFile = SPIFFS.open(path, "w");
    if (!configFile) {
      #if DEBUG
        Serial.println("Failed to open config file for writing");
        return false;
      #endif
    }
    else {
      #if DEBUG
        json.printTo(Serial);
        json.printTo(configFile);
        configFile.close();
        Serial.println();
        Serial.println("<<<<<< SAVE CONFIG");
      #endif
      return true;
    }
  }
};
#endif
