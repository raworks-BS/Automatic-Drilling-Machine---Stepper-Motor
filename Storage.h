#ifndef STORAGE_H
#define STORAGE_H

#include <EEPROM.h>
#include "Globals.h"

bool changed = false;

void saveToFlash() {
    EEPROM.put(0, cfg);
    EEPROM.commit();
    Serial.println("Zapisano do EEPROM.");
}

void loadFromFlash() {
    EEPROM.get(0, cfg);
    if (isnan(cfg.szerokosc) || cfg.ilosc <= 0 || cfg.ilosc > 100) {
        cfg.szerokosc = 100.0;
        cfg.odstep = 20.0;
        cfg.ilosc = 2;
        cfg.cycleCount = 0;
        cfg.autoMode = false;
        cfg.workSpeed = 100;
        changed = true;
        
    
        
    }
    if (isnan(cfg.homeBackoff) || cfg.homeBackoff <= 0 || isnan(cfg.margin) || cfg.margin <= 0) {
        cfg.homeBackoff = 5.0; // domyślne 5mm
        cfg.homeSpeed = 600;   // domyślne 600 kroków/s
        cfg.margin = 2.0;
        cfg.motorCurrent = 50;
        changed = true;
        
    }
    if (changed) saveToFlash();
}


#endif