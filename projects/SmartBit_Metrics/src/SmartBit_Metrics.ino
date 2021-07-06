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

//For tanks level
int tankDepth1 = 46; // depth in cms
int offsetTank1 = 0; // offset from sensor to start lvl in cms (63)

float readingsTank1[NUM_READINGS];
int readIndexTank1 = 0;
float totalTank1 = 0;
float averageTank1 = 0;

int tank1Level = 0L;
int tank1Temp = 0;

int presentValues = -1; // to present values...

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
    Serial.begin(57600);
    Serial1.begin(baud, PROTOCOL);

    //Read last state from memory...
    EEPROM.get(addr_ep1, tankDepth1);
    EEPROM.get(addr_ep2, offsetTank1);

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
    Particle.function("debugStatus", pDebugStatus);

    Particle.variable("tank1Level", tank1Level);
    Particle.variable("tank1Temp", tank1Temp);
    Particle.variable("offsetTank1", offsetTank1);
    Particle.variable("tankDepth1", tankDepth1);

    pinMode(LED_READY_PIN, OUTPUT);
    digitalWrite(LED_READY_PIN, LOW);

    pinMode(D5, OUTPUT); //Activamos el trigger del sensor SEN0300
    digitalWrite(D5, HIGH);
}

void loop()
{
    checkForReboot();
    checkWiFiReady();
    stLib.process(); //Process possible messages from SmartThings

    //digitalWrite(D5, HIGH);
    readLevelTank(tank1Level, tank1Temp, readingsTank1, readIndexTank1, totalTank1, averageTank1, tankDepth1, offsetTank1, "tank1");
    hasToPresentValues();

    wifiSignalLvl = WiFi.RSSI();
}

// **** LOCAL FUNCTIONS **** //

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

void readLevelTank(int &tankLevel, int &tankTemp, float readings[], int &readIndex, float &total, float &average, int tankDepth, int offsetTank, String tankName)
{

    char col;
    unsigned char buffer_RTT[6] = {};
    int distance = 0;
    float temp = 0;
    int Tflag = 0;

    //Trigger
    digitalWrite(D5, LOW);
    delay(10);
    digitalWrite(D5, HIGH);
    delay(60);

    do
    {
        for (int j = 0; j <= 5; j++)
        {
            col = Serial1.read();
            buffer_RTT[j] = (char)col;
        }
    } while (Serial1.read() == 0xff);

    Serial1.flush();

    if (buffer_RTT[0] == 0xff)
    {
        int cor;
        cor = (buffer_RTT[0] + buffer_RTT[1] + buffer_RTT[2] + buffer_RTT[3] + buffer_RTT[4]) & 0x00FF; //Check
        // log(String(cor) + " cor");
        // log(String(buffer_RTT[5]) + " buffer_RTT[5]");

        if (buffer_RTT[5] == cor)
        {
            distance = (buffer_RTT[1] << 8) + buffer_RTT[2];
            Tflag = buffer_RTT[3] & 0x80;
            if (Tflag == 0x80)
            {
                buffer_RTT[3] = buffer_RTT[3] ^ 0x80;
            }
            temp = (buffer_RTT[3] << 8) + buffer_RTT[4];
            temp = temp / 10;
            tankTemp = (int)temp;
            caculateTankLevel(distance, tankLevel, readings, readIndex, total, average, tankDepth, offsetTank, tankName);
        }
        else
        {
            distance = 0;
            temp = 0;
        }
        delay(500);
    }
    // log("distance : ");
    // log(String(distance) + " mm"); //Output distance unit mm
    // log("temperature: ");
    // if (Tflag == 0x80)
    // {
    //     log("-");
    // }
    // log(String(temp) + " C"); //Output temperature
    // log("============================== ");
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

        Particle.publish("tank1-distance", String(averageTank1));
        Particle.publish("tank1-level", String(tank1Level));
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
