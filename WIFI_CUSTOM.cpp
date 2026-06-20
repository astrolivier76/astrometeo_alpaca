#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include "variablesWIFI.h"
#include "debug.h"
#include <esp_wifi.h>
#include "SPIFFS.h"

// --- PARAMETRES WIFI ---
unsigned long previousMillis = 0;
const long interval = 10000;
int reconnectAttempts = 0;
const int maxReconnectAttempts = 20; 
unsigned long lastWiFiCheck = 0;
const long wifiCheckInterval = 30000; 

bool isReconnecting = false;

// Serveur web d'urgence pour le portail de configuration
AsyncWebServer setupServer(80);

void loadWiFiConfig() {
    if (SPIFFS.exists("/wifi.txt")) {
        File file = SPIFFS.open("/wifi.txt", "r");
        if (file) {
            networkSSID = file.readStringUntil('\n');
            networkPassword = file.readStringUntil('\n');
            networkSSID.trim();
            networkPassword.trim();
            file.close();
            DEBUG_PRINTLN("Configuration WiFi chargée depuis SPIFFS");
        }
    }
}

void saveWiFiConfig(String qsid, String qpass) {
    File file = SPIFFS.open("/wifi.txt", "w");
    if (file) {
        file.println(qsid);
        file.println(qpass);
        file.close();
        DEBUG_PRINTLN("Configuration WiFi sauvegardée");
    }
}

void startAPMode() {
    DEBUG_PRINTLN("\n[WiFi] DÉMARRAGE DU POINT D'ACCÈS POUR CONFIGURATION");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("AstroMeteo_Setup");

    DEBUG_PRINT("Connectez-vous au réseau 'AstroMeteo_Setup' et allez sur l'IP : ");
    DEBUG_PRINTLN(WiFi.softAPIP());

    // Page 1 : Le formulaire
    setupServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<title>AstroMeteo - Setup WiFi</title>";
        html += "<style>body{font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif; background:#2c3e50; color:white; text-align:center; padding:20px; margin:0;}";
        html += ".container{background:#34495e; padding:30px; border-radius:12px; box-shadow:0 10px 20px rgba(0,0,0,0.3); max-width:400px; margin:40px auto;}";
        html += "h1{color:#46b8da; margin-bottom:20px;}";
        html += "input{margin:10px 0; padding:12px; width:100%; box-sizing:border-box; border-radius:6px; border:1px solid #7f8c8d; background:#ecf0f1; color:#2c3e50; font-size:16px;}";
        html += "button{margin-top:20px; padding:12px; width:100%; background:linear-gradient(135deg, #2ecc71 0%, #27ae60 100%); color:white; border:none; border-radius:6px; font-size:18px; font-weight:bold; cursor:pointer; transition:transform 0.2s;}";
        html += "button:hover{transform:scale(1.02);}</style></head>";
        html += "<body><div class='container'>";
        html += "<h1>AstroMeteo</h1><p>Configuration du réseau WiFi</p>";
        html += "<form action='/save' method='POST'>";
        html += "<input type='text' name='ssid' placeholder='Nom de votre box (SSID)' required><br>";
        html += "<input type='password' name='pass' placeholder='Mot de passe du WiFi' required><br>";
        html += "<button type='submit'>Connecter la station</button>";
        html += "</form></div></body></html>";
        request->send(200, "text/html", html);
    });

    // Page 2 : La confirmation de sauvegarde
    setupServer.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
        String newSSID = "";
        String newPass = "";
        if (request->hasParam("ssid", true)) newSSID = request->getParam("ssid", true)->value();
        if (request->hasParam("pass", true)) newPass = request->getParam("pass", true)->value();

        saveWiFiConfig(newSSID, newPass);

        // CORRECTION : Ajout du <head> avec charset UTF-8 et viewport pour mobile
        String response = "<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Sauvegarde AstroMeteo</title></head>";
        response += "<body style='background:#2c3e50; color:white; font-family:Arial; text-align:center; padding-top:50px;'>";
        response += "<div style='background:#34495e; padding:30px; border-radius:12px; max-width:500px; margin:0 auto;'>";
        response += "<h2 style='color:#2ecc71;'>Identifiants sauvegardés !</h2>";
        response += "<p>La station AstroMeteo redémarre et va se connecter à votre réseau.</p>";
        response += "<hr style='border:1px solid #7f8c8d; margin:20px 0;'>";
        response += "<h3>Étape suivante :</h3>";
        response += "<p>1. Reconnectez cet appareil à votre réseau WiFi habituel.</p>";
        response += "<p>2. Ouvrez votre navigateur et allez sur :</p>";
        response += "<h3 style='color:#46b8da;'>http://astrometeo.local</h3>";
        response += "</div></body></html>";
        
        request->send(200, "text/html", response);

        delay(3000);
        ESP.restart();
    });

    setupServer.begin();

    while(true) {
        delay(100);
    }
}

void WiFiEvent(arduino_event_id_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:      // <-- Nouveau nom
            DEBUG_PRINTLN("[WiFi] Connecté !");
            isReconnecting = false;
            reconnectAttempts = 0;
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: // <-- Nouveau nom
            DEBUG_PRINTLN("[WiFi] Déconnecté.");
            break;
        default:
            break;
    }
}

void initWiFi(void) {
    if (!SPIFFS.begin(true)) {
        DEBUG_PRINTLN("Erreur SPIFFS dans initWiFi");
    }
    
    loadWiFiConfig();

    if (networkSSID == "") {
        DEBUG_PRINTLN("Aucun réseau enregistré.");
        startAPMode(); 
    }

    WiFi.mode(WIFI_STA);
    WiFi.onEvent(WiFiEvent);
    
    WiFi.begin(networkSSID.c_str(), networkPassword.c_str());
    DEBUG_PRINT("Connexion au WiFi en cours");
    
    previousMillis = millis();
    while (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINT('.');
        delay(500);
        if (millis() - previousMillis >= 15000) { 
            DEBUG_PRINTLN("\n[WiFi] Impossible de joindre le réseau sauvegardé.");
            startAPMode(); 
            break; 
        }
    }
    
    WiFi.setSleep(true);
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    
    if(WiFi.status() == WL_CONNECTED){
        DEBUG_PRINTLN("\n[WiFi] Connecté au démarrage");
        DEBUG_PRINT("Adresse IP allouée par le DHCP: ");
        DEBUG_PRINTLN(WiFi.localIP());

        if (!MDNS.begin("astrometeo")) {
            DEBUG_PRINTLN("Erreur lors de la configuration du répondeur mDNS !");
        } else {
            DEBUG_PRINTLN("mDNS démarré : accessible via http://astrometeo.local");
        }
    }
}

void checkWiFi() {
    unsigned long currentMillis = millis();

    if (WiFi.status() == WL_CONNECTED) {
        isReconnecting = false;
        reconnectAttempts = 0;
        return;
    }

    if (currentMillis - lastWiFiCheck >= wifiCheckInterval) {
        lastWiFiCheck = currentMillis;
        
        DEBUG_PRINTLN("[WiFi] Tentative de reconnexion (Non-bloquant)...");
        WiFi.reconnect();  
        reconnectAttempts++;
        if (reconnectAttempts > maxReconnectAttempts) {
             DEBUG_PRINTLN("[WiFi] Trop d'échecs. Redémarrage du module.");
             ESP.restart(); 
        }
    }
}

String getLocalIPAddress() {
    return "{\"ip\":\"" + WiFi.localIP().toString() + "\"}";
}
