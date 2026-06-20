#ifndef WIFI_CUSTOM_H
#define WIFI_CUSTOM_H
#include <Arduino.h>
#include <WiFi.h>

extern unsigned long lastWiFiCheck;
extern void WiFiEvent(arduino_event_id_t event); // <-- Modification ici
extern void initWiFi();
extern void checkWiFi();
extern String getLocalIPAddress();

#endif
