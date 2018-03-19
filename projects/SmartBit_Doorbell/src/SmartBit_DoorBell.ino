/*
 * Project SmartBit_DoorBell
 * Description:
 * Author:
 * Date:
 */
#include "application.h"
#include "relay-lib.h"
#include <SmartThingsLib.h>
#include <ArduinoJson.h>
#include <HttpClient.h>
#include <clickButton.h>

//Definitions
#define BUTTON_BELL_PIN D0
#define RELAY1_PIN D1
#define LED_READY_PIN D2

SmartThingsLib stLib("smartbit-doorbell", "SmartBit Doorbell", "SmartBit", "1.0.0");
ClickButton buttonBell(BUTTON_BELL_PIN, LOW, CLICKBTN_PULLUP);

//Pre-declare timer functions for timers
void initPulseRelay(int delayTime);
void endPulseRelay(void);
void initTimerNotifySTHub(int delayTime);
void doNotifyST(void);
void playTTS();

//Timers
Timer timerPulseRelay(500, endPulseRelay, true);
Timer timerdoNotifyST(1000, doNotifyST, true);
Timer timerPlayTTS(2500, playTTS, true);

//Relays
RelayLib relayBell = RelayLib(RELAY1_PIN, LOW, 0);

//Relay variables
int relayBellStatus = 0;

//Button results
int function = 0;

//Last time bell ring
unsigned long lastTimeRing = 0;

 // **** FOR VLC CALL **** //
String vlcSrvIp = "192.168.1.61";
int vlcSrvPort = 8080;
String vlcSrvAuth = "OmRlc2NvbmNoaXMwMQ==";

//Json status response
StaticJsonBuffer<200> jsonBufferStatus;
JsonObject& statusJson = jsonBufferStatus.createObject();

//Current status...
String bellStatus = "idle";

//3 is Unknow, from -127 (weak) to -1dB (strong), 1 Wi-Fi chip error and 2 time-out error
int wifiSignalLvl = 3;

// setup() runs once, when the device is first turned on.
void setup() {
    // Put initialization like pinMode and begin functions here.
    Serial.begin(9600);
    delay(1500); // Allow board to settle

    //For SmartThings configuration and callbacks
    stLib.begin();
    stLib.callbackForAction("status", &callbackStatus);
    stLib.callbackForAction("reboot", &callbackReboot);

    stLib.monitorVariable("vlcSrvIp", vlcSrvIp);
    stLib.monitorVariable("vlcSrvPort", vlcSrvPort);
    stLib.monitorVariable("vlcSrvAuth", vlcSrvAuth);

    Particle.function("signalLvl", signalLvl);
    Particle.function("reboot", doReboot);

    //IMPORTANTE!
    //Es INPUT_PULLUP ya que no recibe voltaje, cuando es INPUT_PULLDOWN es porque va a conectarse a voltaje
    //Setup de PIN para el boton
    pinMode(BUTTON_BELL_PIN, INPUT_PULLUP);
    pinMode(LED_READY_PIN, OUTPUT);

    // Setup button timers (all in milliseconds / ms)
    // (These are default if not set, but changeable for convenience)
    buttonBell.debounceTime   = 20;   // Debounce timer in ms
    buttonBell.multiclickTime = 250;  // Time limit for multi clicks
    buttonBell.longClickTime  = 1000; // time until "held-down clicks" register

    digitalWrite(LED_READY_PIN, HIGH);
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
    stLib.process(); //Process possible messages from SmartThings
    // The core of your code will likely live here.
    checkButtonBellStatus();
    updateRelayStatus();
    wifiSignalLvl = WiFi.RSSI();
}

// **** LOCAL FUNCTIONS **** //
void checkButtonBellStatus() {
    // Update button state
    buttonBell.Update();

    // Save click codes in LEDfunction, as click codes are reset at next Update()
    if(buttonBell.clicks != 0) function = buttonBell.clicks;
    if(function != 0)  {
        relayBellStatus = 1;
    }

    function = 0;
}

void updateRelayStatus() {
    if (relayBellStatus == 1 && !timerPulseRelay.isActive()) {
        relayBellStatus = 0;
        initPulseRelay(2500);
        initPlayTTSTimer(3000); //Al finalizar el sonido de la campana

        long timeOut = 60000 * 1; //Minutos en milisegundos
        long diffTime =  millis() - lastTimeRing;
        if (lastTimeRing == 0 ||  diffTime >= timeOut) {
            bool success;
            lastTimeRing = millis();
            bellStatus = "ringing";
            initTimerNotifySTHub(1500);
        }
    }
}

void initPlayTTSTimer(int delayTime) {
    timerPlayTTS.changePeriod(delayTime);
    timerPlayTTS.start();
}

void playTTS() {
    timerPlayTTS.stop();
    if (vlcSrvIp.length() > 0 && vlcSrvPort > 0) {
        HttpClient http;
         // Headers currently need to be set at init, useful for API keys etc.
        http_header_t headers[] = {
             { "Authorization" , "Basic " + vlcSrvAuth},
             { "Accept" , "*/*"},
             { NULL, NULL } // NOTE: Always terminate headers with NULL
        };

        http_request_t request;
        http_response_t response;

        IPAddress server = WiFi.resolve(vlcSrvIp);
        request.ip = server;
        request.port = vlcSrvPort;
        request.path = "/requests/status.json?command=in_play&input=/home/pi/bell_ringing.mp3";

        // Get request
        http.get(request, response, headers);

        if (response.status == 200) {
            Serial.println("playTTS OK!");
        } else {
            Serial.println("playTTS ERROR! " + String(response.status) + " " + String(response.body));
        }
    }
}

void initTimerNotifySTHub(int delayTime) {
    timerdoNotifyST.changePeriod(delayTime);
    timerdoNotifyST.start();
}

void doNotifyST(void) {
    timerdoNotifyST.stop();
    notifyStatusToSTHub();
    if (bellStatus == "ringing") {
        bellStatus = "idle";
        initTimerNotifySTHub(60000 * 1); //Despues de un minuto notificamos inactivo
    }
}

//Para el pulso del relay sin usar delay
void initPulseRelay(int delayTime) {
    relayBell.toggle();
    timerPulseRelay.changePeriod(delayTime);
    timerPulseRelay.start();
}

void endPulseRelay(void) {
    timerPulseRelay.stop();
    relayBell.toggle();
}

//Send to SmartThings  the current device status
 void notifyStatusToSTHub() {
     String uptime;
     stLib.getUpTime(uptime);

     statusJson["signalLvl"] = wifiSignalLvl;
     statusJson["action"] = bellStatus.c_str();
     statusJson["uptime"] = uptime.c_str();
     char jsonChar[512];
     statusJson.printTo(jsonChar);
     stLib.notifyHub(String(jsonChar));
 }

 //SmartThings callbacks
void callbackStatus() {
    notifyStatusToSTHub();
}

void callbackReboot() {
    System.reset();
}

//Particle functions
int signalLvl(String cmd) {
    return wifiSignalLvl;
}

int doReboot(String command) {
    System.reset();
}
