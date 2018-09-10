/*
 * Project SmartBit_Garage
 * Description:
 * Author: Juvenal Guzman
 * Date: 15 MAR 2018
 */

// This #include statement was automatically added by the Particle IDE.
#include <SmartThingsLib.h>
#include <ArduinoJson.h>
#include "application.h"
#include "relay-lib.h"
#include <clickButton.h>
//#include <SparkCorePolledTimer.h>

// Definiciones
#define SECS_PER_MIN  (60UL)
#define DOOR_OPEN_TIMEOUT(_mins_) (_mins_ * SECS_PER_MIN)

#define RELAY1_PIN D0 //Apertura/Cierre manual
#define RELAY2_PIN D1 //Power del garage
#define RELAY3_PIN D2 //Cerradura (opcional)

#define MAGNETIC_SWITCH_PIN D3 //Sensor apertura/cierre garage
#define BUTTON_OPEN_CLOSE_PIN D4 //Boton para apertura/cierre
#define BUTTON_OPEN_CLOSE_PIN_NUM 4 //Boton para apertura/cierre
#define READY_LED_PIN D5 //Led listo

ClickButton buttonOpenClose(BUTTON_OPEN_CLOSE_PIN_NUM, LOW, CLICKBTN_PULLUP);
SmartThingsLib stLib("smartbit-garage", "SmartBit Garage", "SmartBit", "1.0.0");

StaticJsonBuffer<200> jsonBufferStatus;
StaticJsonBuffer<200> jsonBufferNotify;
JsonObject& statusJson = jsonBufferStatus.createObject();
JsonObject& notifyJson = jsonBufferNotify.createObject();

 //Relays
RelayLib relayOpenClose = RelayLib(RELAY1_PIN, LOW, 1);
RelayLib relayPower = RelayLib(RELAY2_PIN, LOW, 1);
RelayLib relayLock = RelayLib(RELAY3_PIN, LOW, 1);
RelayLib pulseRelay;

//Relay variables
int requestOpenClose = 0;
// Button results
int function = 0;
// Led ready state
int ledRdy_On = 0;
//Magnetic sensor
// door 1 status
int doorStatus = -1;
//Para el manejo del modo override
int overrideMode = 0;
//Para el manejo del modo lock
int lockMode = -1;
//State when call from ST
int fromSTAction = 0;
//How many seconds the garage door is open
unsigned long openTime = 0;
//How many minutes to report garage still open
int minsOpenTimeout = 1;

void setup() {
    Serial.begin(9600);
    delay(3000); // Allow board to settle

    //Read last state from memory...
    EEPROM.get(0, lockMode);
    EEPROM.get(1, overrideMode);

    //For SmartThings configuration and callbacks
    stLib.begin();
    stLib.callbackForAction("lock", &callbackLock);
    stLib.callbackForAction("unlock", &callbackUnlock);
    stLib.callbackForAction("status", &callbackStatus);
    stLib.callbackForAction("open", &callbackOpenCloseState);
    stLib.callbackForAction("close", &callbackOpenCloseState);
    stLib.callbackForAction("override", &callbackOverride);
    stLib.callbackForAction("reboot", &callbackReboot);

    stLib.monitorVariable("openTimeout", minsOpenTimeout);

    //stLib.callbackForVarSet(&callbackVariableSet);

    Particle.variable("overrideMode", overrideMode);
    Particle.variable("openClose", requestOpenClose);
    Particle.variable("lockMode", lockMode);
    Particle.variable("openTime", openTime);
    Particle.variable("doorStatus", doorStatus);

    Particle.function("open", doOpen);
    Particle.function("override", doOverride);
    Particle.function("lock", doLock);
    Particle.function("unlock", doUnlock);
    Particle.function("reboot", doReboot);

    //IMPORTANTE!
    //Es INPUT_PULLUP ya que no recibe voltaje, cuando es INPUT_PULLDOWN es porque va a conectarse a voltaje
    //Setup de PIN para el boton
    pinMode(BUTTON_OPEN_CLOSE_PIN, INPUT_PULLUP);
    //Particle.publish("button", "Configurado en D4!");

    //Setup de PIN para sensor magnetico
    pinMode(MAGNETIC_SWITCH_PIN, INPUT_PULLUP);
    //Particle.publish("magnetic_switch", "Configurado en D2!");

    pinMode(READY_LED_PIN, OUTPUT);
    digitalWrite(READY_LED_PIN, LOW);

    // Setup button timers (all in milliseconds / ms)
    // (These are default if not set, but changeable for convenience)
    buttonOpenClose.debounceTime   = 20;   // Debounce timer in ms
    buttonOpenClose.multiclickTime = 250;  // Time limit for multi clicks
    buttonOpenClose.longClickTime  = 1000; // time until "held-down clicks" register
}

void loop() {
    stLib.process(); //Process possible messages from SmartThings

    ledReadyOnOnce();
    monitorMagneticSwitch();
    checkOpenDoorTime();
    checkButtonOpenCloseStatus();
    updateRelayStatus();
 }

void checkOpenDoorTime() {
    if (openTime > 0) {
        long diff = (millis() - openTime) / 1000;
        if (minsOpenTimeout != 0 && diff >= DOOR_OPEN_TIMEOUT(minsOpenTimeout)) { //If minsOpenTimeout is zero don't notify
            openTime = millis();
            notifyGarageOpenTimeoutToSTHub();
        }
    }
}

void ledReadyOnOnce() {
    if (!ledRdy_On) {
        ledRdy_On = 1;
        digitalWrite(READY_LED_PIN, HIGH);
    }
}

void updateRelayStatus() {
    if (requestOpenClose == 1) {
        requestOpenClose = 0;
        if (lockMode == 0 || lockMode == -1) { //Solo cuando no estamos en modo lock
            relayOpenClose.pulse(250);
        }
    }

    if (lockMode == 1 && (doorStatus != -1 || doorStatus == 0)) { //Lock only if is closed
        relayLock.on();
        relayPower.on();
    } else if (lockMode == 0)  {
        relayLock.off();
        relayPower.off();
    }
}

void monitorMagneticSwitch() {
    //constantly monitor the door magnetic switch status (garage door open or closed)
    int currentStatus = digitalRead(MAGNETIC_SWITCH_PIN);
    if (currentStatus == HIGH && (doorStatus == -1 || doorStatus == 0)) { //El sensor esta en NO entonces LOW
        doorStatus = 1;
        fromSTAction = 1; //Report to ST this action and put on feed
        notifyStatusToSTHub();
        openTime = millis(); //Starts to count time for check open garage door
    } else if (currentStatus == LOW && (doorStatus == -1 || doorStatus == 1)) {
        doorStatus = 0;
        fromSTAction = 1; //Report to ST this action and put on feed
        notifyStatusToSTHub();
        openTime = 0; //Restart time for open garage door
        if (overrideMode == 1 && (lockMode == 0 || lockMode == -1)) {
            //delay(10);
            relayPower.toggle();
            delay(2000);
            relayPower.toggle();
        }
     }
     delay(50);
}

void checkButtonOpenCloseStatus() {
    // Update button state
    buttonOpenClose.Update();

    // Save click codes in LEDfunction, as click codes are reset at next Update()
    if(buttonOpenClose.clicks != 0) function = buttonOpenClose.clicks;

    if(function == 1)  { //SINGLE click
        requestOpenClose = 1;
    }

    if(function == -1) { //SINGLE LONG click
        changeOverrideMode();
    }

    function = 0;
    delay(5);
}

//Particle functions to call from app Particle
int doOpen(String command) {
    changeOpenCloseState();
}

int doOverride(String command) {
    changeOverrideMode();
}

int doLock(String command) {
    changeLockMode(1);
}

int doUnlock(String command) {
    changeLockMode(0);
}

int doReboot(String command) {
    System.reset();
}

//SmartThings callbacks
String callbackStatus() {
    String status = getStatusJson();
    Serial.println(status);
    return String(status);
}

String callbackLock() {
    changeLockMode(1);
    return String(getStatusJson());
}

String callbackUnlock() {
    changeLockMode(0);
    return String(getStatusJson());
}

String callbackOverride() {
    changeOverrideMode();
    return String(getStatusJson());
}

String callbackOpenCloseState() {
    changeOpenCloseState();
    return "200";
}

String callbackReboot() {
    System.reset();
    return "200";
}

void callbackVariableSet(String param) {
    Serial.println("Variable " + param + " changed!");
}

//Local functions to change state
void changeOpenCloseState() {
    if (lockMode == 0) { //Is unlocked
        requestOpenClose = 1;
        fromSTAction = 1;
    } else if (lockMode == 1) { //Always update state because of openning state on ST
        notifyStatusToSTHub();
    }
}

void changeOverrideMode() {
    if (overrideMode == 0) {
        //TODO Possible blink of led ready to visual notify this mode
        Particle.publish("override", "ON");
        overrideMode = 1;
    } else {
        Particle.publish("override", "OFF");
        overrideMode = 0;
    }
    EEPROM.put(1, overrideMode);
    notifyStatusToSTHub();
}

void changeLockMode(int changeTo) {
    //TODO Possible blink of led ready to visual notify is locked
    if (doorStatus != -1 || doorStatus == 0) { //If door is close you can locked
        lockMode = changeTo;
        EEPROM.put(0, lockMode);
    }
    notifyStatusToSTHub();
}

//Send to SmartThings the current device status
void notifyStatusToSTHub() {
    stLib.notifyHub(String(getStatusJson()));
}

//Notify SmartThings for open garate timeout
void notifyGarageOpenTimeoutToSTHub() {
    notifyJson["alarm"] = "open_garage_timeout";
    char jsonChar[512];
    notifyJson.printTo(jsonChar);
    stLib.notifyHub(String(jsonChar));
}

//Build json string for status device
String getStatusJson() {
    String uptime;
    stLib.getUpTime(uptime);
    statusJson["lockStatus"] = lockMode == -1 ? "unknow" : lockMode == 1 ? "locked" : "unlock";
    statusJson["door"] = doorStatus == -1 ? "unknow" : doorStatus == 0 ? "closed" : "open";
    statusJson["override"] = overrideMode == 0 ? "off" : "on";
    statusJson["uptime"] = uptime.c_str();
    statusJson["fromAction"]  = fromSTAction == 1 ? "true" : "false";
    fromSTAction = 0;
    char jsonChar[512];
    statusJson.printTo(jsonChar);
    String jsonResult = String(jsonChar);
    return jsonResult;
}
