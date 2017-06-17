#ifndef IMU_H
#define IMU_H
#include <main.h>
#include <packet.h>

#define IMU_NUM_CHANNELS 4
#define IMU_SAMPLE_SIZE ((32 / 16) * IMU_NUM_CHANNELS) // 16-bit
#define IMU_DATA_DUMP_SIZE (100 * IMU_SAMPLE_SIZE)
#define IMU_BUFFER_SIZE (10000 * IMU_SAMPLE_SIZE)
#define IMU_SAMPLE_FREQUENCY 5000 // Hz
#define IMU_SAMPLE_PERIOD (1000000/IMU_SAMPLE_FREQUENCY) // micros
#define IMU_DATA_READY_PIN 19

// Only variable visible should be packet related
extern volatile bool imuPacketReady;
extern volatile uint16_t imuDumpPacket[IMU_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD + 10];
extern volatile uint16_t imuPacketChecksum;

void imuSetup();
void taskIMU();
void enterIMU();
void leaveIMU();
void imuPacketSent();
void recordPid(const volatile adcSample& s, const mirrorOutput& out);
#endif // IMU
