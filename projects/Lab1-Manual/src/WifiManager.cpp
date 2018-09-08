#include "WifiManager.h"

//Timer timerCheckWifi(1000, checkWifi);

/**
 * Singleton IoT instance
 * Use getInstance() instead of constructor
 */
WifiManager* WifiManager::getInstance()
{
    if(_instance == NULL)
    {
        _instance = new WifiManager();
    }
    return _instance;
}
WifiManager* WifiManager::_instance = NULL;

WifiManager::WifiManager()
{
    _timerCheckWifi = new Timer(1000, &WifiManager::checkWifi, *this);
}

void WifiManager::begin() {
    Serial.println("Connecting wifi...");
    WiFi.connect();
}

void WifiManager::manageWifi() {
    if (WiFi.ready()) {
        if (_connected == 0) {
            Serial.printf("WiFi is ready!, connected to : ");
            Serial.printf(WiFi.SSID());
            Serial.println(", localIP : " + String(WiFi.localIP()));
            _connected = 1;
            //Connecting to particle
            if (!Particle.connected()) {
                Serial.println("Connecting to Particle...");
                Particle.connect();
            }
        }
    } else {
        if (!WiFi.connecting() && _connected == 1) {
            _connected = 0;
            Serial.println("Reconnecting WiFi...");
            WiFi.connect();
        }
        if (!_timerCheckWifi->isActive()) {
            _timerCheckWifi->start();
        }
    }
    if (Particle.connected()) {
      Particle.process();
    }
}

void WifiManager::checkWifi() {
    if (WiFi.ready()) {
        _timerCheckWifi->stop();
    } else {
        Serial.println("Trying connect to WiFi...");
    }
}
