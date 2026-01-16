#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

// --- TYPY DANYCH ---
enum WorkState {
    IDLE,
    INIT_HOMING,
    WAIT_FOR_HOME_FAST,
    HOMING_BACKOFF_FAST, 
    WAIT_FOR_HOME_SLOW,
    HOMING_BACKOFF_FINAL, 
    CHECK_IR_SENSOR,      
    MOVE_TO_OFFSET,
    MOVE_TO_NEXT_HOLE,    
    DRILL_CYCLE_DOWN,     
    DRILL_CYCLE_WAIT,     
    DRILL_CYCLE_UP,       
    RETURN_TO_ZERO,
    COMPLETED,
    DRILL_ERROR,
    WAIT_FOR_RELOAD
};

enum MenuState { 
    STATUS_SCREEN,
    MAIN_MENU,
    PROG_MENU,
    SETTINGS_MENU, 
    RESET_MENU,
    START_CONFIRM,
    WORKING_ANIM,
    HOMING_SCREEN,
    CALIBRATION_MENU,
    CAL_JOG_MODE,
    CAL_TEST_SENSORS,
    EMERGENCY_STOP_SCREEN
};

struct Config {
    float szerokosc;
    float odstep;
    int ilosc;
    int cycleCount;
    bool autoMode;
    int speed;
    int workSpeed; 
    float homeBackoff; 
    int homeSpeed;
    float margin;
    int motorCurrent;

};

// --- ZMIENNE GLOBALNE (ZAPOWIEDZI - EXTERN) ---
// To mówi innym plikom: "Te zmienne istnieją, szukaj ich w pliku głównym"
extern WorkState currentState;
extern MenuState currentMenu;
extern String currentAction;
extern bool motorFinished;
extern Config cfg;
extern Config tempCfg;
bool isHomed = false; // Na starcie maszyny ustawione na false

// --- PINY MASZYNY ---
#define ENCODER_A 32
#define ENCODER_B 33
#define ENCODER_SW 25
#define PIN_CONFIRM 5
#define PIN_BACK 27
#define PIN_S3_EXTEND  2   
#define S_DRILL        34
#define S_IR           14
#define PIN_HOME        0
#define PIN_RS485_RX    16
#define PIN_RS485_TX    17
#define PIN_RS485_RE_DE 4

// --- STAŁE ---
#define ARM_START_DELAY 500
#define ARM_TOTAL_TIME  2000
#define STEPS_PER_MM 100.0
#define HOMING_SPEED 500
#define HOME_BACKOFF_DIST 2.0 
#define DIRECTION_DELAY 10
#define DRILL_TIMEOUT 10000
#define MARGIN 2.0
#define MOTOR_CURRENT 50

#endif