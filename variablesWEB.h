#ifndef VARIABLES_WEB_H
#define VARIABLES_WEB_H

#include <Arduino.h>

// Structure pour les données des capteurs
struct SensorData {
    float temperature;
    float pression;
    float humidity;
    float dewpoint;
    float skyT;
    float nuages;
    float safe;
    float lux;
    float sqm;
    float vent;
    float pluie;
    int gouttes;
    float temp_gouttes;         // NOUVEAU : Température de surface du capteur
    unsigned long freq_gouttes; // NOUVEAU : Fréquence brute du LM555
    unsigned long lastUpdate;
};

// Déclaration des variables globales
extern SensorData cachedData;
extern SemaphoreHandle_t xSensorDataMutex;

// Constantes MLX et Seuils
extern float K1, K2, K3, K4, K5, K6, K7;
extern float temperature_ciel_clair;
extern float temperature_ciel_couvert;
extern float seuil_pluie; // NOUVEAU : Seuil de détection dynamique

// Déclaration des fonctions
void formatWebSocketData(const SensorData& data, char* buffer, size_t bufferSize);

#endif
