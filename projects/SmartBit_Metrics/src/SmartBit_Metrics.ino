#include "A1335Lib.h"
#include "HC-SR04.h"
#include <SmartThingsLib.h>
#include <SoftAPLib.h>
#include <ArduinoJson.h>

// Initialize SoftAP for WiFi management
STARTUP(softap_set_application_page_handler(SoftAPLib::getPage, nullptr));

SYSTEM_THREAD(ENABLED);

ApplicationWatchdog wd(30000, System.reset, 1536);
SmartThingsLib stLib("smartbit-metrics", "SmartBit Metrics", "SmartBit", "0.0.2");

#define SENSORS 3
#define NUM_READINGS 120 // 1 minute of readings
#define TANK_1_PIN_TRIG D3
#define TANK_1_PIN_ECO D2
#define TANK_2_PIN_TRIG D5
#define TANK_2_PIN_ECO D4
#define LED_READY_PIN D6 //Led listo

//Para el sensor del angulo magnetico del gas
A1335State s;

//For magnetic sensor
int readingsAngle[NUM_READINGS]; // the readings from the
int readIndexAngle = 0;          // the index of the current reading
int totalAngle = 0;              // the running total
int averageAngle = 0;            // the average

int readingsTemp[NUM_READINGS];
int readIndexTemp = 0;
int totalTemp = 0;
int averageTemp = 0;

//For tanks level
int tankDepth = 46; // depth in cms
int offsetTank = 0; // offset from sensor to start lvl in cms (63)

float readingsTank1[NUM_READINGS];
int readIndexTank1 = 0;
float totalTank1 = 0;
float averageTank1 = 0;

float readingsTank2[NUM_READINGS];
int readIndexTank2 = 0;
float totalTank2 = 0;
float averageTank2 = 0;

HC_SR04 rangeTank1 = HC_SR04(TANK_1_PIN_TRIG, TANK_1_PIN_ECO);
HC_SR04 rangeTank2 = HC_SR04(TANK_2_PIN_TRIG, TANK_2_PIN_ECO);
int tank1Level = 0L;
int tank2Level = 0L;

int calculatingLegend = 1; // to show legend calculating... once
int presentValues = 0;     // to present values...

//3 is Unknow, from -127 (weak) to -1dB (strong), 1 Wi-Fi chip error and 2 time-out error
int wifiSignalLvl = 3;

//For connected flag
int connected = 0;

//EEPROM Memory address
int addr_ep1 = 0;
int addr_ep2 = 10;

//Json status response
StaticJsonDocument<200> jsonDoc;

void setup()
{
    Wire.begin();
    Serial.begin(9600);

    //Read last state from memory...
    EEPROM.get(addr_ep1, tankDepth);
    EEPROM.get(addr_ep2, offsetTank);

    //For SmartThings configuration and callbacks
    stLib.begin();
    stLib.callbackForAction("status", &callbackStatus);
    stLib.callbackForAction("reboot", &callbackReboot);
    stLib.callbackForAction("info", &callbackInfo);

    //Particle functions
    Particle.function("signalLvl", signalLvl);
    Particle.function("reboot", doReboot);

    Particle.function("cTankDepth", pChangeTankDepth);
    Particle.function("cOffsetTank", pChangeOffsetTank);
    Particle.function("debugStatus", pDebugStatus);

    Particle.variable("tank1Level", tank1Level);
    Particle.variable("tank2Level", tank2Level);
    Particle.variable("offsetTank", offsetTank);
    Particle.variable("tankDepth", tankDepth);
    Particle.variable("angle", averageAngle);
    Particle.variable("temp", averageTemp);

    pinMode(LED_READY_PIN, OUTPUT);
    digitalWrite(LED_READY_PIN, LOW);

    rangeTank1.init();
    rangeTank2.init();
}

void loop()
{
    checkWiFiReady();
    stLib.process(); //Process possible messages from SmartThings
    processAngleTempReading();
    processLevelTank(rangeTank1, tank1Level, readingsTank1, readIndexTank1, totalTank1, averageTank1, "tank1");
    processLevelTank(rangeTank2, tank2Level, readingsTank2, readIndexTank2, totalTank2, averageTank2, "tank2");
    hasToPresentValues();
    wifiSignalLvl = WiFi.RSSI();
    delay(50);
}

// **** LOCAL FUNCTIONS **** //
void checkWiFiReady()
{
    if (WiFi.ready() && connected == 0)
    {
        connected = 1;
        digitalWrite(LED_READY_PIN, HIGH);
        log("Ready!");
    }
    else if (!WiFi.ready() && connected == 1)
    {
        connected = 0;
        digitalWrite(LED_READY_PIN, LOW);
    }
}

void hasToPresentValues()
{
    if (presentValues >= SENSORS)
    {
        presentValues = 0;

        String angle = "Angle : ";
        angle += String(averageAngle);
        angle += "°";
        angle += ", Temp : ";
        angle += String(averageTemp);
        angle += "°C";
        log(angle);

        String tank = "Tank 1, distance: ";
        tank += String(averageTank1 - offsetTank);
        tank += " cms, real distance: ";
        tank += String(averageTank1);
        tank += " cms, offset: ";
        tank += String(offsetTank);
        tank += " cms, Level: ";
        tank += String(tank1Level);
        tank += "%";
        log(tank);

        tank = "Tank 2, distance: ";
        tank += String(averageTank2 - offsetTank);
        tank += " cms, real distance: ";
        tank += String(averageTank2);
        tank += " cms, offset: ";
        tank += String(offsetTank);
        tank += " cms, Level: ";
        tank += String(tank2Level);
        tank += "%";
        log(tank);
    }
}

//For local processing...
void processAngleTempReading()
{
    if (readDeviceState(0x0C, &s))
    {
        if (calculatingLegend == 1)
        {
            log("[Angle] Calculating data, 1 minute to refresh...");
            calculatingLegend = 0;
        }
        // subtract the last reading:
        totalAngle = totalAngle - readingsAngle[readIndexAngle];
        totalTemp = totalTemp - readingsTemp[readIndexAngle];

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
            log("[Angle] Cleared Flags because of Reset Condition; Rescanning...");
        }
    }
}

void processLevelTank(HC_SR04 &rangeTank, int &tankLevel, float readings[], int &readIndex, float &total, float &average, String tankName)
{
    float cms = rangeTank.distCM();

    // subtract the last reading:
    //total = total - readings[readIndex];
    // read from the sensor:
    //readings[readIndex] = cms;
    // add the reading to the total:
    total = total + cms;
    // advance to the next position in the array:
    readIndex = readIndex + 1;
    // calculate the average:
    average = total / NUM_READINGS;

    // if we're at the end of the array...
    if (readIndex >= NUM_READINGS)
    {
        // ...wrap around to the beginning:
        readIndex = 0;
        total = 0;
        presentValues++;

        Particle.publish(tankName + "-distance", String(average));
        float currentTankDepth = average - offsetTank;
        if (currentTankDepth > -1)
        {
            long pct = (currentTankDepth * 100) / tankDepth;
            int pctLvl = pct + 0.5; //round(pct);
            if (pctLvl > 100)
            {
                pctLvl = 100;
            }
            else if (pctLvl < 0)
            {
                pctLvl = 0;
            }
            //Invert the pctLvl
            pctLvl = 100 - pctLvl;
            tankLevel = pctLvl;
        }
        else
        {
            tankLevel = 0;
        }
    }
}

int changeTankDepth(int changeTo)
{
    tankDepth = changeTo;
    EEPROM.put(addr_ep1, tankDepth);
    callbackStatus();
    return tankDepth;
}

int changeOffsetTank(int changeTo)
{
    offsetTank = changeTo;
    EEPROM.put(addr_ep2, offsetTank);
    callbackStatus();
    return offsetTank;
}

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
    System.reset();
    return "";
}

String callbackInfo()
{
    stLib.showInfo();
    log("WiFi connected to  : " + String(WiFi.SSID()));
    log("WiFi SignalLvl     : " + String(wifiSignalLvl));
    log("Angle 1            : " + String(averageAngle) + "°");
    log("Angle 1 - Temp 1   : " + String(averageTemp) + "°C");
    log("Tank 1 level       : " + String(tank1Level) + "%");
    log("Tank 2 level       : " + String(tank2Level) + "%");
    log("Tank depth config  : " + String(tankDepth) + "cm");
    log("Tank offset config : " + String(offsetTank) + "cm");
    return "ok";
}

//Particle functions
int signalLvl(String cmd)
{
    return wifiSignalLvl;
}

int doReboot(String command)
{
    System.reset();
    return 0;
}

int pChangeTankDepth(String command)
{
    return changeTankDepth(command.toInt());
}

int pChangeOffsetTank(String command)
{
    return changeOffsetTank(command.toInt());
}

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
    jsonDoc["angle1"] = averageAngle;
    jsonDoc["angle1Temp"] = averageTemp;
    jsonDoc["tank1lvl"] = tank1Level;
    jsonDoc["tank2lvl"] = tank2Level;
    jsonDoc["offsetTank"] = offsetTank;
    jsonDoc["tankDepth"] = tankDepth;
    jsonDoc["uptime"] = uptime.c_str();
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
