// Example usage for SmartThingsLib library by Juvenal Guzman.

#include "SmartThingsLib.h"

// Initialize objects from the lib device id prefix, device name, device class, version
SmartThingsLib smartThingsLib("mydevice", "My Device", "MyDevice", "0.0.1");

int turnOn = 0;
int myVar = 0;

void setup() {
    Serial.begin(9600);
    delay(1500); // Allow board to settle
    // Call functions on initialized library objects that require hardware
    smartThingsLib.begin();

    //A GET call from ST to this device, normally don't pass parameters
    smartThingsLib.callbackForAction("action", &callbackAction);

    //To change this value from ST call a GET using this url http://{your_ip}/configSet?myVar=1, the value will change to 1
    //can register int, long and String type var
    //you can also retrive the value of this variable on ST using this url http://{your_ip}/configGet?name=myVar
    smartThingsLib.monitorVariable("myVar", myVar);

    //A callback that notifies when a variable change using configSet call, applies to all variables register using monitorVariable
    smartThingsLib.callbackForVarSet(&callbackVariableSet);

    //Shows usefull info
    smartThingsLib.showInfo();

    pinMode(D7, OUTPUT);
}

void loop() {
    // Use the library's initialized objects and functions
    smartThingsLib.process();

    if (System.buttonPushed() > 100) { // press SETUP button for 500 mills
        Serial.println("Sending event to ST hub!");
        String json = "{\"event\":\"monitor_start\"}";
        smartThingsLib.notifyHub(json);
    }
    delay(500);
}

void callbackAction() {
    //Do your stuff...
    Serial.println("Call from /action url, do some!");
    if (turnOn == 0) {
        turnOn = 1;
        digitalWrite(D7, HIGH);
    } else {
        turnOn = 0;
        digitalWrite(D7, LOW);
    }
}

void callbackVariableSet(String param) {
    //When the variable change, do some usefull thing on this device...
    Serial.println("Variable " + param + " changed!");
    if (myVar > 0) {
        digitalWrite(D7, HIGH);
    } else {
        digitalWrite(D7, LOW);
    }
}
