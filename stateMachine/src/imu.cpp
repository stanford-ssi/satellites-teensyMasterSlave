#include <main.h>
volatile uint8_t imuSamples[IMU_BUFFER_SIZE];
volatile int dataPointer = 0;
volatile bool sampling = false;
void enterIMU() {
    sampling = false;
    dataPointer = 0;
    trx = DmaSpi::Transfer((const unsigned uint8_t *) dmaSrc, DMASIZE, imuSamples);
    DMASPI0.registerTransfer(trx);
}
