#include "SmartThingsLib.h"

/* Useful Constants */
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)

/* Useful Macros for getting elapsed time */
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN)
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define elapsedDays(_time_) ( _time_ / SECS_PER_DAY)

// returns the number of elements in the array
#define SIZE(array) (sizeof(array) / sizeof(*array))

String _persistentUuid;
String _deviceName;
String _deviceClass;
String _version;
String _serialDevice;
String _callbackURLST;
SmartThingsLib _stLib;
int _callbacksCount;
SmartThingsLib::CallbacksMap _callbacks[SMARTTHINGS_LIB_CALLBACKS_COUNT];
unsigned long _uptime = 0;

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

SmartThingsLib::SmartThingsLib(const char *deviceId, const char *deviceName, const char *deviceClass, const char *version) :
    m_deviceId(deviceId),
    m_deviceName(deviceName),
    m_deviceClass(deviceClass),
    m_version(version),
    _udpMulticastAddress(239, 255, 255, 250),
    _webserver(WS_PREFIX, WS_PORT)
{
    _stLib = *this;
}

void SmartThingsLib::prepareIds() {
  _serialDevice = System.deviceID();
  _version = String(m_version);
  _deviceName = String(m_deviceName);
  _deviceClass = String(m_deviceClass);
  _persistentUuid = String(m_deviceName) + "-" + String(m_version) + "-" + _serialDevice;
}

void SmartThingsLib::begin() {

    _uptime = millis();

    prepareIds();

    // start the UDP and the Multicast
    _udp.begin(UDP_LOCAL_PORT);
    _udp.joinMulticast(_udpMulticastAddress);

    /* setup our default command that will be run when the user accesses
    * the root page on the server */
    _webserver.setDefaultCommand(&indexWebCmd);

    /* run the same command if you try to load /index.html, a common
    * default page name */
    _webserver.addCommand("index.html", &indexWebCmd);
    _webserver.addCommand("description.xml", &descriptionWebCmd);
    _webserver.addCommand("subscribe", &subscribeSTWebCmd);
    _webserver.setFailureCommand(&failureWebCmd);

    /* start the webserver */
    _webserver.begin();

    Serial.println("SmartthingsLib ok!");
    Serial.println("- Device Id    = " + String(m_deviceId));
    Serial.println("- Device Name  = " + String(m_deviceName));
    Serial.println("- Device Class = " + String(m_deviceClass));
    Serial.println("- Version      = " + String(m_version));
    Serial.println("WebServer Ready!");
    Serial.println("IP CONFIG:");
    Serial.println("- localIP = " + String(WiFi.localIP()));
    Serial.println("- subnetMask = " + String(WiFi.subnetMask()));
    Serial.println("- gatewayIP = " + String(WiFi.gatewayIP()));
    Serial.println("- dnsServerIP = " + String(WiFi.dnsServerIP()));
    Serial.println("- dhcpServerIP = " + String(WiFi.dhcpServerIP()));
}

void SmartThingsLib::process() {

    char buff[64];
    int len = 64;

    /* process incoming connections one at a time forever */
    _webserver.processConnection(buff, &len);

    checkUDPMsg();
}

void SmartThingsLib::getUpTime(String &uptime) {
    long diff = (millis() - _uptime) / 1000;
    int days = elapsedDays(diff);
    int hours = numberOfHours(diff);
    int minutes = numberOfMinutes(diff);
    int seconds = numberOfSeconds(diff);
    if (days > 0) {
        uptime = String(days) + " days and " + String(hours) + ":" + String(minutes) + ":" + String(seconds);
    } else {
        uptime = String(hours) + ":" + String(minutes) + ":" + String(seconds);
    }
}

void SmartThingsLib::setCallbackFor(const char *action, Callback *callback) {
    if (_callbacksCount < SIZE(_callbacks)){
        _callbacks[_callbacksCount].action = action;
        _callbacks[_callbacksCount++].callback = callback;
        Serial.println("[Callbacks] Added action = " + String(action) + " to callbacks, records = " + String(_callbacksCount));
    }
}

bool SmartThingsLib::dispatchCallback(char *action) {

    Serial.println("[Callbacks] Checking action = " + String(action) + " to dispatch possible callback in " + String(_callbacksCount) + " records...");

    // if there is no URL, i.e. we have a prefix and it's requested without a
    // trailing slash or if the URL is just the slash
    // if the URL is just a slash followed by a question mark
    // we're looking at the default command with GET parameters passed
    if ((action[0] == 0) || ((action[0] == '/') && (action[1] == 0)) || (action[0] == '/') && (action[1] == '?')) {
        Serial.println("[Callbacks] Action = " + String(action) + " malformed.");
        return false;
    }

    // We now know that the URL contains at least one character.  And,
    // if the first character is a slash,  there's more after it.
    if (action[0] == '/') {
        uint8_t i;
        char *qm_loc;
        size_t action_len;
        uint8_t qm_offset;
        // Skip over the leading "/",  because it makes the code more
        // efficient and easier to understand.
        action++;
        // Look for a "?" separating the filename part of the URL from the
        // parameters.  If it's not there, compare to the whole URL.
        qm_loc = strchr(action, '?');
        action_len = (qm_loc == NULL) ? strlen(action) : (qm_loc - action);
        qm_offset = (qm_loc == NULL) ? 0 : 1;
        for (i = 0; i < _callbacksCount; ++i) {
            //Serial.println("[Callbacks] Checking callback, index = " + String(i) + ", action = " + String(callbacks[i].action));
            if ((action_len == strlen(_callbacks[i].action)) && (strncmp(action, _callbacks[i].action, action_len) == 0)) {
                Serial.println("[Callbacks] Found callback for action = " + String(action) + " dispatching...");
                _callbacks[i].callback();
                return true;
            }

        }
    }
    Serial.println("[Callbacks] No callback found for action = " + String(action));
    return false;
}

// **** ST ACTIONS **** //

bool SmartThingsLib::notifyHub(String body) {
    if (_callbackURLST.trim().length() > 0) {
        HttpClient http;
        http_request_t request;
        http_response_t response;
        http_header_t headersST[] = {
            { "content-type" , "application/json"},
            { "Accept" , "*/*"},
            { "access-control-allow-origin" , "*"},
            { "server" , "Webduino/1.9.1"},
            { NULL, NULL } // NOTE: Always terminate headers will NULL
        };

        String serverIP = _callbackURLST;
        String serverPort;
        String serverPath;

        serverIP = serverIP.substring(7); //remove http://

        int indexColon = serverIP.indexOf(":");

        serverPort = serverIP.substring(indexColon + 1, serverIP.indexOf("/", indexColon + 1));
        serverPath = serverIP.substring(serverIP.indexOf("/", indexColon + 1));
        serverIP = serverIP.substring(0, serverIP.indexOf(":"));

        Serial.println("[NotifyHub] Notify to ST, body: " + body + ", server: " + serverIP + ", port: " + serverPort + ", path: " + serverPath);

        IPAddress server = WiFi.resolve(serverIP);
        request.ip = server;
        request.port = serverPort.toInt();
        request.path = serverPath;

        request.body = body;

        // Get request
        http.post(request, response, headersST);

        if (response.status == 200 || response.status == 202) {
            Serial.println("[NotifyHub] Notify to HUB with response OK!");
            return true;
        } else {
            Serial.println("[NotifyHub] Notify ST ERROR! " + String(response.status) + " " + String(response.body));
            return false;
        }
    } else {
        Serial.println("[NotifyHub] No callbackurl for notify to the HUB!");
    }
}

// **** UDP PROCESSING **** //

void SmartThingsLib::checkUDPMsg() {

  // if there’s data available, read a packet
  int packetSize = _udp.parsePacket();

  // Check if data has been received
  if (packetSize) {

    int len = _udp.read(packetBuffer, 255);
    if (len > 0) {
        packetBuffer[len] = 0;
    }

    String request = packetBuffer;

    if (request.indexOf('M-SEARCH * HTTP/1.1') > 0 && request.indexOf('ST: urn:schemas-upnp-org:device:') > 0) {
        if(request.indexOf("urn:schemas-upnp-org:device:" + _deviceClass + ":1") > 0) {
            /*Serial.println("");
            Serial.print("Received packet of size ");
            Serial.println(packetSize);
            Serial.print("From ");
            IPAddress remote = _udp.remoteIP();

            for (int i =0; i < 4; i++) {
                Serial.print(remote[i], DEC);
                if (i < 3) {
                    Serial.print(".");
                }
            }
            Serial.print(", port ");
            Serial.println(_udp.remotePort());
            Serial.println("Request:");
            Serial.println(request);

            Serial.println("Responding to search request ...");*/

            int r = random(4);
            delay(r);
            respondToSearchUdp();
        }
    } else if (request.indexOf("upnp:event") > 0) {
        Serial.println("");
        Serial.print("Received packet of size ");
        Serial.println(packetSize);
        Serial.print("From ");
        IPAddress remote = _udp.remoteIP();

        for (int i =0; i < 4; i++) {
            Serial.print(remote[i], DEC);
            if (i < 3) {
                Serial.print(".");
            }
        }
        Serial.print(", port ");
        Serial.println(_udp.remotePort());
        Serial.println("Request:");
        Serial.println(request);
    }
  }
}

void SmartThingsLib::respondToSearchUdp() {
    /*Serial.println("");
    Serial.print("Sending response to ");
    Serial.println(_udp.remoteIP());
    Serial.print("Port : ");
    Serial.println(_udp.remotePort());*/

    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    String response =
         "HTTP/1.1 200 OK\r\n"
         "CACHE-CONTROL: max-age=86400\r\n"
         //"DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
         "EXT:\r\n"
         "LOCATION: http://" + String(s) + ":80/description.xml\r\n"
         "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
         //"01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
         "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/0.1\r\n"
         "ST: urn:schemas-upnp-org:device:" + _deviceClass + ":1\r\n"
         "USN: uuid:" + _persistentUuid + "::urn:schemas-upnp-org:device:" + _deviceClass + ":1\r\n";
         //"X-User-Agent: redsonic\r\n\r\n";

    _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
    _udp.write(response.c_str());
    _udp.endPacket();

    /*Serial.println("Response sent !");*/
}

// **** WEBSERVER COMMANDS **** //

void SmartThingsLib::indexWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
  /* this line sends the standard "we're all OK" headers back to the
     browser */
  server.httpSuccess();

  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type != WebServer::HEAD) {
    String indexMsg = "<h1>" + _deviceName + "</h1><br/><h2>v" + _version + "</h2>";

    /* this is a special form of print that outputs from PROGMEM */
    server.printP(indexMsg.c_str());
  }
}

void SmartThingsLib::descriptionWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
    Serial.println("[WebServer] Call from description.xml...");
    /* this line sends the standard "we're all OK" headers back to the
        browser */
    server.httpSuccess("text/xml");

  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type != WebServer::HEAD)
  {

    /* this defines some HTML text in read-only memory aka PROGMEM.
     * This is needed to avoid having the string copied to our limited
     * amount of RAM. */

    String description_xml = "<?xml version=\"1.0\"?>"
         "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
             "<device>"
                "<deviceType>urn:schemas-upnp-org:device:" + _deviceClass + ":1</deviceType>"
                "<friendlyName>"+ _deviceName +"</friendlyName>"
                "<manufacturer>Tenser International Inc.</manufacturer>"
                "<modelName>" + _deviceName + "</modelName>"
                "<modelNumber>" + _version + "</modelNumber>"
                "<UDN>uuid:"+ _persistentUuid +"</UDN>"
                "<serialNumber>" + _serialDevice + "</serialNumber>"
                "<binaryState>0</binaryState>"
                "<serviceList>"
                /*  "<service>"
                      "<serviceType>urn:SmartBell:service:basicevent:1</serviceType>"
                      "<serviceId>urn:SmartBell:serviceId:basicevent1</serviceId>"
                      "<controlURL>/upnp/control/basicevent1</controlURL>"
                      "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                      "<SCPDURL>/eventservice.xml</SCPDURL>"
                  "</service>"*/
              "</serviceList>"
              "</device>"
          "</root>"
        "\r\n";

    /* this is a special form of print that outputs from PROGMEM */
    server.printP(description_xml.c_str());

    Serial.println("[WebServer] Sending response from description.xml call...");
  }
}

void SmartThingsLib::subscribeSTWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
    Serial.println("[WebServer] Call from subscribe...");

    /* this line sends the standard "we're all OK" headers back to the
     browser */
    server.httpSuccess();

    /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
    if (type == WebServer::SUBSCRIBE)
    {
        char callback[255];

        server.getHeaderCallback(callback);
        _callbackURLST = String(callback);
        _callbackURLST = _callbackURLST.substring(1); //Removemos el <
        _callbackURLST.remove(_callbackURLST.length() - 1); //Removemos el >

        Serial.println("[WebServer] Callback : " + _callbackURLST);
        /* this defines some HTML text in read-only memory aka PROGMEM.
         * This is needed to avoid having the string copied to our limited
         * amount of RAM. */
        String eventservice_xml = "\r\n";
        /* this is a special form of print that outputs from PROGMEM */
        server.printP(eventservice_xml.c_str());
    }
    Serial.println("[WebServer] Sending response from subscribe call...");
}

void SmartThingsLib::failureWebCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
    if (!_stLib.dispatchCallback(url_tail)) {
        Serial.println("[WebServer] Failure! url not mapped!, type = " + String(type) + ", url = " + String(url_tail) + ", tail_complete = " + String(tail_complete) );
    }
}
