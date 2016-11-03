#ifndef __CMMC_MANAGER_WEBSERVER_H
#define __CMMC_MANAGER_WEBSERVER_H

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
#include "CMMC_Blink.hpp"

#ifdef ESP8266

extern CMMC_Blink blinker;
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
    bool _success = false;
    bool _connected = false;
  public:

    JustPresso_WebServer(ESP8266WebServer *server_pointer) {
      this->server = server_pointer;
      init_webserver();
      WiFi.onStationModeConnected([](const WiFiEventStationModeConnected &conn) {
        Serial.println("connected");
      });

      WiFi.onStationModeGotIP([&](const WiFiEventStationModeGotIP &ip) {
        Serial.println("got ip");
        this->_success = false;
        this->_connected = true;
      });

      WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &dis) {
        Serial.println("Disconnected.");
        Serial.printf("REASON: %d\r\n", dis.reason);
      });

      static JustPresso_WebServer *__this = this;
      WiFi.onEvent([&](WiFiEvent_t event) {
        // Serial.printf("[WiFi-event] event: %d\n", event);
        switch(event) {
          case WIFI_EVENT_STAMODE_GOT_IP:
              Serial.println("WiFi connected");
              Serial.println("IP address: ");
              Serial.println(WiFi.localIP());
              Serial.println("response !");
              __this->_success = true;
              __this->_connected = true;
              break;
          case WIFI_EVENT_STAMODE_DISCONNECTED:
              Serial.println("WiFi lost connection");
              break;
        }
      }, WIFI_EVENT_ANY);
    }



          String getContentType(ESP8266WebServer *server, String filename) {
            if (server->hasArg("download")) return "application/octet-stream";
            else if (filename.endsWith(".htm")) return "text/html";
            else if (filename.endsWith(".html")) return "text/html";
            else if (filename.endsWith(".css")) return "text/css";
            else if (filename.endsWith(".js")) return "application/javascript";
            else if (filename.endsWith(".png")) return "image/png";
            else if (filename.endsWith(".gif")) return "image/gif";
            else if (filename.endsWith(".jpg")) return "image/jpeg";
            else if (filename.endsWith(".ico")) return "image/x-icon";
            else if (filename.endsWith(".xml")) return "text/xml";
            else if (filename.endsWith(".pdf")) return "application/x-pdf";
            else if (filename.endsWith(".zip")) return "application/x-zip";
            else if (filename.endsWith(".gz")) return "application/x-gzip";
            return "text/plain";
          }

          bool handleFileRead(ESP8266WebServer *server, String path) {
            Serial.println("handleFileRead: " + path);
            if ( path.endsWith("/") )
              path += "index.htm";
            String contentType = getContentType(server, path);
            String pathWithGz = path + ".gz";
            if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
              if (SPIFFS.exists(pathWithGz))
                path += ".gz";
              File file = SPIFFS.open(path, "r");
              size_t sent = server->streamFile(file, contentType);
              file.close();
              return true;
            }
            return false;
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


      server->onNotFound([]() {
        // if (!that->handleFileRead(that->server, that->server->uri()))
          that->server->send(404, "text/plain", "ERROR 404: File Not Found.");
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
        blinker.blink(1000, LED_BUILTIN);
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
        String path = String("/") + String("api_wifi_ap.json");
        if (server->method() == HTTP_GET) {
          File configFile = SPIFFS.open(path.c_str(), "r");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);

          // We don't use String here because  ArduinoJson library requires the input
          // buffer to be mutable. If you don't use ArduinoJson, you may as well
          // use configFile.readString instead.
          configFile.readBytes(buf.get(), size);

          StaticJsonBuffer<200> jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(buf.get());
          char buffer[256];
          json.printTo(buffer, sizeof(buffer));
          configFile.close();
          Serial.println(buffer);
          that->server->send(200, "text/json", buffer);
        }
        else if (server->method() == HTTP_POST) {
          File configFile = SPIFFS.open(path.c_str(), "w");
          String ap_ssid = that->server->arg("ap_ssid");
          String ap_password = that->server->arg("ap_password");
          // SAVE CONFIG
          StaticJsonBuffer<200> jsonBuffer;
          JsonObject& json = jsonBuffer.createObject();
          json.set("ssid", ap_ssid);
          json.set("password", ap_password);
          json.printTo(configFile);
          configFile.close();

          char buffer[256];
          json.printTo(buffer, sizeof(buffer));
          configFile.close();
          Serial.println(buffer);
          that->server->send(200, "text/json", buffer);
        }
        else {
          that->server->send(501, "text/plain", "method not implemented.");
        }
      });

      server->on("/api/wifi/sta", HTTP_POST, [&]() {
        String sta_ssid = that->server->arg("sta_ssid");
        String sta_password = that->server->arg("sta_password");

        sta_ssid.replace("+", " ");
        sta_ssid.replace("%40", "@");

        Serial.println("STA SSID: " + sta_ssid );
        Serial.println("STA Password: " + sta_password );

        blinker.blink(50, LED_BUILTIN);
        WiFi.begin(sta_ssid.c_str(), sta_password.c_str());
        Serial.println("Waiting for WiFi to connect!");

        this->_connected = false;
        this->_success =  false;
        static JustPresso_WebServer* nat = this;

        // WiFi.onStationModeDHCPTimeout([](void) {
        //   Serial.println("TIMEOUT");
        // });

        unsigned long _prev = millis();
        while(!this->_connected) {
          yield();
          if (millis() - _prev  > 15000) {
            Serial.println("time out.");
            break;
          }
        }

        if (_success) {
          Serial.println("success.");
          String json = "{";
            char myIpString[24];
            IPAddress myIp = WiFi.localIP();
            sprintf(myIpString, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);
            json += "\"result\":\"success\"";
            json += ",\"current\":\""+sta_ssid+"\"";
            json += ", \"ip\":\""+String( myIpString )+"\"";
            json += "}";
            Serial.println("WiFi: Success!");
            // SAVE CONFIG
            StaticJsonBuffer<200> jsonBuffer;
            JsonObject& jsonObject = jsonBuffer.createObject();
            jsonObject.set("ssid", sta_ssid);
            jsonObject.set("password", sta_password);
            String path = String("/") + String("api_wifi_sta.json");
            File configFile = SPIFFS.open(path.c_str(), "w");
            jsonObject.printTo(configFile);
            configFile.close();
            // WiFiStatus(true);
            // that->server->send(200, "text/json", "{\"status\": \"success\"}");
            Serial.println(json);
            WiFiStatus(true);
            blinker.blink(1000, LED_BUILTIN);
            that->server->send(200, "text/json", json);
        }
        else {
          Serial.println("failed.");
          that->server->send(200, "text/json", "{\"status\": \"failed\"}");
        }
        //
        // int c = 0;
        // while (c < 50*5) {
        //   int n = WiFi.status();
        //   Serial.printf( "WiFi status: %i\n", n );
        //   if( n == WL_CONNECTED) {
        //     Serial.println();
        //     Serial.print("WiFi connected OK, local IP " );
        //     Serial.println( WiFi.localIP() );
        //     delay(50);
        //     break;
        //   }
        //   if( n == WL_IDLE_STATUS ) {
        //     Serial.println("No auto connect available!");
        //     break;
        //   }
        //   c++;
        //   yield();
        //   delay(45);
        // }
        //
        // String json = "{";
        // if (WiFi.status() == WL_CONNECTED) {
        // } else {
        //   json += "\"result\":\"failed\"";
        //   Serial.println("WiFi: Failed!");
        // }
        // json += "}";
        // Serial.println( json );
        // that->server->send(200, "text/json", json);
      });

      server->on("/reboot", HTTP_GET, [&](){
        server->send(200, "text/plain", "Device Restarted.");
        delay(100);
        ESP.reset();
      });

      server->serveStatic("/font", SPIFFS, "/font","max-age=86400");
      server->serveStatic("/js",   SPIFFS, "/js"  ,"max-age=86400");
      server->serveStatic("/css",  SPIFFS, "/css" ,"max-age=86400");
      server->serveStatic("/", SPIFFS, "/");
      server->begin();
      blinker.blink(500, LED_BUILTIN);
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
