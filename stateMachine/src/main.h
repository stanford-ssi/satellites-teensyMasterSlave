#include <Wire.h>
#include <ssiSpi.h>
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
#define assert(e) (((e) || !DEBUG) ? (true) : Serial.printf("%s, %d: assertion '%s' failed\n", __FILE__, __LINE__, #e), false)

void clearBuffer(void);
void handlePacket(void);
void packetReceived(void);

#endif
