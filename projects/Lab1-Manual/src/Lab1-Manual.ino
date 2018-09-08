/*
 * Project Lab1-Manual
 * Description:
 * Author:
 * Date:
 */
#include "clickButton.h"
#include "softap_http.h"
#include "SoftAP.h"
#include "WifiManager.h"

STARTUP(softap_set_application_page_handler(SoftAP::getPage, nullptr));

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(MANUAL);

// void checkWifi(void);
void broadcastMsg(void);

// Timer timerCheckWifi(1000, checkWifi);
Timer timerBroadcast(3000, broadcastMsg);

WifiManager *wfm;

// the Button
#define BUTTON_TEST_PIN D0
ClickButton button1(BUTTON_TEST_PIN, LOW, CLICKBTN_PULLUP);

// Button results
int function = 0;
int connected = 0;

// setup() runs once, when the device is first turned on.
void setup() {
    // Put initialization like pinMode and begin functions here.
    Serial.begin(9600);
    Serial.println("Warming up board...");
    pinMode(BUTTON_TEST_PIN, INPUT_PULLUP);
    // Setup button timers (all in milliseconds / ms)
    // (These are default if not set, but changeable for convenience)
    button1.debounceTime   = 20;   // Debounce timer in ms
    button1.multiclickTime = 250;  // Time limit for multi clicks
    button1.longClickTime  = 1000; // time until "held-down clicks" register

    wfm = WifiManager::getInstance();
    wfm->begin();

    //delay(1500); // Allow board to settle
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
    checkButton();
    initBroadcast();
    wfm->manageWifi();
    delay(5);
}

void initBroadcast() {
    if (WiFi.ready() && !timerBroadcast.isActive()) {
        timerBroadcast.start();
    } else if (!WiFi.ready() && timerBroadcast.isActive()) {
        timerBroadcast.stop();
    }
}

void broadcastMsg() {
    Serial.println("Sending msg...");
}

void checkButton() {
    // Serial.println("Checking button state...");
    // Update button state
    button1.Update();

    // Save click codes in LEDfunction, as click codes are reset at next Update()
    if(button1.clicks != 0) function = button1.clicks;
    if(function == 1) Serial.println("SINGLE click");
    if(function == 2) Serial.println("DOUBLE click");
    if(function == 3) Serial.println("TRIPLE click");
    if(function == -1) Serial.println("SINGLE LONG click");
    if(function == -2) Serial.println("DOUBLE LONG click");
    if(function == -3) Serial.println("TRIPLE LONG click");

    function = 0;
}
