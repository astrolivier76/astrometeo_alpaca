#ifndef ALPACA_API_H
#define ALPACA_API_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <freertos/semphr.h>

// Variables externes depuis ton programme principal
extern SemaphoreHandle_t xSensorDataMutex;
extern SensorData cachedData;

// --- CONSTANTES ASCOM ---
const int ASCOM_SUCCESS = 0;
const int ASCOM_NOT_IMPLEMENTED = 0x400;
const int ASCOM_INVALID_VALUE = 0x401;
const int ASCOM_NOT_CONNECTED = 0x407;
const int ASCOM_METHOD_NOT_IMPLEMENTED = 0x404;

// --- ETAT DE CONNEXION ---
bool obsCondConnected = true;

// --- FONCTION UTILITAIRE : FORMATAGE ALPACA ---
void sendAlpacaResponse(AsyncWebServerRequest *request, JSONVar value, int errNo = ASCOM_SUCCESS, String errMsg = "") {
    uint32_t clientTransactionID = 0;
    if (request->hasParam("ClientTransactionID")) {
        clientTransactionID = request->getParam("ClientTransactionID")->value().toInt();
    } else if (request->hasParam("ClientTransactionID", true)) { 
        clientTransactionID = request->getParam("ClientTransactionID", true)->value().toInt();
    }
    
    static uint32_t serverTransactionID = 1;
    
    JSONVar response;
    response["ClientTransactionID"] = clientTransactionID;
    response["ServerTransactionID"] = serverTransactionID++;
    response["ErrorNumber"] = errNo;
    response["ErrorMessage"] = errMsg;
    
    if (errNo == ASCOM_SUCCESS) {
        response["Value"] = value;
    }

    String jsonString = JSON.stringify(response);
    request->send(200, "application/json", jsonString);
}

// --- FONCTION UTILITAIRE : ENDPOINTS COMMUNS ---
void bindCommonAlpacaEndpoints(AsyncWebServer& server, String deviceType, int deviceNumber, String deviceName, String deviceDescription, bool& connectedState) {
    String basePath = "/api/v1/" + deviceType + "/" + String(deviceNumber);

    server.on((basePath + "/connected").c_str(), HTTP_GET, [&connectedState](AsyncWebServerRequest *request) {
        sendAlpacaResponse(request, connectedState);
    });
    server.on((basePath + "/connected").c_str(), HTTP_PUT, [&connectedState](AsyncWebServerRequest *request) {
        if (request->hasParam("Connected", true)) {
            String val = request->getParam("Connected", true)->value();
            val.toLowerCase();
            connectedState = (val == "true");
            sendAlpacaResponse(request, ""); 
        } else {
            sendAlpacaResponse(request, "", ASCOM_INVALID_VALUE, "Paramètre Connected manquant");
        }
    });

    server.on((basePath + "/name").c_str(), HTTP_GET, [deviceName](AsyncWebServerRequest *request) { sendAlpacaResponse(request, deviceName); });
    server.on((basePath + "/description").c_str(), HTTP_GET, [deviceDescription](AsyncWebServerRequest *request) { sendAlpacaResponse(request, deviceDescription); });
    server.on((basePath + "/driverinfo").c_str(), HTTP_GET, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, "Meteo Astro ESP32 Alpaca Driver"); });
    server.on((basePath + "/driverversion").c_str(), HTTP_GET, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, "1.0"); });
    server.on((basePath + "/interfaceversion").c_str(), HTTP_GET, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, 1); });

    server.on((basePath + "/supportedactions").c_str(), HTTP_GET, [](AsyncWebServerRequest *request) { JSONVar actions; sendAlpacaResponse(request, actions); });
    server.on((basePath + "/action").c_str(), HTTP_PUT, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, "", ASCOM_METHOD_NOT_IMPLEMENTED, "Action not implemented"); });
    server.on((basePath + "/commandblind").c_str(), HTTP_PUT, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, "", ASCOM_METHOD_NOT_IMPLEMENTED, "CommandBlind not implemented"); });
    server.on((basePath + "/commandbool").c_str(), HTTP_PUT, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, "", ASCOM_METHOD_NOT_IMPLEMENTED, "CommandBool not implemented"); });
    server.on((basePath + "/commandstring").c_str(), HTTP_PUT, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, "", ASCOM_METHOD_NOT_IMPLEMENTED, "CommandString not implemented"); });
}

// --- INITIALISATION PRINCIPALE DES ROUTES ---
void initAlpacaAPI(AsyncWebServer& server) {

    // 1. API DE DECOUVERTE (Management) - Uniquement Observing Conditions
    server.on("/management/apiversions", HTTP_GET, [](AsyncWebServerRequest *request) {
        JSONVar value; value[0] = 1; sendAlpacaResponse(request, value);
    });
    server.on("/management/v1/configureddevices", HTTP_GET, [](AsyncWebServerRequest *request) {
        JSONVar devices;
        JSONVar devCond; 
        devCond["DeviceName"] = "Meteo Astro Conditions"; 
        devCond["DeviceType"] = "ObservingConditions"; 
        devCond["DeviceNumber"] = 0; 
        devCond["UniqueID"] = "ESP32-ObsCond-001";
        devices[0] = devCond;
        sendAlpacaResponse(request, devices);
    });

    // 2. ENDPOINTS COMMUNS
    bindCommonAlpacaEndpoints(server, "observingconditions", 0, "Meteo Astro Conditions", "Station Météo Astronomique", obsCondConnected);

    // 3. ENDPOINTS SPECIFIQUES : OBSERVING CONDITIONS
    String obsPath = "/api/v1/observingconditions/0";
    
    auto checkConnAndMutex = [](AsyncWebServerRequest *request, double& valueToRead, std::function<double()> getter) {
        if (!obsCondConnected) { sendAlpacaResponse(request, 0.0, ASCOM_NOT_CONNECTED, "Device not connected"); return; }
        if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            valueToRead = getter();
            xSemaphoreGive(xSensorDataMutex);
            sendAlpacaResponse(request, valueToRead);
        } else { sendAlpacaResponse(request, 0.0, ASCOM_INVALID_VALUE, "Bus I2C occupé"); }
    };

    // Capteurs implémentés
    server.on((obsPath + "/temperature").c_str(), HTTP_GET, [checkConnAndMutex](AsyncWebServerRequest *r) { double v; checkConnAndMutex(r, v, [](){ return cachedData.temperature; }); });
    server.on((obsPath + "/humidity").c_str(), HTTP_GET, [checkConnAndMutex](AsyncWebServerRequest *r) { double v; checkConnAndMutex(r, v, [](){ return cachedData.humidity; }); });
    server.on((obsPath + "/dewpoint").c_str(), HTTP_GET, [checkConnAndMutex](AsyncWebServerRequest *r) { double v; checkConnAndMutex(r, v, [](){ return cachedData.dewpoint; }); });
    server.on((obsPath + "/pressure").c_str(), HTTP_GET, [checkConnAndMutex](AsyncWebServerRequest *r) { double v; checkConnAndMutex(r, v, [](){ return cachedData.pression; }); });
    server.on((obsPath + "/windspeed").c_str(), HTTP_GET, [checkConnAndMutex](AsyncWebServerRequest *r) { double v; checkConnAndMutex(r, v, [](){ return cachedData.vent / 3.6; }); }); // km/h -> m/s
    server.on((obsPath + "/cloudcover").c_str(), HTTP_GET, [checkConnAndMutex](AsyncWebServerRequest *r) { double v; checkConnAndMutex(r, v, [](){ return cachedData.nuages; }); });
    server.on((obsPath + "/skytemperature").c_str(), HTTP_GET, [checkConnAndMutex](AsyncWebServerRequest *r) { double v; checkConnAndMutex(r, v, [](){ return cachedData.skyT; }); });
    server.on((obsPath + "/skybrightness").c_str(), HTTP_GET, [checkConnAndMutex](AsyncWebServerRequest *r) { double v; checkConnAndMutex(r, v, [](){ return cachedData.sqm; }); });
    server.on((obsPath + "/rainrate").c_str(), HTTP_GET, [checkConnAndMutex](AsyncWebServerRequest *r) { double v; checkConnAndMutex(r, v, [](){ return cachedData.pluie; }); });

    // Non Implémentés
    server.on((obsPath + "/averageperiod").c_str(), HTTP_GET, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, 0.0, ASCOM_NOT_IMPLEMENTED, "Average period not implemented"); });
    server.on((obsPath + "/averageperiod").c_str(), HTTP_PUT, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, "", ASCOM_NOT_IMPLEMENTED, "Average period not implemented"); });
    server.on((obsPath + "/refresh").c_str(), HTTP_PUT, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, "", ASCOM_METHOD_NOT_IMPLEMENTED, "Refresh not implemented"); });
    server.on((obsPath + "/starfwhm").c_str(), HTTP_GET, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, 0.0, ASCOM_NOT_IMPLEMENTED, "FWHM not implemented"); });
    server.on((obsPath + "/winddirection").c_str(), HTTP_GET, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, 0.0, ASCOM_NOT_IMPLEMENTED, "Wind direction not implemented"); });
    server.on((obsPath + "/windgust").c_str(), HTTP_GET, [](AsyncWebServerRequest *request) { sendAlpacaResponse(request, 0.0, ASCOM_NOT_IMPLEMENTED, "Wind gust not implemented"); });
    server.on((obsPath + "/sensordescription").c_str(), HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("SensorName")) { sendAlpacaResponse(request, "", ASCOM_INVALID_VALUE, "SensorName manquant"); return; }
        sendAlpacaResponse(request, "", ASCOM_NOT_IMPLEMENTED, "SensorDescription not implemented");
    });
}

#endif
