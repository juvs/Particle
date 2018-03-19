#include "relay-lib.h"

//
// The constructor sets up a single relay, specified by the Pin that relay is attached to

//
// The constructor will also properly set the assigned pin to OUTPUT.
//
RelayLib::RelayLib(int _relayPin, int _state, int _invert)
{
  init(_relayPin, _state, _invert);
}


// Constructor for custom digitalWrite function NOTE: will not set pinMode of output pin
// function must be of type `void function(uint16_t, uint8_t)`
RelayLib::RelayLib(dig_write_func_t* _digWrite, int _relayPin, int _state, int _invert) {
  init(_digWrite, _relayPin, _state, _invert);
}

void RelayLib::init(int _relayPin, int _state, int _invert)
{
  digWrite=digitalWrite;
  relayPin=_relayPin;
  invert = _invert;
  pinMode(relayPin, OUTPUT);

  if (_state == LOW) {
    relayState=LOW;
    if (_invert == 1) relayState=HIGH;
    off();
  }
  else {
    relayState=HIGH;
    if (_invert == 1) relayState=LOW;
    on();
  }
}

void RelayLib::init(dig_write_func_t* _digWrite, int _relayPin, int _state, int _invert) {
  digWrite=_digWrite;
  relayPin=_relayPin;
  invert = _invert;

  if (_state == LOW) {
    relayState=LOW;
    if (_invert == 1) relayState=HIGH;
    off();
  }
  else {
    relayState=HIGH;
    if (_invert == 1) relayState=LOW;
    on();
  }
}

// Turns the relay on.
void RelayLib::on()
{
  int value = HIGH;
  if (invert == 1) value = LOW;
  digitalWrite(relayPin, value);
  relayState=value;
}

// Turns the relay off.
void RelayLib::off()
{
  int value = LOW;
  if (invert == 1) value = HIGH;
  digitalWrite(relayPin, value);
  relayState=value;
}

//Toggles the state of the relay
void RelayLib::toggle()
{
  int value = HIGH;
  if (invert == 1) value = LOW;
  if (relayState==value) {
    off();
  } else {
    on();
  }
}

// Pulse relay on, then off.  If relay is on, turn off after delay time
void RelayLib::pulse(int delayTime) {
  on();
  delay(delayTime);
  off();
}

// Returns the state of the relay (LOW/0 or HIGH/1)
int RelayLib::state()
{
  return(relayState);
}

// If the relay is on, returns true, otherwise returns false
bool RelayLib::isRelayOn()
{
  int value = HIGH;
  if (invert == 1) value = LOW;
  if (relayState==value)
    return true;
  else
    return false;
}

// If the relay is off, returns true, otherwise returns false
bool RelayLib::isRelayOff()
{
  int value = LOW;
  if (invert == 1) value = HIGH;
  if (relayState==value)
    return true;
  else
    return false;
}

bool RelayLib::isInverted() {
  if (invert == 1)
    return true;
  else
    return false;
}
