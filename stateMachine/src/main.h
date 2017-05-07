#include <Wire.h>
#include <ssiSpi.h>
#ifndef MAIN_H
#define MAIN_H

#define DEBUG true

void clearBuffer(void);
void handlePacket(void);
void packetReceived(void);

#endif
