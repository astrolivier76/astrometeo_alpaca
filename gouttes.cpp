#include <Arduino.h>
#include "gouttes.h"
#include "debug.h"
#include <math.h>
#include "variablesWEB.h" // NOUVEAU : Pour accéder à seuil_pluie

namespace {
    struct CapteurGouttes {
        const uint8_t pinFreq = 34;       
        const uint8_t pinNTC = 32;        
        const uint8_t pinHeater = 13;     
        
        int valeur = 0;                   
        unsigned long frequence = 0;      
        float tempSurface = 0.0;          
    };
    CapteurGouttes capteur;

    volatile unsigned long impulsions = 0;

    void IRAM_ATTR compterImpulsions() {
        impulsions++;
    }
}

void init_GOUTTES() {
    pinMode(capteur.pinFreq, INPUT); 
    pinMode(capteur.pinHeater, OUTPUT);
    digitalWrite(capteur.pinHeater, LOW); 
}

void lireTemperatureSurface() {
    int adcVal = analogRead(capteur.pinNTC);
    
    if (adcVal > 0 && adcVal < 4095) {
        float rNtc = 1000.0 * ((float)adcVal / (4095.0 - (float)adcVal));
        float temp = rNtc / 1000.0;          
        temp = log(temp);                    
        temp /= 3400.0;                      
        temp += 1.0 / (25.0 + 273.15);       
        temp = 1.0 / temp;                   
        capteur.tempSurface = temp - 273.15; 
        
        DEBUG_PRINTF("[CAPTEUR PLUIE] Température surface : %.1f °C\n", capteur.tempSurface);
    }
}

void updateGOUTTES() {
    lireTemperatureSurface();

    impulsions = 0;
    attachInterrupt(digitalPinToInterrupt(capteur.pinFreq), compterImpulsions, RISING);
    vTaskDelay(pdMS_TO_TICKS(500)); 
    detachInterrupt(digitalPinToInterrupt(capteur.pinFreq));

    capteur.frequence = impulsions * 2;
    DEBUG_PRINTF("[CAPTEUR PLUIE] Fréquence : %lu Hz\n", capteur.frequence);

    // MODIFICATION : Utilisation de la variable modifiable par le Web !
    if (capteur.frequence == 0) {
        capteur.valeur = 1;
    } else if (capteur.frequence < seuil_pluie) {
        capteur.valeur = 1;
    } else {
        capteur.valeur = 0;
    }
}

float getTemperatureSurfaceGouttes() {
    return capteur.tempSurface;
}

unsigned long getFrequenceGouttes() {
    return capteur.frequence;
}

int getGOUTTES() {
    return capteur.valeur;
}
