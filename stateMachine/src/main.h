#include <Wire.h>
#include <ssiSpi.h>
#include <string.h>
#ifndef MAIN_H
#define MAIN_H

#define DEBUG true

#if DEBUG
    #define debugPrintf(...) Serial.printf(__VA_ARGS__)
    #define debugPrintln(x) Serial.println(x)
#else
    #define debugPrintf(...) do {} while (0)
    #define debugPrintln(x) do {} while (0)
#endif

// Inspired by JPL power of ten document
#define assert(e) (((e) || !DEBUG) ? (true) : assertionError(__FILE__, __LINE__, #e), false)

extern volatile unsigned int timeAlive;
extern volatile unsigned int lastLoopTime;
extern volatile int errors;

bool assertionError(const char* file, int line, const char* assertion);
void heartbeat(void);
void clearBuffer(void);
void handlePacket(void);
void packetReceived(void);

#endif
