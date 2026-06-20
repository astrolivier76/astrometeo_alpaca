#include <Arduino.h>
#include <Wire.h>
#include "debug.h"
//#define SIMULATION_MODE
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2591.h>
Adafruit_TSL2591 tsl = Adafruit_TSL2591();

// --- CONSTANTES TSL2591 ---
float lux = 0.00;
float sqm = 0.00;
bool tslAvailable = false;
int tslRetryCount = 0;
unsigned long tslLastRetryTime = 0;
const int tslMaxRetries = 3;
const unsigned long tslRetryInterval = 30000; // 30 secondes

// --- MODE SIMULATION ---
#ifdef SIMULATION_MODE
bool isTSL2591Present(){
    tslAvailable = true;
    return true;
}
bool calibrateTSL2591(){
    return true;
}
void updateTSL2591(){}
float getLux() {
  if (!tslAvailable) return -999.0;
  lux = 500 + random(-100, 100);
  return lux;
}

float getSQM() {
  if (!tslAvailable) return -999.0;
  updateTSL2591();
  lux = getLux();
  sqm = log10(lux/108000)/-0.4;
  return sqm;
}

#else

// --- MODE REEL ---
struct {
  bool status;
  uint32_t full;
  uint16_t ir;
  uint16_t visible;
  int      gain;
  int      timing;
  float    lux;
} tsl2591Data {false, 0, 0, 0, 0, 0, 0.0};

bool isTSL2591Present() {
  Wire.beginTransmission(TSL2591_ADDR);
  byte error = Wire.endTransmission();
  tslAvailable = (error == 0);
  return tslAvailable;
}

bool initTSL2591() {
    if (tsl.begin()) {
        tslAvailable = true;
        tslRetryCount = 0;
        
        // CORRECTION : On démarre systématiquement en sensibilité MINIMALE (Journée ensoleillée)
        // L'auto-gain se chargera de l'augmenter la nuit.
        tsl.setGain(TSL2591_GAIN_LOW);                 // 1x
        tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // Exposition la plus courte
        
        DEBUG_PRINTLN("TSL2591 initialisé avec succès (Gain LOW, 100ms).");
        return true;
    }
    tslAvailable = false;
    DEBUG_PRINTLN("Erreur: Impossible d'initialiser le TSL2591.");
    return false;
}

void configureSensorTSL2591(tsl2591Gain_t gainSetting, tsl2591IntegrationTime_t timeSetting) {
  if (!tslAvailable) return;
  tsl.setGain(gainSetting);
  tsl.setTiming(timeSetting);
}

bool calibrateTSL2591() {
  if (!tslAvailable) return false;
  
  // S'il fait très sombre, on augmente la sensibilité
  if (tsl2591Data.visible < 100) {
    switch (tsl2591Data.gain) {
      case TSL2591_GAIN_LOW :
        configureSensorTSL2591(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_MED :
        configureSensorTSL2591(TSL2591_GAIN_HIGH, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_HIGH :
        configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_MAX :
        switch (tsl2591Data.timing) {
          case TSL2591_INTEGRATIONTIME_100MS :
            configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_200MS);
            break;
          case TSL2591_INTEGRATIONTIME_200MS :
            configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_300MS);
            break;
          case TSL2591_INTEGRATIONTIME_300MS :
            configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_400MS);
            break;
          case TSL2591_INTEGRATIONTIME_400MS :
            configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_500MS);
            break;
          case TSL2591_INTEGRATIONTIME_500MS :
            configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_600MS);
            break;
          case TSL2591_INTEGRATIONTIME_600MS :
            return false; // On est au maximum absolu
            break;
          default:
            configureSensorTSL2591(TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_600MS);
            break;
        }
        break;
      default:
        configureSensorTSL2591(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
    }
    return true;
  }

  // S'il y a trop de lumière (ou si le capteur est saturé = 65535)
  // CORRECTION : On s'assure d'attraper l'éblouissement total (0xFFFF)
  if (tsl2591Data.visible > 30000 || tsl2591Data.visible == 65535 || tsl2591Data.ir == 65535) {
    switch (tsl2591Data.gain) {
      case TSL2591_GAIN_LOW :
        switch (tsl2591Data.timing) {
          case TSL2591_INTEGRATIONTIME_600MS :
            configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_500MS);
            break;
          case TSL2591_INTEGRATIONTIME_500MS :
            configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_400MS);
            break;
          case TSL2591_INTEGRATIONTIME_400MS :
            configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_300MS);
            break;
          case TSL2591_INTEGRATIONTIME_300MS :
            configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_200MS);
            break;
          case TSL2591_INTEGRATIONTIME_200MS :
            // CORRECTION : On descend bien à 100ms !
            configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_100MS);
            break;
          case TSL2591_INTEGRATIONTIME_100MS :
            return false; // On est au bouclier maximum, on ne peut pas faire plus !
            break;
          default:
            configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_100MS);
            break;
        }
        break;
      case TSL2591_GAIN_MED :
        configureSensorTSL2591(TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_HIGH :
        configureSensorTSL2591(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
      case TSL2591_GAIN_MAX :
        configureSensorTSL2591(TSL2591_GAIN_HIGH, TSL2591_INTEGRATIONTIME_200MS);
        break;
      default:
        configureSensorTSL2591(TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_200MS);
        break;
    }
    return true;
  }
  return false;
}

void updateTSL2591() {
    static int recursionDepth = 0; 

    if (!tslAvailable) {
        unsigned long currentMillis = millis();
        if (tslRetryCount < tslMaxRetries && 
            (currentMillis - tslLastRetryTime >= tslRetryInterval)) {
            
            tslRetryCount++;
            tslLastRetryTime = currentMillis;
            DEBUG_PRINTF("[TSL2591] Tentative de récupération %d/%d\n", tslRetryCount, tslMaxRetries);
            
            if (initTSL2591()) {
                DEBUG_PRINTLN("[TSL2591] Capteur récupéré avec succès!");
                return; 
            } else {
                DEBUG_PRINTF("[TSL2591] Échec de la récupération %d/%d\n", tslRetryCount, tslMaxRetries);
            }
        }
        return; 
    }
  
    if (!tsl.begin()) {
        tslAvailable = false;
        DEBUG_PRINTLN("Erreur: TSL2591 non disponible");
        return;
    }
    
    tsl2591Data.full    = tsl.getFullLuminosity();
    tsl2591Data.ir      = tsl2591Data.full >> 16;
    tsl2591Data.visible = tsl2591Data.full & 0xFFFF;
    tsl2591Data.lux     = tsl.calculateLux(tsl2591Data.visible, tsl2591Data.ir);
    tsl2591Data.gain    = tsl.getGain();
    tsl2591Data.timing  = tsl.getTiming();

    bool changed = calibrateTSL2591();
    
    if (changed) {
        if (recursionDepth < 3) {
            DEBUG_PRINTF("[TSL2591] Gain ajusté, nouvelle mesure (profondeur %d)...\n", recursionDepth);
            recursionDepth++;
            updateTSL2591();
            recursionDepth--; 
        } else {
            DEBUG_PRINTLN("[TSL2591] AVERTISSEMENT : Limite de récursion atteinte.");
        }
    }
}

float getLux() {
  if (!tslAvailable) return -999.0;
  // Securité : si calculateLux() a retourné -1 ou NaN malgré nos réglages minimaux
  lux = (isnan(tsl2591Data.lux) || tsl2591Data.lux < 0) ? 88000.0 : tsl2591Data.lux;
  return lux;
}

float getSQM() {
  if (!tslAvailable) return -999.0;
  if (tsl2591Data.lux <= 0 || isnan(tsl2591Data.lux)) return 0.0; 
  return log10(tsl2591Data.lux/108000.0)/-0.4;
}

#endif
