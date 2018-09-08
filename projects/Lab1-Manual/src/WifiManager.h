#pragma once

#include "Particle.h"

class WifiManager {

public:
    static WifiManager* getInstance();
    void manageWifi();
    void begin();

private:
    static WifiManager* _instance;
    Timer *_timerCheckWifi;
    int _connected = 0;

    /**
     * Constructor
     * Private because this is a singleton
     */
    WifiManager();

    void checkWifi();
    void stopTimer();
};
