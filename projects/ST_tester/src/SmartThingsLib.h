/*
 Arduino Library for connect with SmartThings Hub using a custom device handler

 License: CC BY-SA 3.0: Creative Commons Share-alike 3.0. Feel free
 to use and abuse this code however you'd like. If you find it useful
 please attribute, and SHARE-ALIKE!

 Created March 2018
 by Juvenal Guzman - hosted on https://github.com/juvs/SmartThingsLib
 */

 #ifndef __SMARTTHINGSLIB_H_
 #define __SMARTTHINGSLIB_H_

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

 #ifndef SMARTTHINGS_LIB_PARAMS_COUNT
 #define SMARTTHINGS_LIB_PARAMS_COUNT 10
 #endif

 class SmartThingsLib
 {
 public:

     typedef String Callback();
     typedef void CallbackVarSet(String param);

     struct CallbacksMap {
         const char *action;
         Callback *callback;
     };

     struct ParamsIntMap {
         const char *name;
         int *value;
     };
     struct ParamsLongMap {
         const char *name;
         long *value;
     };
     struct ParamsStringMap {
         const char *name;
         String *value;
     };

     SmartThingsLib() {}; //Needs for internal use

     // constructor for
     SmartThingsLib(const char *deviceId, const char *deviceName, const char *deviceClass, const char *version);

     // can be used as initialization after empty constructor
     //void init(const char *deviceName, const char *deviceId);
     void begin();
     void process();
     bool notifyHub(String body);
     void callbackForAction(const char *action, Callback *callback);
     void callbackForVarSet(CallbackVarSet *callback);
     void getUpTime(String &uptime);
     void showInfo();

     void monitorVariable(const char *name, int &var);
     void monitorVariable(const char *name, long &var);
     void monitorVariable(const char *name, String &var);

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

     String dispatchCallback(char *action);
     void checkVariable(const char *name);

     static void indexWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool);
     static void failureWebCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);
     static void descriptionWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool);
     static void subscribeSTWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool);
     static void configSetWebCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);
     static void configGetWebCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);

     static const char* buildResponseVariable(char *name, int value);
     static const char* buildResponseVariable(char *name, long value);
     static const char* buildResponseVariable(char *name, String value);
     static const char* buildJsonResponseVariable(char *name, String value, bool success);
 };

 #endif  //_SMARTTHINGSLIB_H_
