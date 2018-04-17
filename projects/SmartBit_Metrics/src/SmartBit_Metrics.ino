#include "A1335Lib.h"
#include "HC-SR04.h"

#define SENSORS         3
#define NUM_READINGS    120 // 1 minute of readings
#define TANK_1_PIN_TRIG D3
#define TANK_1_PIN_ECO  D2
#define TANK_2_PIN_TRIG D5
#define TANK_2_PIN_ECO  D4
#define READY_LED_PIN   D6 //Led listo

//Para el sensor del angulo magnetico del gas
A1335State s;

// Led ready state
int ledRdyOn = 0;

int const numReadings = NUM_READINGS;

//For magnetic sensor
int readingsAngle[numReadings];       // the readings from the
int readIndexAngle = 0;               // the index of the current reading
int totalAngle = 0;                   // the running total
int averageAngle = 0;                 // the average

int readingsTemp[numReadings];
int readIndexTemp = 0;
int totalTemp = 0;
int averageTemp = 0;

//For tanks level
int tankDepth = 47;                   // depth in cms
int offsetSensor = 66;                // offset from sensor to start lvl in cms

float readingsTank1[numReadings];
int readIndexTank1 = 0;
float totalTank1 = 0;
float averageTank1 = 0;

float readingsTank2[numReadings];
int readIndexTank2 = 0;
float totalTank2 = 0;
float averageTank2 = 0;

HC_SR04 rangeTank1 = HC_SR04(TANK_1_PIN_TRIG, TANK_1_PIN_ECO);
HC_SR04 rangeTank2 = HC_SR04(TANK_2_PIN_TRIG, TANK_2_PIN_ECO);
int tank1Level = 0L;
int tank2Level = 0L;

int calculatingLegend = 1;            // to show legend calculating... once
int presentValues = 0;                 // to present values...

void setup() {
    Wire.begin();

    Serial.begin(9600);
    delay(3000);
    Serial.println("\nSmartBit Metrics Ready!\n");

    pinMode(READY_LED_PIN, OUTPUT);
    digitalWrite(READY_LED_PIN, LOW);

    rangeTank1.init();
}

void loop() {
    ledReadyOnOnce();
    processAngleTempReading();
    processLevelTank(rangeTank1, tank1Level, numReadings, readingsTank1, readIndexTank1, totalTank1, averageTank1, tankDepth, offsetSensor);
    processLevelTank(rangeTank2, tank2Level, numReadings, readingsTank2, readIndexTank2, totalTank2, averageTank2, tankDepth, offsetSensor);
    hasToPresentValues();
    delay(500);
}

void ledReadyOnOnce() {
    if (!ledRdyOn) {
        ledRdyOn = 1;
        digitalWrite(READY_LED_PIN, HIGH);
    }
}

void hasToPresentValues() {
    if (presentValues >= SENSORS) {
        presentValues = 0;

        Serial.print(F("    Angle:  "));
        Serial.print(averageAngle);
        Serial.print(F("°"));
        Serial.print(F(", Temp:  "));
        Serial.print(averageTemp);
        Serial.println(F("°C"));

        Serial.print(F("    Tank 1, distance:  "));
        Serial.print(averageTank1 - offsetSensor);
        Serial.print(F(" cms, Level:  "));
        Serial.print(tank1Level);
        Serial.println(F(" %"));

        Serial.print(F("    Tank 2, distance:  "));
        Serial.print(averageTank2 - offsetSensor);
        Serial.print(F(" cms, Level:  "));
        Serial.print(tank2Level);
        Serial.println(F(" %"));
    }
}

void processAngleTempReading() {
    if(readDeviceState(0x0C, &s)){
        if (calculatingLegend == 1) {
            Serial.println("Calculating data, 1 minute to refresh...");
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
        averageAngle = totalAngle / numReadings;
        averageTemp = totalTemp / numReadings;

        // if we're at the end of the array...
        if (readIndexAngle >= numReadings) {
          // ...wrap around to the beginning:
          readIndexAngle = 0;
          presentValues++;
        }

        if(s.status_flags & 0b1000){
            clearStatusRegisters(0x0C);
            Serial.println(F("Cleared Flags because of Reset Condition; Rescanning..."));
        }
    }
}

void processLevelTank(HC_SR04 &rangeTank, int &tankLevel, const int numReadings, float readings[], int &readIndex, float &total, float &average, int tankDepth, int offsetSensor) {
    float cms = rangeTank.distCM();

    // subtract the last reading:
    total = total - readings[readIndex];
    // read from the sensor:
    readings[readIndex] = cms;
    // add the reading to the total:
    total = total + readings[readIndex];
    // advance to the next position in the array:
    readIndex = readIndex + 1;
    // calculate the average:
    average = total / numReadings;

    // if we're at the end of the array...
    if (readIndex >= numReadings) {
        // ...wrap around to the beginning:
        readIndex = 0;
        presentValues++;

        float currentTankDepth = average - offsetSensor;
        long pct = (currentTankDepth * 100) / tankDepth;
        int pctLvl = round(pct);
        if (pctLvl > 100) {
            pctLvl = 100;
        } else if (pctLvl < 0) {
            pctLvl = 0;
        }
        //Invert the pctLvl
        pctLvl = 100 - pctLvl;
        tankLevel = pctLvl;
    }
}
