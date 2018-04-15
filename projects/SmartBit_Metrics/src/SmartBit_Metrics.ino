#include "A1335Lib.h"

A1335State s;

void setup()
{
  Wire.begin();

  Serial.begin(9600);
  delay(5000);
  Serial.println("\nI2C Scanner");
}

int reading = 0;
float angle;
int state = LOW;

void loop() {

    if(readDeviceState(0x0C, &s)){

        //Serial.print(F("==== Sensor: "));
        //Serial.print(s.address);
        //Serial.print(F(" ==== ("));
        //Serial.print(s.isOK ? "OK" : "NOT OK");
        //Serial.println(F(") ===="));

        Serial.print(F("    Angle:  "));
        Serial.print(s.angle);
        Serial.print(F("° ("));
        SerialPrintFlags(s.angle_flags, ANGLE_FLAGS, 2);
        Serial.println(F(")"));

        /*Serial.print(F("   Status:  "));
        SerialPrintFlags(s.status_flags, STATUS_FLAGS, 4);
        Serial.println();

        Serial.print(F("   Errors:  "));
        SerialPrintFlags(s.err_flags, ERROR_FLAGS, 12);
        Serial.println();

        Serial.print(F("  XErrors:  "));
        SerialPrintFlags(s.xerr_flags, XERROR_FLAGS, 12);
        Serial.println();

        Serial.print(F("     Temp:  "));
        Serial.print(s.temp);
        Serial.println(F("°C"));

        Serial.print(F("    Field:  "));
        Serial.print(s.fieldStrength);
        Serial.println(F("mT"));*/


        /*Serial.println();
        Serial.println(F("Raw Register Values:"));
        char buf[30];
        for(uint8_t i=0; i < 8; i++){
          // Register numbers
          uint16_t reg = i < num_registers
                  ? start_register +(i<<1)
                  : start_register2 +(i-num_registers)<<1; // (second half)
          sprintf(buf, "0x%02x: %02x %02x", reg, s.rawData[i][0], s.rawData[i][1]);
          Serial.println(buf);
        }*/

        if(s.status_flags & 0b1000){
            clearStatusRegisters(0x0C);
            Serial.println(F("Cleared Flags because of Reset Condition; Rescanning..."));
        }

        //Serial.println();
        //Serial.println();

    }

    delay(500);
}
