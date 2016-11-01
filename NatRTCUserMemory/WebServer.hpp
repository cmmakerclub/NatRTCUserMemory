#ifndef __JUST_PRESSO_WEBSERVER_H
#define __JUST_PRESSO_WEBSERVER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "FS.h"
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "Utils.hpp"

#ifdef ESP8266
extern "C" {
#include "user_interface.h"

}
#endif

int currentWiFiStatus = -1;
void WiFiStatus( bool force );
void WiFiStatus(bool bForse=false) {
  static struct station_config conf;
  wifi_station_get_config(&conf);

  int n = WiFi.status();
  if( bForse || n != currentWiFiStatus )  {
    Serial.printf( "WiFi status: %i\r\n", n );
    currentWiFiStatus = n;
    if( n == WL_CONNECTED ) {
      Serial.printf( "SSID: %s\n", WiFi.SSID().c_str() );
      const char* passphrase = reinterpret_cast<const char*>(conf.password);
      Serial.printf( "Password: (%i) %s\r\n", strlen( passphrase ), passphrase );
      Serial.print("Local IP: ");
      Serial.println(WiFi.localIP());
    }
  }
}

class JustPresso_WebServer {
  private:
    ESP8266WebServer *server = NULL;
  public:

    JustPresso_WebServer(ESP8266WebServer *server_pointer) {
      this->server = server_pointer;
      init_webserver();
    }

    void init_webserver() {
      static JustPresso_WebServer *that = this;
      server->on("/factory_reset", [&]() {
        SPIFFS.remove("/wifi.json");
        String out = String("FACTORY RESET.. BEING RESTARTED.");
        server->send(200, "text/plain", out.c_str());
        WiFi.disconnect();
        delay(1000);
        ESP.reset();
      });

      server->on("/save", HTTP_POST, []() {
        struct station_config conf;
        wifi_station_get_config(&conf);

        String ssid = String(reinterpret_cast<char*>(conf.ssid));
        String password = String(reinterpret_cast<char*>(conf.password));

        if (ssid == "" && password == "") {
          String out = String("Config WiFi First..");
          that->server->send(403, "text/plain", out.c_str());
          return;
        }

        // JsonObject& json = jsonBuffer.createObject();
        // json["ssid"]              = ssid;
        // json["password"]          = password;

        // bool config = justPresso.saveConfig(json);
        bool config = false;

        yield();
        // String out = String(config) + String(".... BEING REBOOTED.");
        String out = String(".... BEING REBOOTED.");
        that->server->send(200, "text/plain", out.c_str());
        // WiFi.disconnect();
        // WiFi.mode(WIFI_STA);
        // WiFi.begin(ssid.c_str(), password.c_str());
        // delay(1000);
        // ESP.reset();
      });

      server->on("/millis", []() {
        char buff[100];
        String ms = String(millis());
        sprintf(buff, "{\"millis\": %s }", ms.c_str());
        that->server->send (200, "text/plain", buff );
      });

      server->on("/api/wifi/scan", HTTP_GET, []() {
        char myIpString[24];
        IPAddress myIp = WiFi.localIP();
        sprintf(myIpString, "%d.%d.%d.%d", IP2STR(&myIp));

        // Serial.printf("client connect %d.%d.%d.%d", IP2STR(&myIp));
        int n = WiFi.scanNetworks();
        String currentSSID = WiFi.SSID();
        String output = "[";
        for( int i=0; i<n; i++ ) {
          if (output != "[") output += ',';
          output += "{\"name\": ";
          output += "\"";
          //if( currentSSID == WiFi.SSID(i) )
          //  output += "***>";
          output += WiFi.SSID(i);
          output += "\"";
          output += "}";
          yield();
        }
        output += "]";
        // output += ",\"current\":\""+currentSSID+"\"";
        // output += ", \"ip\":\""+String( myIpString )+"\"";
        // output += ", \"host\":\""+host_name+"\"";
        // output += ", \"name\":\""+system_name+"\"";
        // output += ", \"version\":\""+String(firmware_version)+"\"";
        // output += ", \"spiffs\":\""+String(spiffs_version)+"\"";

        Serial.println( output );
        that->server->send(200, "text/json", output);
      });

      server->on("/disconnect", HTTP_GET, [&](){
        WiFi.disconnect();
        server->send(200, "text/plain", "Disconnected.");
      });

      server->on( "/batt", []() {
        char buff[100];
        // sprintf(buff, "{\"batt\": %s }", String(getBatteryVoltage()).c_str());
        that->server->send (200, "text/plain", buff );
      });

      server->on( "/storage", []() {
        char buff[600];
        // justPresso.loadConfig();
        // justPresso.getJsonParser()->getRoot()->printTo(buff, sizeof(buff));
        that->server->send (200, "text/plain", buff );
      });

      server->on("/reset", HTTP_GET, [&](){
        server->send(200, "text/plain", "Device Restarted.");
        delay(100);
        ESP.reset();
      });

      server->on("/api/wifi/ap", [&]() {
        Serial.println("/api/wifi/ap ARGS=> ");
        String method = (server->method() == HTTP_GET ) ? "GET" : "POST";
        Serial.println(method);
        String json =  "{}";
        if (server->method() == HTTP_GET) {
          json = "{\"ssid\":\"__SSID__\",\"password\":\"__PASSWORD__\"}";
          json.replace("__SSID__", String("CMMC-")+String(ESP.getChipId(), HEX));
          json.replace("__PASSWORD__", "");
          that->server->send(200, "text/json", json);
        }
        else if (server->method() == HTTP_GET) {
          that->server->send(200, "text/json", json);
        }
        else {
          that->server->send(200, "text/json", json);
        }
      });
      server->on("/api/wifi/sta", HTTP_POST, [&]() {
        String sta_ssid = that->server->arg("sta_ssid");
        String sta_password = that->server->arg("sta_password");

        // String ap_ssid = that->server->arg("ap_ssid");
        // String ap_password = that->server->arg("ap_password");

        // justPresso.loadConfig();
        // JsonObject *jsonConfig= justPresso.getJsonParser()->getRoot();
        // (*jsonConfig)["ssid"] = ssid;
        // (*jsonConfig)["password"] = password;
        // justPresso.saveConfig(*jsonConfig);

        sta_ssid.replace("+", " ");
        sta_ssid.replace("%40", "@");

        // ap_ssid.replace("+", " ");
        // ap_ssid.replace("%40", "@");

        Serial.println("STA SSID: " + sta_ssid );
        Serial.println("STA Password: " + sta_password );

        // Serial.println("AP SSID: " + sta_ssid );
        // Serial.println("AP Password: " + sta_password );
        //
        WiFi.begin(sta_ssid.c_str(), sta_password.c_str());
        Serial.println("Waiting for WiFi to connect!");

        int c = 0;
        while (c < 50*5) {
          int n = WiFi.status();
          Serial.printf( "WiFi status: %i\n", n );
          if( n == WL_CONNECTED) {
            Serial.println();
            Serial.print("WiFi connected OK, local IP " );
            Serial.println( WiFi.localIP() );
            delay(50);
            break;
          }
          if( n == WL_IDLE_STATUS ) {
            Serial.println("No auto connect available!");
            break;
          }
          c++;
          yield();
          delay(45);
        }

        String json = "{";
        if (WiFi.status() == WL_CONNECTED) {
          char myIpString[24];
          IPAddress myIp = WiFi.localIP();
          sprintf(myIpString, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);
          json += "\"result\":\"success\"";
          json += ",\"current\":\""+sta_ssid+"\"";
          json += ", \"ip\":\""+String( myIpString )+"\"";
          Serial.println("WiFi: Success!");
          WiFiStatus(true);
        } else {
          json += "\"result\":\"failed\"";
          Serial.println("WiFi: Failed!");
        }
        json += "}";
        Serial.println( json );
        that->server->send(200, "text/json", json);
      });

      server->on("/reboot", HTTP_GET, [&](){
        server->send(200, "text/plain", "Device Restarted.");
        delay(100);
        ESP.reset();
      });

      server->serveStatic("/", SPIFFS, "/");
      server->begin();
    }

    void loop() {
      handleClient();
    }

    void handleClient() {
      if (server != NULL) {
        server->handleClient();
      }
      else {
        Serial.println("Uninializing server");
      }
    }
};


#endif
