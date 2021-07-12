#include "A1335Lib.h"

#include <SmartThingsLib.h>
#include <SoftAPLib.h>
#include <ArduinoJson.h>
#include <ParticleSoftSerial.h>

// Initialize SoftAP for WiFi management
STARTUP(softap_set_application_page_handler(SoftAPLib::getPage, nullptr));

SYSTEM_THREAD(ENABLED);

String sbversion = "0.0.10";

ApplicationWatchdog wd(60000, System.reset, 1536);
SmartThingsLib stLib("smartbit-metrics", "SmartBit Metrics", "SmartBit", sbversion);

#define SENSORS 2
#define NUM_READINGS 60  //Is really good this sensor  //120 //--> 1 minute of readings
#define LED_READY_PIN D6 //Led listo

const uint32_t baud = 9600;

#define PROTOCOL SERIAL_8N1
#if (SYSTEM_VERSION >= 0x00060000)
SerialLogHandler logHandler;
#endif

//For reboot handle

#define DELAY_BEFORE_REBOOT (15 * 1000)

unsigned int rebootDelayMillis = DELAY_BEFORE_REBOOT;
unsigned long rebootSync = millis();
bool resetFlag = false;

//For magnetic sensor
int readingsAngle[NUM_READINGS]; // the readings from the
int readIndexAngle = 0;          // the index of the current reading
int totalAngle = 0;              // the running total
int averageAngle = 0;            // the average

int readingsTemp[NUM_READINGS];
int readIndexTemp = 0;
int totalTemp = 0;
int averageTemp = 0;

int presentValues = -1; // to present values...

//3 is Unknow, from -127 (weak) to -1dB (strong), 1 Wi-Fi chip error and 2 time-out error
int wifiSignalLvl = 3;

//For connected flag
int connected = 0;

//EEPROM Memory address
int addr_ep1 = 0;
int addr_ep2 = 10;

//Para el sensor del angulo magnetico del gas
A1335State s;

//Json status response
StaticJsonDocument<200> jsonDoc;

void setup()
{
    Wire.begin();
    Serial.begin(9600);

    //Read last state from memory...
    //    EEPROM.get(addr_ep1, tankDepth1);

    //For SmartThings configuration and callbacks
    stLib.begin();
    stLib.callbackForAction("status", &callbackStatus);
    stLib.callbackForAction("reboot", &callbackReboot);
    stLib.callbackForAction("info", &callbackInfo);

    //Particle functions
    Particle.function("signalLvl", signalLvl);
    Particle.function("reboot", doReboot);

    Particle.function("debugStatus", pDebugStatus);

    Particle.variable("angle", averageAngle);
    Particle.variable("temp", averageTemp);

    pinMode(LED_READY_PIN, OUTPUT);
    digitalWrite(LED_READY_PIN, LOW);

    digitalWrite(D5, HIGH);
}

void loop()
{
    checkForReboot();
    checkWiFiReady();
    stLib.process(); //Process possible messages from SmartThings

    processAngleTempReading();
    hasToPresentValues();
    delay(500);

    wifiSignalLvl = WiFi.RSSI();
}

// **** LOCAL FUNCTIONS **** //

void processAngleTempReading()
{
    if (readDeviceState(0x0C, &s))
    {
        // subtract the last reading:
        totalAngle = totalAngle - readingsAngle[readIndexAngle];
        totalTemp = totalTemp - readingsTemp[readIndexAngle];

        SerialPrintFlags(s.angle_flags, ANGLE_FLAGS, 1);
        Serial.print("\n");
        SerialPrintFlags(s.status_flags, STATUS_FLAGS, 3);
        Serial.print("\n");
        SerialPrintFlags(s.err_flags, ERROR_FLAGS, 11);
        Serial.print("\n");

        log(String(s.isOK) + " isOk");
        //log(String(s.angle_flags) + " af");
        log(String(s.angle) + "°");
        log(String(s.fieldStrength) + " fs");
        log(String(s.temp) + "°C");

        // read from the sensor:
        readingsAngle[readIndexAngle] = round(s.angle);
        readingsTemp[readIndexAngle] = round(s.temp);

        // add the reading to the total:
        totalAngle = totalAngle + readingsAngle[readIndexAngle];
        totalTemp = totalTemp + readingsTemp[readIndexAngle];
        // advance to the next position in the array:
        readIndexAngle = readIndexAngle + 1;
        // calculate the average:
        averageAngle = totalAngle / NUM_READINGS;
        averageTemp = totalTemp / NUM_READINGS;

        // if we're at the end of the array...
        if (readIndexAngle >= NUM_READINGS)
        {
            // ...wrap around to the beginning:
            readIndexAngle = 0;
            presentValues++;
        }

        if (s.status_flags & 0b1000)
        {
            clearStatusRegisters(0x0C);
            Serial.println(F("Cleared Flags because of Reset Condition; Rescanning..."));
        }
    }
}

void checkForReboot()
{
    if (resetFlag && (millis() - rebootSync >= rebootDelayMillis))
    {
        Particle.publish("reset", "by user", 300, PRIVATE);
        System.reset();
    }
}

void checkWiFiReady()
{
    if (WiFi.ready() && connected == 0)
    {
        connected = 1;
        digitalWrite(LED_READY_PIN, HIGH);
        log("Ready!");
        Particle.publish("version", sbversion);
    }
    else if (!WiFi.ready() && connected == 1)
    {
        connected = 0;
        digitalWrite(LED_READY_PIN, LOW);
    }
}

void hasToPresentValues()
{
    if (presentValues == -1)
    {
        log("Taking readings...");
        presentValues = 0;
    }

    if (presentValues >= SENSORS)
    {
        presentValues = -1;

        String angle = "Angle: ";
        angle += String(averageAngle);
        angle += "°";
        angle += ", Temp: ";
        angle += String(averageTemp);
        angle += "°C";

        log(angle);

        Particle.publish("averageAngle", String(averageAngle));
        Particle.publish("averageTemp", String(averageTemp));
    }
}

// int changeTankDepth1(int changeTo)
// {
//     tankDepth1 = changeTo;
//     EEPROM.put(addr_ep1, tankDepth1);
//     callbackStatus();
//     return tankDepth1;
// }

//Send to SmartThings  the current device status
void notifyStatusToSTHub(String json)
{
    stLib.notifyHub(json);
}

//SmartThings callbacks
String callbackStatus()
{
    String json = getStatusJson();
    notifyStatusToSTHub(json);
    return json;
}

String callbackReboot()
{
    doReboot("");
    return "";
}

String callbackInfo()
{
    stLib.showInfo();
    log("WiFi connected to    : " + String(WiFi.SSID()));
    log("WiFi SignalLvl       : " + String(wifiSignalLvl));
    log("Angle                : " + String(averageAngle) + "°");
    log("Temp                 : " + String(averageTemp) + "°C");

    String json = getStatusJson();
    Particle.publish("status", json);
    return "ok";
}

//Particle functions
int signalLvl(String cmd)
{
    return wifiSignalLvl;
}

int doReboot(String command)
{
    resetFlag = true;
    rebootSync = millis();
    return 0;
}

// int pChangeTankDepth1(String command)
// {
//     return changeTankDepth1(command.toInt());
// }

int pDebugStatus(String command)
{
    callbackInfo();
    return 0;
}

//Local helper functions
//Build json string for status device
String getStatusJson()
{
    String uptime;
    stLib.getUpTime(uptime);

    jsonDoc["signalLvl"] = wifiSignalLvl;
    jsonDoc["averageAngle"] = averageAngle;
    jsonDoc["averageTemp"] = averageTemp;
    jsonDoc["uptime"] = uptime.c_str();
    jsonDoc["version"] = sbversion.c_str();
    char jsonChar[512];
    //statusJson.printTo(jsonChar);
    serializeJson(jsonDoc, jsonChar);
    String jsonResult = String(jsonChar);
    return jsonResult;
}

void log(String msg)
{
    Serial.println(String("[SmartBit Metrics] " + msg));
}
