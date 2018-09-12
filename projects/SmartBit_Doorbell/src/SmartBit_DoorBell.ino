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
#include <Base64.h>

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(MANUAL);

//Definitions
#define BUTTON_BELL_PIN D2
#define RELAY1_PIN D0
#define LED_READY_PIN D1
#define LED_TEST_PIN D7

SmartThingsLib stLib("smartbit-doorbell", "SmartBit Doorbell", "SmartBit", "1.0.0");
ClickButton buttonBell(BUTTON_BELL_PIN, LOW, CLICKBTN_PULLUP);

//Pre-declare timer functions for timers
// void initPulseRelay(int delayTime);
// void endPulseRelay(void);
// void initTimerNotifySTHub(int delayTime);
// void doNotifyST(void);
// void playTTS();

//Timers
// Timer timerPulseRelay(500, endPulseRelay, true);
// Timer timerdoNotifyST(1000, doNotifyST, true);
// Timer timerPlayTTS(2500, playTTS, true);

//Relays
//RelayLib relayBell = RelayLib(RELAY1_PIN, HIGH, 1);

//Relay variables
int relayBellStatus = 0;

//Button results
int function = 0;

//For connected flag
int connected = 0;

//Last time bell ring
unsigned long lastTimeRing = 0;

 // **** FOR VLC CALL **** //
String vlcSrvIp = ""; //"192.168.1.61";
int vlcSrvPort = 8080;
String vlcSrvAuthUser = "";
String vlcSrvAuthPass = ""; //"OmRlc2NvbmNoaXMwMQ==";

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
    //delay(1500); // Allow board to settle

    //Read last state from memory...
    EEPROM.get(0, vlcSrvIp);
    EEPROM.get(1, vlcSrvPort);
    EEPROM.get(2, vlcSrvAuthUser);
    EEPROM.get(3, vlcSrvAuthPass);

    //For SmartThings configuration and callbacks
    stLib.begin();
    stLib.callbackForAction("status", &callbackStatus);
    stLib.callbackForAction("reboot", &callbackReboot);
    stLib.callbackForAction("info", &callbackInfo);

    stLib.monitorVariable("vlcSrvIp", vlcSrvIp);
    stLib.monitorVariable("vlcSrvPort", vlcSrvPort);
    stLib.monitorVariable("vlcSrvAuthUser", vlcSrvAuthUser);
    stLib.monitorVariable("vlcSrvAuthPass", vlcSrvAuthPass);

    //A callback that notifies when a variable change using configSet call, applies to all variables register using monitorVariable
    stLib.callbackForVarSet(&callbackVariableSet);


    Particle.function("signalLvl", signalLvl);
    Particle.function("reboot", doReboot);

    //IMPORTANTE!
    //Es INPUT_PULLUP ya que no recibe voltaje, cuando es INPUT_PULLDOWN es porque va a conectarse a voltaje
    //Setup de PIN para el boton
    pinMode(BUTTON_BELL_PIN, INPUT_PULLUP);
    pinMode(LED_READY_PIN, OUTPUT);
    pinMode(LED_TEST_PIN, OUTPUT);
    pinMode(RELAY1_PIN, OUTPUT);

    // Setup button timers (all in milliseconds / ms)
    // (These are default if not set, but changeable for convenience)
    buttonBell.debounceTime   = 20;   // Debounce timer in ms
    buttonBell.multiclickTime = 250;  // Time limit for multi clicks
    buttonBell.longClickTime  = 1000; // time until "held-down clicks" register

    digitalWrite(RELAY1_PIN, HIGH);
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
    checkWiFiReady();
    stLib.process(); //Process possible messages from SmartThings
    // The core of your code will likely live here.
    checkButtonBellStatus();
    updateRelayStatus();
    wifiSignalLvl = WiFi.RSSI();
}

// **** LOCAL FUNCTIONS **** //
void checkWiFiReady() {
    if (WiFi.ready() && connected == 0) {
        connected = 1;
        digitalWrite(LED_READY_PIN, HIGH);
    } else {
        connected = 0;
    }
}

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
    if (relayBellStatus == 1) {
        long timeOut = 60000 * 1; //Minutos en milisegundos
        long diffTime =  millis() - lastTimeRing;
        if (lastTimeRing == 0 ||  diffTime >= timeOut) {
            bool success;
            lastTimeRing = millis();
            bellStatus = "ringing";
            doNotifyST();
        }
        initPulseRelay(2500);
        relayBellStatus = 0;
    } else {
        long timeOut = 60000 * 1; //Minutos en milisegundos
        long diffTime =  millis() - lastTimeRing;
        if (bellStatus == "ringing" && diffTime >= timeOut) {
            log("Notify change state to idle");
            bellStatus = "idle";
            doNotifyST();
        }
    }
}

void playTTS() {
    log("playTTS...");
    if (vlcSrvIp.length() > 0 && vlcSrvPort > 0 && WiFi.ready()) {
        HttpClient http;

        // String credentials = String("Basic " + vlcSrvAuth).c_str();
        // char cCredentials[sizeof(credentials)];
        // strcpy(cCredentials, credentials.c_str());
        String credentials = String(vlcSrvAuthUser + ":" + vlcSrvAuthPass);
        int inputLen = sizeof(credentials);
        char cCredentials[inputLen];
        strcpy(cCredentials, credentials.c_str());
        int encodedLen = base64_enc_len(inputLen);
        char encodedCredentials[encodedLen];
        log("VLC Auth " + String(encodedCredentials));
        base64_encode(encodedCredentials, cCredentials, inputLen);
        http_header_t headers[] = {
             { "Authorization" , encodedCredentials},
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
            log("playTTS OK!");
        } else {
            log("playTTS ERROR! " + String(response.status) + " " + String(response.body));
        }
    } else {
        log("No server IP, port or WiFi device connected to playTTS");
    }
}

void doNotifyST(void) {
    notifyStatusToSTHub(getStatusJson());
}

//Para el pulso del relay sin usar delay
void initPulseRelay(int delayTime) {
    log("Ringing...");
    //For test
    digitalWrite(RELAY1_PIN, LOW);
    digitalWrite(LED_TEST_PIN, HIGH);

    //relayBell.toggle();
    delay(delayTime);

    log("Ring off");
    //relayBell.toggle();
    //For test
    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(LED_TEST_PIN, LOW);

    delay(500);
    playTTS();
}

//Send to SmartThings  the current device status
void notifyStatusToSTHub(String json) {
    stLib.notifyHub(json);
}

 //SmartThings callbacks
String callbackStatus() {
    String json = getStatusJson();
    notifyStatusToSTHub(json);
    return json;
}

String callbackReboot() {
    System.reset();
}

String callbackInfo() {
    stLib.showInfo();
    return "ok";
}

void callbackVariableSet(String var) {
    //When the variable change, do some usefull thing on this device...
    Serial.println("Variable " + var + " changed!");
    if (var == "vlcSrvIp") {
        EEPROM.put(0, vlcSrvIp);
    } else if (var == "vlcSrvPort") {
        EEPROM.put(1, vlcSrvPort);
    } else if (var == "vlcSrvAuthUser") {
        EEPROM.put(2, vlcSrvAuthUser);
    } else if (var == "vlcSrvAuthPass") {
        EEPROM.put(3, vlcSrvAuthPass);
    }
}

//Particle functions
int signalLvl(String cmd) {
    return wifiSignalLvl;
}

int doReboot(String command) {
    System.reset();
}

//Local helper functions
//Build json string for status device
String getStatusJson() {
    String uptime;
    stLib.getUpTime(uptime);

    statusJson["signalLvl"] = wifiSignalLvl;
    statusJson["action"] = bellStatus.c_str();
    statusJson["uptime"] = uptime.c_str();
    char jsonChar[512];
    statusJson.printTo(jsonChar);
    String jsonResult = String(jsonChar);
    return jsonResult;
}

void log(String msg) {
    Serial.println(String("[SmartBit DoorBell] " + msg));
}
