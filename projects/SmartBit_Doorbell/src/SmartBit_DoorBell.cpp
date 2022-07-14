/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "d:/projects/particle/src/projects/SmartBit_Doorbell/src/SmartBit_DoorBell.ino"
/*
 * Project SmartBit_DoorBell
 * Description:
 * Author:
 * Date:
 */
#include "relay-lib.h"
#include <SmartThingsLib.h>
#include <SoftAPLib.h>
#include <ArduinoJson.h>
#include <HttpClient.h>
#include <clickButton.h>
#include <Base64.h>

// Initialize SoftAP for WiFi management
void watchdogHandler();
void setup();
void loop();
void checkForReboot();
void checkWiFiReady();
void checkButtonBellStatus();
void updateRelayStatus();
void playTTS();
int ringSonoff();
void doNotifyST(void);
void initPulseRelay(bool particlePub, bool ringL, bool ringS, bool playT, bool notifyST);
void notifyStatusToSTHub(String json);
String callbackStatus();
String callbackReboot();
String callbackInfo();
String callbackTest();
void callbackVariableSet(String var);
int signalLvl(String cmd);
int doReboot(String cmd);
int test(String cmd);
int pDebugStatus(String command);
String getStatusJson();
String getInfoJson();
void log(String msg);
void getStringFromEEPROM(int addr, String &value);
void setStringToEEPROM(int addr, String data);
#line 16 "d:/projects/particle/src/projects/SmartBit_Doorbell/src/SmartBit_DoorBell.ino"
STARTUP(softap_set_application_page_handler(SoftAPLib::getPage, nullptr));

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(MANUAL);

// Definitions
const int STRING_BUF_SIZE = 256;

#define BUTTON_BELL_PIN D2
#define RELAY1_PIN D0
#define LED_READY_PIN D1
#define LED_TEST_PIN D7

ApplicationWatchdog *wd;

void watchdogHandler()
{
    System.reset();
}

SmartThingsLib stLib("smartbit-doorbell", "SmartBit Doorbell", "SmartBit", "1.0.12");
ClickButton buttonBell(BUTTON_BELL_PIN, LOW, CLICKBTN_PULLUP);

// For reboot handle

#define DELAY_BEFORE_REBOOT (15 * 1000)

unsigned int rebootDelayMillis = DELAY_BEFORE_REBOOT;
unsigned long rebootSync = millis();
bool resetFlag = false;

// Pre-declare timer functions for timers
//  void initPulseRelay(int delayTime);
//  void endPulseRelay(void);
//  void initTimerNotifySTHub(int delayTime);
//  void doNotifyST(void);
//  void playTTS();

// Timers
//  Timer timerPulseRelay(500, endPulseRelay, true);
//  Timer timerdoNotifyST(1000, doNotifyST, true);
//  Timer timerPlayTTS(2500, playTTS, true);

// Relays
// RelayLib relayBell = RelayLib(RELAY1_PIN, HIGH, 1);

// Relay variables
int relayBellStatus = 0;

// Button results
int function = 0;

// For connected flag
int connected = 0;

// Last time bell ring
unsigned long lastTimeRing = 0;

// **** FOR VLC CALL **** //
String vlcSrvIp = ""; //"192.168.1.61";
int vlcSrvPort = 0;   // 8080
String vlcSrvAuthUser = "";
String vlcSrvAuthPass = ""; //"OmRlc2NvbmNoaXMwMQ==";

// ***** FOR Other bell device *** //
String sonoffIp = ""; //"192.168.1.83";

// Current status...
String bellStatus = "idle";

// 3 is Unknow, from -127 (weak) to -1dB (strong), 1 Wi-Fi chip error and 2 time-out error
int wifiSignalLvl = 3;

int addr_p1 = 0;             // VLC Server IP
int addr_p2 = addr_p1 + 256; // VLC Server Port
int addr_p3 = addr_p2 + 4;   // VLC Server user
int addr_p4 = addr_p3 + 256; // VLC Server password
int addr_p5 = addr_p4 + 256; // Other bell relay server IP

// Json status response
StaticJsonDocument<200> jsonDoc;

// setup() runs once, when the device is first turned on.
void setup()
{
    wd = new ApplicationWatchdog(10000, watchdogHandler, 1536);
    // Put initialization like pinMode and begin functions here.
    Serial.begin(9600);
    // delay(1500); // Allow board to settle

    // Read last state from memory...
    // EEPROM.get(0, vlcSrvIp);
    getStringFromEEPROM(addr_p1, vlcSrvIp);
    EEPROM.get(addr_p2, vlcSrvPort);
    getStringFromEEPROM(addr_p3, vlcSrvAuthUser);
    getStringFromEEPROM(addr_p4, vlcSrvAuthPass);
    getStringFromEEPROM(addr_p5, sonoffIp);

    // For SmartThings configuration and callbacks
    stLib.begin();
    stLib.callbackForAction("status", &callbackStatus);
    stLib.callbackForAction("reboot", &callbackReboot);
    stLib.callbackForAction("info", &callbackInfo);

    stLib.callbackForAction("test", &callbackTest);

    stLib.monitorVariable("vlcSrvIp", vlcSrvIp);
    stLib.monitorVariable("vlcSrvPort", vlcSrvPort);
    stLib.monitorVariable("vlcSrvAuthUser", vlcSrvAuthUser);
    stLib.monitorVariable("vlcSrvAuthPass", vlcSrvAuthPass);
    stLib.monitorVariable("sonoffIp", sonoffIp);

    // A callback that notifies when a variable change using configSet call, applies to all variables register using monitorVariable
    stLib.callbackForVarSet(&callbackVariableSet);

    Particle.function("signalLvl", signalLvl);
    Particle.function("reboot", doReboot);
    Particle.function("test", test);
    Particle.function("debugStatus", pDebugStatus);

    // IMPORTANTE!
    // Es INPUT_PULLUP ya que no recibe voltaje, cuando es INPUT_PULLDOWN es porque va a conectarse a voltaje
    // Setup de PIN para el boton
    pinMode(BUTTON_BELL_PIN, INPUT_PULLUP);
    pinMode(LED_READY_PIN, OUTPUT);
    pinMode(LED_TEST_PIN, OUTPUT);
    pinMode(RELAY1_PIN, OUTPUT);

    // Setup button timers (all in milliseconds / ms)
    // (These are default if not set, but changeable for convenience)
    buttonBell.debounceTime = 20;    // Debounce timer in ms
    buttonBell.multiclickTime = 250; // Time limit for multi clicks
    buttonBell.longClickTime = 1000; // time until "held-down clicks" register

    digitalWrite(RELAY1_PIN, HIGH);
}

// loop() runs over and over again, as quickly as it can execute.
void loop()
{
    checkWiFiReady();
    stLib.process(); // Process possible messages from SmartThings
    // The core of your code will likely live here.
    checkButtonBellStatus();
    updateRelayStatus();
    wifiSignalLvl = WiFi.RSSI();
    delay(50);
}

// **** LOCAL FUNCTIONS **** //

void checkForReboot()
{
    if (resetFlag && (millis() - rebootSync >= rebootDelayMillis))
    {
        // Particle.publish("reset", "by user", 300, PRIVATE);
        System.reset();
    }
}

void checkWiFiReady()
{
    if (WiFi.ready() && connected == 0)
    {
        connected = 1;
        digitalWrite(LED_READY_PIN, HIGH);
    }
    else if (!WiFi.ready() && connected == 1)
    {
        connected = 0;
        digitalWrite(LED_READY_PIN, LOW);
    }
}

void checkButtonBellStatus()
{
    // Update button state
    buttonBell.Update();

    // Save click codes in LEDfunction, as click codes are reset at next Update()
    if (buttonBell.clicks != 0)
        function = buttonBell.clicks;
    if (function != 0)
    {
        relayBellStatus = 1;
    }

    function = 0;
}

void updateRelayStatus()
{
    if (relayBellStatus == 1)
    {
        long timeOut = 60000 * 1; // Minutos en milisegundos
        long diffTime = millis() - lastTimeRing;
        if (lastTimeRing == 0 || diffTime >= timeOut)
        {
            lastTimeRing = millis();
            bellStatus = "ringing";
            doNotifyST();
        }
        initPulseRelay(true, true, true, true, false);
        relayBellStatus = 0;
    }
    else
    {
        long timeOut = 60000 * 1; // Minutos en milisegundos
        long diffTime = millis() - lastTimeRing;
        if (bellStatus == "ringing" && diffTime >= timeOut)
        {
            log("Notify change state to idle");
            bellStatus = "idle";
            doNotifyST();
        }
    }
}

void playTTS()
{
    log("playTTS...");
    if (vlcSrvIp.length() > 0 && vlcSrvPort > 0 && WiFi.ready())
    {
        HttpClient http;

        String credentials = String(vlcSrvAuthUser + ":" + vlcSrvAuthPass);
        // log("VLC credentials " + credentials);
        int inputLen = strlen(credentials);
        char cCredentials[inputLen];
        strcpy(cCredentials, credentials.c_str());
        cCredentials[inputLen] = '\0';
        int encodedLen = base64_enc_len(inputLen);
        char encodedCredentials[encodedLen];
        base64_encode(encodedCredentials, cCredentials, inputLen);
        log("playTTS - VLC Auth " + String(encodedCredentials));

        String authorization = String("Basic " + String(encodedCredentials));
        char cAuthorization[strlen(authorization)];
        strcpy(cAuthorization, authorization.c_str());
        http_header_t headers[] = {
            {"Authorization", cAuthorization},
            {"Accept", "*/*"},
            {NULL, NULL} // NOTE: Always terminate headers with NULL
        };

        http_request_t request;
        http_response_t response;

        IPAddress server = WiFi.resolve(vlcSrvIp);
        request.ip = server;
        request.port = vlcSrvPort;
        request.path = "/requests/status.json?command=in_play&input=/home/pi/bell_ringing.mp3";

        // Get request
        http.get(request, response, headers);

        if (response.status == 200)
        {
            log("playTTS OK!");
        }
        else
        {
            log("playTTS ERROR! " + String(response.status) + " " + String(response.body));
        }
    }
    else
    {
        log("No server IP, port or WiFi device connected to playTTS");
    }
}

int ringSonoff()
{
    log("ringSonoff...");
    if (sonoffIp.length() > 0 && WiFi.ready())
    {
        HttpClient http;

        http_request_t request;
        http_response_t response;

        IPAddress server = WiFi.resolve(sonoffIp);
        request.ip = server;
        request.port = 80;
        request.path = "/cm?cmnd=Power%203";

        // Get request
        http.get(request, response);

        if (response.status == 200)
        {
            log("Sonoff on ok!");
            return 0;
        }
        else
        {
            log("Sonoff ERROR! " + String(response.status) + " " + String(response.body));
            return -1;
        }
    }
    else
    {
        log("No server IP or WiFi device connected for Sonoff");
        return -1;
    }
}

void doNotifyST(void)
{
    notifyStatusToSTHub(getStatusJson());
}

// 1 = Only particle publish
// 2 = Ring local
// 3 = Ring sonoff
// 4 = Play TTS
// 5 = Notify to SmartThings - just for test
// Para el pulso del relay sin usar delay
void initPulseRelay(bool particlePub, bool ringL, bool ringS, bool playT, bool notifyST)
{
    if (notifyST)
    {
        doNotifyST();
    }

    if (particlePub)
    {
        Particle.publish("ringing");
    }

    log("Ringing...");
    // For test
    digitalWrite(LED_TEST_PIN, HIGH);

    if (ringL)
    {
        for (int i = 0; i < 3; i++)
        {
            log("   - Ringing " + String(i));
            digitalWrite(RELAY1_PIN, LOW);
            delay(500);
            digitalWrite(RELAY1_PIN, HIGH);
            delay(250);
        }
    }
    if (ringS)
    {
        ringSonoff();
    }

    // For test
    log("Ring off");
    digitalWrite(LED_TEST_PIN, LOW);

    delay(500);
    if (playT)
    {
        playTTS();
    }
}

// Send to SmartThings  the current device status
void notifyStatusToSTHub(String json)
{
    stLib.notifyHub(json);
}

// SmartThings callbacks
String callbackStatus()
{
    String json = getStatusJson();
    notifyStatusToSTHub(json);
    return json;
}

String callbackReboot()
{
    System.reset();
}

String callbackInfo()
{
    stLib.showInfo();
    log("bellStatus        : " + bellStatus);
    log("WiFi connected to : " + String(WiFi.SSID()));
    log("WiFi SignalLvl    : " + String(wifiSignalLvl));
    log("vlcSrvIp          : " + String(vlcSrvIp));
    log("vlcSrvPort        : " + String(vlcSrvPort));
    log("vlcSrvAuthUser    : " + String(vlcSrvAuthUser));
    log("vlcSrvAuthPass    : " + String(vlcSrvAuthPass));
    log("sonoffIp          : " + String(sonoffIp));

    String json = getInfoJson();
    Particle.publish("status", json);
    return json;
}

String callbackTest()
{
    initPulseRelay(true, true, true, true, true);
    return "ok";
}

void callbackVariableSet(String var)
{
    // When the variable change, do some usefull thing on this device...
    if (var == "vlcSrvIp")
    {
        log("Variable " + var + " changed!, value : " + String(vlcSrvIp));
        setStringToEEPROM(addr_p1, vlcSrvIp);
    }
    else if (var == "vlcSrvPort")
    {
        log("Variable " + var + " changed!, value : " + String(vlcSrvPort));
        EEPROM.put(addr_p2, vlcSrvPort);
    }
    else if (var == "vlcSrvAuthUser")
    {
        log("Variable " + var + " changed!, value : " + String(vlcSrvAuthUser));
        setStringToEEPROM(addr_p3, vlcSrvAuthUser);
    }
    else if (var == "vlcSrvAuthPass")
    {
        log("Variable " + var + " changed!, value : " + String(vlcSrvAuthPass));
        setStringToEEPROM(addr_p4, vlcSrvAuthPass);
    }
    else if (var == "sonoffIp")
    {
        log("Variable " + var + " changed!, value : " + String(sonoffIp));
        setStringToEEPROM(addr_p5, sonoffIp);
    }
}

// Particle functions
int signalLvl(String cmd)
{
    return wifiSignalLvl;
}

int doReboot(String cmd)
{
    resetFlag = true;
    rebootSync = millis();
    return 0;
}

int test(String cmd)
{
    if (cmd.compareTo(""))
    {
        Particle.publish("for_test_use_params", "all -> pp rl rs pt ns");
        return 1;
    }
    else
    {
        bool p1 = false;
        bool p2 = false;
        bool p3 = false;
        bool p4 = false;
        bool p5 = false;

        if (cmd.compareTo("all"))
        {
            p1 = true;
            p2 = true;
            p3 = true;
            p4 = true;
            p5 = true;
        }
        else
        {
            if (cmd.indexOf("pp") > -1)
            {
                p1 = true;
            }
            if (cmd.indexOf("rl") > -1)
            {
                p2 = true;
            }
            if (cmd.indexOf("rs") > -1)
            {
                p3 = true;
            }
            if (cmd.indexOf("pt") > -1)
            {
                p4 = true;
            }
            if (cmd.indexOf("ns") > -1)
            {
                p5 = true;
            }
        }
        initPulseRelay(p1, p2, p3, p4, p5);

        return 0;
    }
}

int pDebugStatus(String command)
{
    callbackInfo();
    return 0;
}

// Local helper functions
// Build json string for status device
String getStatusJson()
{
    String uptime;
    stLib.getUpTime(uptime);

    jsonDoc["signalLvl"] = wifiSignalLvl;
    jsonDoc["action"] = bellStatus.c_str();
    jsonDoc["uptime"] = uptime.c_str();

    char jsonChar[512];
    serializeJson(jsonDoc, jsonChar);
    String jsonResult = String(jsonChar);
    return jsonResult;
}

String getInfoJson()
{
    String uptime;
    stLib.getUpTime(uptime);

    jsonDoc["signalLvl"] = wifiSignalLvl;
    jsonDoc["uptime"] = uptime.c_str();
    jsonDoc["bellStatus"] = bellStatus.c_str();
    jsonDoc["vlcSrvIp"] = vlcSrvIp.c_str();
    jsonDoc["vlcSrvPort"] = vlcSrvPort;
    jsonDoc["vlcSrvAuthUser"] = vlcSrvAuthUser.c_str();
    jsonDoc["vlcSrvAuthPass"] = vlcSrvAuthPass.c_str();
    jsonDoc["sonoffIp"] = sonoffIp.c_str();

    char jsonChar[512];
    serializeJson(jsonDoc, jsonChar);
    String jsonResult = String(jsonChar);
    return jsonResult;
}

void log(String msg)
{
    Serial.println(String("[SmartBit DoorBell] " + msg));
}

void getStringFromEEPROM(int addr, String &value)
{
    char stringBuf[STRING_BUF_SIZE];
    EEPROM.get(addr, stringBuf);
    stringBuf[sizeof(stringBuf) - 1] = 0; // make sure it's null terminated
    // Initialize a String object from the buffer
    value = String(stringBuf);
}

void setStringToEEPROM(int addr, String data)
{
    char value[STRING_BUF_SIZE];
    strcpy(value, data.c_str());
    value[STRING_BUF_SIZE - 1] = '\0'; // lazy way to make sure to have the string terminated
    EEPROM.put(addr, value);
}
