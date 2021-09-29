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

#define TANK_1_PSS_RX D2 // RX must be interrupt enabled (on Photon/Electron D0/A5 are not)
#define TANK_1_PSS_TX D3

ParticleSoftSerial serialTank1(TANK_1_PSS_RX, TANK_1_PSS_TX);

//For reboot handle

#define DELAY_BEFORE_REBOOT (15 * 1000)

unsigned int rebootDelayMillis = DELAY_BEFORE_REBOOT;
unsigned long rebootSync = millis();
bool resetFlag = false;

//For tanks level
int tankDepth1 = 46; // depth in cms
int offsetTank1 = 0; // offset from sensor to start lvl in cms (63)

int tankDepth2 = 46; // depth in cms
int offsetTank2 = 0; // offset from sensor to start lvl in cms (63)

float readingsTank1[NUM_READINGS];
int readIndexTank1 = 0;
float totalTank1 = 0;
float averageTank1 = 0;

float readingsTank2[NUM_READINGS];
int readIndexTank2 = 0;
float totalTank2 = 0;
float averageTank2 = 0;

int tank1Level = 0L;
int tank2Level = 0L;

int presentValues = -1; // to present values...

//3 is Unknow, from -127 (weak) to -1dB (strong), 1 Wi-Fi chip error and 2 time-out error
int wifiSignalLvl = 3;

//For connected flag
int connected = 0;

//EEPROM Memory address
int addr_ep1 = 0;
int addr_ep2 = 10;
int addr_ep3 = addr_ep2 + 10;
int addr_ep4 = addr_ep3 + 10;

//Json status response
StaticJsonDocument<200> jsonDoc;

void setup()
{
    Wire.begin();
    Serial.begin(9600);
    Serial1.begin(9600);

    //Read last state from memory...
    EEPROM.get(addr_ep1, tankDepth1);
    EEPROM.get(addr_ep2, offsetTank1);
    EEPROM.get(addr_ep3, tankDepth2);
    EEPROM.get(addr_ep4, offsetTank2);

    //For SmartThings configuration and callbacks
    stLib.begin();
    stLib.callbackForAction("status", &callbackStatus);
    stLib.callbackForAction("reboot", &callbackReboot);
    stLib.callbackForAction("info", &callbackInfo);

    //Particle functions
    Particle.function("signalLvl", signalLvl);
    Particle.function("reboot", doReboot);

    Particle.function("cTankDepth1", pChangeTankDepth1);
    Particle.function("cOffsetTank1", pChangeOffsetTank1);
    Particle.function("cTankDepth2", pChangeTankDepth2);
    Particle.function("cOffsetTank2", pChangeOffsetTank2);
    Particle.function("debugStatus", pDebugStatus);

    Particle.variable("tank1Level", tank1Level);
    Particle.variable("tank2Level", tank2Level);
    Particle.variable("offsetTank1", offsetTank1);
    Particle.variable("offsetTank2", offsetTank2);
    Particle.variable("tankDepth1", tankDepth1);
    Particle.variable("tankDepth2", tankDepth2);

    pinMode(LED_READY_PIN, OUTPUT);
    digitalWrite(LED_READY_PIN, LOW);

    serialTank1.begin(baud, PROTOCOL);
}

void loop()
{
    checkForReboot();
    checkWiFiReady();
    stLib.process(); //Process possible messages from SmartThings

    readLevelTank(serialTank1, tank1Level, readingsTank1, readIndexTank1, totalTank1, averageTank1, tankDepth1, offsetTank1, "tank1");
    readLevelTankSerial1(tank2Level, readingsTank2, readIndexTank2, totalTank2, averageTank2, tankDepth2, offsetTank2, "tank2");
    hasToPresentValues();

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
        log("Ready!");
        // Particle.publish("version", sbversion);
    }
    else if (!WiFi.ready() && connected == 1)
    {
        connected = 0;
        digitalWrite(LED_READY_PIN, LOW);
    }
}

void caculateTankLevel(float distance, int &tankLevel, float readings[], int &readIndex, float &total, float &average, int tankDepth, int offsetTank, String tankName)
{
    if (distance > 30)
    {
        distance = distance / 10;
        // String tank = tankName + ", distance: ";
        // tank += String(distance);
        // tank += " cms";
        // log(tank);

        if (distance > -1)
        {
            total = total + distance;
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
                    //tankLevel = 0;
                    //Mantain last value dont change...
                }
            }
        }
    }
    else
    {
        log("Reading below the lower limit " + tankName);
    }
}

void readLevelTankSerial1(int &tankLevel, float readings[], int &readIndex, float &total, float &average, int tankDepth, int offsetTank, String tankName)
{
    float distance = -1;
    unsigned char data[4] = {};

    do
    {
        for (int i = 0; i < 4; i++)
        {
            data[i] = Serial1.read();
        }
    } while (Serial1.read() == 0xff);

    Serial1.flush();

    if (data[0] == 0xff)
    {
        int sum;
        sum = (data[0] + data[1] + data[2]) & 0x00FF;
        if (sum == data[3])
        {
            distance = (data[1] << 8) + data[2];
            caculateTankLevel(distance, tankLevel, readings, readIndex, total, average, tankDepth, offsetTank, tankName);
        }
        // else
        //     log("ERROR");
        delay(100);
    }
}

void readLevelTank(ParticleSoftSerial &serialReceiver, int &tankLevel, float readings[], int &readIndex, float &total, float &average, int tankDepth, int offsetTank, String tankName)
{
    float distance = -1;
    unsigned char data[4] = {};

    do
    {
        for (int i = 0; i < 4; i++)
        {
            data[i] = serialReceiver.read();
        }
    } while (serialReceiver.read() == 0xff);

    serialReceiver.flush();
    if (data[0] == 0xff)
    {
        int sum;
        sum = (data[0] + data[1] + data[2]) & 0x00FF;
        if (sum == data[3])
        {
            distance = (data[1] << 8) + data[2];
            // log(tankName + ", read distance " + String(distance));
            caculateTankLevel(distance, tankLevel, readings, readIndex, total, average, tankDepth, offsetTank, tankName);
        }
        // else
        //     log("ERROR");
        delay(100);
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

        String tank = "Tank 1, distance: ";
        tank += String(averageTank1 - offsetTank1);
        tank += " cms, real distance: ";
        tank += String(averageTank1);
        tank += " cms, offset: ";
        tank += String(offsetTank1);
        tank += " cms, Level: ";
        tank += String(tank1Level);
        tank += "%";
        log(tank);

        // Particle.publish("tank1-distance", String(averageTank1));
        // Particle.publish("tank1-level", String(tank1Level));

        tank = "Tank 2, distance: ";
        tank += String(averageTank2 - offsetTank2);
        tank += " cms, real distance: ";
        tank += String(averageTank2);
        tank += " cms, offset: ";
        tank += String(offsetTank1);
        tank += " cms, Level: ";
        tank += String(tank2Level);
        tank += "%";
        log(tank);

        // Particle.publish("tank2-distance", String(averageTank2));
        // Particle.publish("tank2-level", String(tank2Level));
    }
}

int changeTankDepth1(int changeTo)
{
    tankDepth1 = changeTo;
    EEPROM.put(addr_ep1, tankDepth1);
    callbackStatus();
    return tankDepth1;
}

int changeOffsetTank1(int changeTo)
{
    offsetTank1 = changeTo;
    EEPROM.put(addr_ep2, offsetTank1);
    callbackStatus();
    return offsetTank1;
}

int changeTankDepth2(int changeTo)
{
    tankDepth2 = changeTo;
    EEPROM.put(addr_ep3, tankDepth2);
    callbackStatus();
    return tankDepth2;
}

int changeOffsetTank2(int changeTo)
{
    offsetTank2 = changeTo;
    EEPROM.put(addr_ep4, offsetTank2);
    callbackStatus();
    return offsetTank2;
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
    doReboot("");
    return "";
}

String callbackInfo()
{
    stLib.showInfo();
    log("WiFi connected to    : " + String(WiFi.SSID()));
    log("WiFi SignalLvl       : " + String(wifiSignalLvl));
    log("Tank 1 level         : " + String(tank1Level) + "%");
    log("Tank 1 depth config  : " + String(tankDepth1) + "cm");
    log("Tank 1 offset config : " + String(offsetTank1) + "cm");
    log("Tank 2 level         : " + String(tank2Level) + "%");
    log("Tank 2 depth config  : " + String(tankDepth2) + "cm");
    log("Tank 2 offset config : " + String(offsetTank2) + "cm");

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

int pChangeTankDepth1(String command)
{
    return changeTankDepth1(command.toInt());
}

int pChangeOffsetTank1(String command)
{
    return changeOffsetTank1(command.toInt());
}

int pChangeTankDepth2(String command)
{
    return changeTankDepth2(command.toInt());
}

int pChangeOffsetTank2(String command)
{
    return changeOffsetTank2(command.toInt());
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
    jsonDoc["tank1Level"] = tank1Level;
    jsonDoc["tank1Offset"] = offsetTank1;
    jsonDoc["tank1Depth"] = tankDepth1;
    jsonDoc["tank2Level"] = tank2Level;
    jsonDoc["tank2Offset"] = offsetTank2;
    jsonDoc["tank2Depth"] = tankDepth2;
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
