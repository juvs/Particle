#ifndef _RELAY_LIB
#define _RELAY_LIB

#include "application.h"

// Based on http://awtfy.com/2011/07/14/a-too-simple-arduino-library-for-handling-the-seeeduino-relay-shield-and-generic-relays/

// Function prototype
typedef void (dig_write_func_t)(uint16_t, uint8_t);

// Class Definition.
class RelayLib
{
public:
  RelayLib() {};  // Empty constructor.  Only for initial setup.
  //RelayLib(int relayPin, int initState=LOW);    // Constructor
  RelayLib(int relayPin, int initState=LOW, int invert=0);    // Constructor
  //RelayLib(dig_write_func_t* digitalWrite, int relayPin, int initState=LOW); // Constructor with custom digitalWrite function
  RelayLib(dig_write_func_t* digitalWrite, int relayPin, int initState=LOW, int invert=0); // Constructor with custom digitalWrite function

  // can be used as initialization after empty constructor, or to reset info for existing relay
  void init(int relayPin, int initState=LOW, int invert=0);
  void init(dig_write_func_t* digitalWrite, int relayPin, int initState=LOW, int invert=0);

  void on();    // Turns relay on
  void off();   // Turns relay off
  void toggle();  // Toggles state of relay between on and off
  void pulse(int delayTime=300);  // pulse relay on, then off after delay (in ms).  if allready on, will turn it off after the given delay time

  int state();    // returns state of relay (LOW/0/off or HIGH/1/on)
  bool isRelayOn();   // Returns TRUE if the relay is on , false otherwise
  bool isRelayOff();  // Returns TRUE if the relay is off, false otherwise

  bool isOn() { return isRelayOn(); }
  bool isOff() { return isRelayOff(); }

  bool isInverted(); //returns is relay is inverted

private:
  int relayState;     // Variables holds on/off state of the relay
  int relayPin;       // Variable holds which Arduino pin connected to relay.
  int invert;         // Variable holds in case the relays works opposite way
  dig_write_func_t* digWrite; // digitalWrite function
};

#endif
