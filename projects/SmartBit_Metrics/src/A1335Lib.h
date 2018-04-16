#include <Wire.h>

const uint8_t start_register = 0x20;
const uint8_t num_registers = 6;
const uint8_t start_register2 = 0x34; // Error Mask registers
const uint8_t num_registers2 = 2;

/**
 * Content of the registers that should be on every chip
 * Starts on ANG register (0x20:21)
 */
const byte expected_registers[][2] = {
  {0b00000000, 0b00000000}, // ANG: Excpect RID & EF to be 0
  {0b10000000, 0b00010001}, // STA: RIDC, Error:0, Status Processing
  {0b10100000, 0b00000000}, // ERR: RIDC, all errors 0
  {0b10110000, 0b00000000}, // XERR: RIDC, all errors 0
  {0b11110000, 0b00000000}, // TSEN: RIDC
  {0b11100000, 0b00000000}, // FIELD: RIDC
  {0b11000000, 0b00000000}, // ERM: RIDC & All Errors enabled
  {0b11010000, 0b00000000}  // XERM: RIDC & All Errors enabled
};

/**
 * 1 marks the bits that belong to the expected values
 * (to check the expected content)
 */
const byte expected_registers_mask[][2] = {
  {0b11000000, 0b00000000}, // ANG: RIDC & EF
  {0b11110001, 0b11111111}, // STA: RIDC, Error & Status
  {0b11111100, 0b11111111}, // ERR: All except Interface & CRC Errors
  {0b11111111, 0b11111111}, // XERR: Check all errors
  {0b11110000, 0b00000000}, // TSEN: Only RIDC (Register IDentifier Code)
  {0b11110000, 0b00000000}, // FIELD: RIDC
  {0b11110100, 0b01111111}, // ERM: RIDC & All Errors except Protocol Erros
  {0b11111111, 0b11111111}  // XERM: RIDC & All Errors
};

bool readMemory(uint8_t deviceaddress, uint8_t eeaddress, byte* rdata, uint8_t num);
bool writeMemory(uint8_t deviceaddress, uint8_t eeaddress, byte* wdata);
bool writeMemoryCheck(uint8_t deviceaddress, uint8_t eeaddress, byte* wdata);

struct A1335State {
  uint8_t address;
  bool isOK;
  float angle; // in deg
  uint8_t angle_flags : 2; // error, new
  uint8_t status_flags : 4;
  uint16_t err_flags : 12;
  uint16_t xerr_flags : 12;
  float temp; // in °C
  float fieldStrength; // in mT

  byte rawData[8][2];
};

const uint8_t FLAGS_STRLEN = 10;

bool readDeviceState(uint8_t deviceaddress, A1335State* state);
bool checkDefaultSettings(A1335State* state);
bool clearStatusRegisters(uint8_t deviceaddress);

void SerialPrintFlags(uint16_t flags, const char meanings[][FLAGS_STRLEN], uint8_t num);
void SerialPrintAlignLeft(String s, uint16_t l);

// Number and list of all devices connected
//const uint8_t all_devices_num_max = 50;
//A1335State all_devices_state[all_devices_num_max];
//uint8_t all_devices_num = 0;

const char ANGLE_FLAGS[][FLAGS_STRLEN] = {
  "NEW",
  "ERR"
};

const char STATUS_FLAGS[][FLAGS_STRLEN] = {
  "ERR",
  "NEW",
  "Soft_Rst",
  "PwON_Rst"
};

const char ERROR_FLAGS[][FLAGS_STRLEN] = {
  "MagLow",
  "MagHigh",
  "UnderVolt",
  "OverVolt",
  "AngleLow",
  "AngleHigh",
  "ProcError",
  "NoRunMode",
  "(CRC_Err)",
  "(INTFErr)",
  "(XOV)",
  "XERR"
};

const char XERROR_FLAGS[][FLAGS_STRLEN] = {
  "SelfTest",
  "MemAddr",
  "Execute",
  "ResetCond",
  "WTD_Timer",
  "WTD_Halt",
  "EEPR_Hard",
  "SRAM_Hard",
  "Temp_Err",
  "AngleWarn",
  "EEPR_Soft",
  "SRAM_Soft"
};

//String commandIn = "";
//bool line = false;
