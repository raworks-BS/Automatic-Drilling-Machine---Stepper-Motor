#ifndef MACHINELOGIC_H
#define MACHINELOGIC_H

#include "Globals.h"

/* =========================================================
 * KONFIGURACJA STEROWNIKA SMCI
 * ========================================================= */
#define MODE_REL "p1"   
#define MODE_ABS "p2"   

/* =========================================================
 * FUNKCJE POMOCNICZE
 * ========================================================= */
float calculateOffset(const Config &c) {
    float holesLength = (c.ilosc - 1) * c.odstep;
    float offset = (c.szerokosc - holesLength) * 0.5f;
    return (offset < 0.0f) ? 0.0f : offset;
}

// Zmienne pomocnicze procesu
static int currentHole = 0;
static long currentTargetSteps = 0;
static unsigned long stateTimer = 0;
static unsigned long drillTimer = 0;

/* =========================================================
 * KOMUNIKACJA RS485 / SMCI
 * ========================================================= */
void processSerial() {
    static String rxBuffer;
    while (Serial2.available()) {
        char c = Serial2.read();
        if (c == '\r') {
            if (rxBuffer.length()) {
                if (rxBuffer.indexOf('j') >= 0) {
                    motorFinished = true;
                }
            }
            rxBuffer = "";
        } else if (c != '\n') {
            rxBuffer += c;
        }
    }
}

void sendSmciCommand(const String &cmd) {
    motorFinished = false;
    while (Serial2.available()) Serial2.read();
    digitalWrite(PIN_RS485_RE_DE, HIGH);
    delay(2);
    Serial2.print("#1" + cmd + "\r");
    Serial2.flush();
    digitalWrite(PIN_RS485_RE_DE, LOW);
    delay(10);
}

inline void smciHardStop() { sendSmciCommand("S"); }
inline void smciSetZero() { sendSmciCommand("D"); delay(20); sendSmciCommand("D+0"); currentTargetSteps = 0; }

void emergencyStop() {
    smciHardStop(); 
    digitalWrite(PIN_S3_EXTEND, LOW);
    motorFinished = true;
    currentState = IDLE;
    currentMenu = STATUS_SCREEN;
    currentAction = "HALT: E-STOP!";
    delay(50);
    sendSmciCommand("D+0");
}

/* =========================================================
 * LOGIKA MASZYNY
 * ========================================================= */
void startMachineProcess() {
    currentAction = "STARTOWANIE...";
    currentHole = 0; // Reset licznika otworów
    currentState = INIT_HOMING;
}

void updateMachine() {
    processSerial();

    switch (currentState) {

    /* =================== HOMING (Bez zmian) =================== */
    case INIT_HOMING:
        currentAction = "HOMING FAST";
        sendSmciCommand(MODE_REL);
        sendSmciCommand("i20");
        sendSmciCommand("o" + String(cfg.homeSpeed));
        sendSmciCommand("d1");
        sendSmciCommand("s1000000");
        sendSmciCommand("A");
        currentState = WAIT_FOR_HOME_FAST;
        break;

    case WAIT_FOR_HOME_FAST:
        if (digitalRead(PIN_HOME) == LOW) {
            smciHardStop();
            delay(10);
            sendSmciCommand("D+0");
            sendSmciCommand(MODE_REL);
            sendSmciCommand("d0");
            sendSmciCommand("s" + String((long)(cfg.homeBackoff * STEPS_PER_MM)));
            sendSmciCommand("A");
            stateTimer = millis();
            currentState = HOMING_BACKOFF_FAST;
        }
        break;

    case HOMING_BACKOFF_FAST:
        currentAction = "HOMING...";
        if (motorFinished || millis() - stateTimer > 2000) {
            sendSmciCommand(MODE_REL);
            sendSmciCommand("o" + String(cfg.homeSpeed / 3));
            sendSmciCommand("d1");
            sendSmciCommand("s1000000");
            sendSmciCommand("A");
            currentState = WAIT_FOR_HOME_SLOW;
        }
        break;

    case WAIT_FOR_HOME_SLOW:
        if (digitalRead(PIN_HOME) == LOW) {
            smciHardStop();
            delay(10);
            sendSmciCommand("D+0");
            sendSmciCommand("o" + String(cfg.homeSpeed));
            sendSmciCommand(MODE_REL);
            sendSmciCommand("d0");
            sendSmciCommand("s" + String((long)(cfg.homeBackoff * STEPS_PER_MM)));
            sendSmciCommand("A");
            stateTimer = millis();
            currentState = HOMING_BACKOFF_FINAL;
        }
        break;

        case HOMING_BACKOFF_FINAL:
                if (motorFinished || millis() - stateTimer > 2000) {
                    smciSetZero();
                    isHomed = true;
                    delay(50);
                    
                    // SPRAWDZAMY: Jeśli przyszliśmy z trybu JOG/Kalibracji, to wróć do menu
                    if (currentMenu == CAL_JOG_MODE || currentMenu == CALIBRATION_MENU) {
                        currentState = IDLE;       // Zatrzymaj maszynę
                        currentAction = "ZERO SET";
                        // Pozostajemy w obecnym menu (JOG)
                    } 
                    else {
                        // W przeciwnym razie (normalny start pracy) idź do czujnika IR
                        currentState = CHECK_IR_SENSOR;
                    }
                }
                break;

    case CHECK_IR_SENSOR:
        if (digitalRead(S_IR) == HIGH) {
            currentAction = "BRAK CZESCI!";
            currentState = WAIT_FOR_RELOAD;
        } else {
            currentTargetSteps = (long)(cfg.margin * STEPS_PER_MM); // Zaczynamy od zera absolutnego (po homingu)
            currentHole = 1;
            motorFinished = true;   // Odblokuj ruch
            currentState = DRILL_CYCLE_DOWN; // Skaczemy od razu do skoku o odstep
        }
        break;

    /* =================== NOWA ELASTYCZNA LOGIKA PRACY =================== */

    case MOVE_TO_OFFSET:
        
            if (motorFinished) {
    
                float finalOffset = calculateOffset(cfg) * 2.0f; // Twoje 2x Offset
                currentTargetSteps += (long)(finalOffset * STEPS_PER_MM);
                currentAction = "FINISHING...";

                sendSmciCommand(MODE_ABS);
                sendSmciCommand("i" + String(cfg.motorCurrent));
                sendSmciCommand("o" + String(cfg.workSpeed));
                sendSmciCommand("s-" + String(currentTargetSteps));
                sendSmciCommand("A");

                // Po tym ruchu możemy wrócić do bazy lub zakończyć
                currentState = RETURN_TO_ZERO; 
            }
            break;

    case MOVE_TO_NEXT_HOLE:
            if (motorFinished) {
                // Dodajemy odległość między otworami
                currentTargetSteps += (long)(cfg.odstep * STEPS_PER_MM);
                currentAction = "NEXT HOLE...";
                
                sendSmciCommand(MODE_ABS);
                sendSmciCommand("i" + String(cfg.motorCurrent));
                sendSmciCommand("o" + String(cfg.workSpeed));
                sendSmciCommand("s-" + String(currentTargetSteps)); // Minus, bo jedziemy od bazy
                sendSmciCommand("A");

                currentState = DRILL_CYCLE_DOWN;
            }
            break;

    case DRILL_CYCLE_DOWN:
        if (motorFinished) {
            currentAction = "WIERCENIE " + String(currentHole);
            digitalWrite(PIN_S3_EXTEND, HIGH);
            drillTimer = millis();
            currentState = DRILL_CYCLE_WAIT;
        }
        break;

    case DRILL_CYCLE_WAIT:
        if (digitalRead(S_DRILL) == HIGH) {
            digitalWrite(PIN_S3_EXTEND, LOW);
            stateTimer = millis();
            currentState = DRILL_CYCLE_UP;
        } else if (millis() - drillTimer > DRILL_TIMEOUT) {
            currentState = DRILL_ERROR;
        }
        break;

    case DRILL_CYCLE_UP:
        if (millis() - stateTimer > 1000) { 
            if (currentHole < cfg.ilosc) {
                currentHole++;
                currentState = MOVE_TO_NEXT_HOLE; // Kolejny otwór o 'odstep'
            } else {
                currentState = MOVE_TO_OFFSET;    // Wszystkie otwory gotowe -> teraz OFFSET
            }
        }
        break;

    case RETURN_TO_ZERO:
    if (motorFinished){
        currentAction = "MOVING <-- HOME";
        sendSmciCommand(MODE_ABS);
        sendSmciCommand("i20");
        sendSmciCommand("o" + String(cfg.workSpeed * 1.5));
        sendSmciCommand("s0");
        sendSmciCommand("A");
        stateTimer = millis();
        currentState = COMPLETED;
    }
        break;

    case COMPLETED:
        if (motorFinished) {
            cfg.cycleCount++;
            if (cfg.autoMode) {
                currentAction = "AUTO RESTART"; 
                currentState = INIT_HOMING;
            } else {
                currentAction = "GOTOWE";
                currentMenu  = STATUS_SCREEN;
                currentState = IDLE;
            }
        }
        break;

    case DRILL_ERROR:
        currentAction = "ERROR WIERTARKI!";
        digitalWrite(PIN_S3_EXTEND, LOW);
        smciHardStop();
        sendSmciCommand("r0");
        break;

    default:
        break;
    }
}

#endif