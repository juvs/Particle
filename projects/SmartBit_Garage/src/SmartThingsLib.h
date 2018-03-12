#ifndef __SMARTTHINGSLIB_H_
#define __SMARTTHINGSLIB_H_

#include <ArduinoJson.h>
#include <WebDuino.h>
#include <HttpClient.h>
#include <string.h>
#include "application.h"
#include <stdlib.h>

// **** WEBSERVER **** //
#define WS_PREFIX ""
#define WS_PORT 80

// **** UDP **** //
#define UDP_TX_PACKET_MAX_SIZE 8192
#define UDP_LOCAL_PORT 1900

#ifndef SMARTTHINGS_LIB_CALLBACKS_COUNT
#define SMARTTHINGS_LIB_CALLBACKS_COUNT 8
#endif

class SmartThingsLib
{
public:

    typedef void Callback();

    struct CallbacksMap
    {
        const char *action;
        Callback *callback;
    };

    SmartThingsLib() {};

    // constructor for
    SmartThingsLib(const char *deviceId, const char *deviceName, const char *deviceClass, const char *version);

    // can be used as initialization after empty constructor
    //void init(const char *deviceName, const char *deviceId);
    void begin();
    void process();
    bool notifyHub(String body);
    void setCallbackFor(const char *action, Callback *callback);

    void getUpTime(String &uptime);

private:

    const char *m_deviceId;
    const char *m_deviceName;
    const char *m_deviceClass;
    const char *m_version;

    WebServer _webserver;
    IPAddress _udpMulticastAddress;
    // An UDP instance to let us send and receive packets over UDP
    UDP _udp;

    void prepareIds();
    void checkUDPMsg();
    void respondToSearchUdp();

    bool dispatchCallback(char *action);

    static void indexWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool);
    static void failureWebCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);
    static void descriptionWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool);
    static void subscribeSTWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool);
};

#endif  //_SMARTTHINGSLIB_H_
