#ifndef IMU_H
#define IMU_H
#include <main.h>

#define IMU_SAMPLE_SIZE (16 / 8) * 3
#define DATA_DUMP_SIZE 100 * imuSampleSize
#define IMU_BUFFER_SIZE 60000
extern volatile uint8_t imuSamples[IMU_BUFFER_SIZE];
#endif // IMU
