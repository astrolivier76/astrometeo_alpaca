#ifndef OTA_H
#define OTA_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "SPIFFS.h"

class OTAHandler {
private:
    String logs;
    
public:
    OTAHandler();
    void begin(AsyncWebServer &server);
    void addToLogs(const String& message);
    void cleanLogs(); 
    
private:
    void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
};

#endif
