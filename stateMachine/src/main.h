#ifndef MAIN_H
#define MAIN_H

#include <Wire.h>
#include <ssiSpi.h>
#include <string.h>
#include <ChipSelect.h>
#include <tests.h>
#include <DMAChannel.h>

#define DEBUG true

// Define GLOBAL pinouts: power and laser
#define ENABLE_MINUS_7_PIN 53 //-7V enable pin
#define ENABLE_7_PIN 54 //+7V enable pin
#define DRIVER_HV_EN_pin 52 // 160V driver enable pin
#define LASER_EN_PIN  A22 // laser attached to analog output A22
extern volatile bool laser_state; // move laser state up to global access so main can ensure laser is off / record laser state

#if DEBUG
    #define debugPrintf(...) do {if (Serial) { Serial.printf(__VA_ARGS__); Serial.flush(); }} while (0);
    #define debugPrintln(x) do {if (Serial) { Serial.println(x); Serial.flush(); }} while (0);
#else
    #define debugPrintf(...) do {} while (0)
    #define debugPrintln(x) do {} while (0)
#endif

// Inspired by JPL power of ten document
#define assert(e) (((e) || !DEBUG) ? (true) : (assertionError(__FILE__, __LINE__, #e), false))

extern volatile unsigned int timeAlive;
extern volatile unsigned int lastLoopTime;
extern volatile unsigned int bugs;
extern volatile unsigned int errors;
extern volatile unsigned int maxLoopTime;

bool assertionError(const char* file, int line, const char* assertion);
void heartbeat(void);
void heartbeat2(void);
void heartbeat3(void);
void heartbeat4(void);
void clearBuffer(void);
void handlePacket(void);
void packetReceived(void);

#endif
