#ifndef IMU_H
#define IMU_H
#include <main.h>
#include <packet.h>

typedef struct pidSample {
    adcSample sample;
    mirrorOutput out;

    pidSample(): sample(), out() {
    }

    pidSample(const volatile pidSample& s) {
        copy(s);
    }

    pidSample(const volatile adcSample& sampleAdc, const volatile mirrorOutput& o) {
        sample.copy(sampleAdc);
        out.copy(o);
    }

    void copy(const volatile pidSample& s) {
        sample = s.sample;
        out = s.out;
    }

    pidSample& operator =(const volatile pidSample& s) {
        /*if (Serial) {
            Serial.printf("Equals operator\n");
        }*/
        copy(s);
        return *this;
    }

    volatile uint16_t getChecksum() {
        return sample.getChecksum() + out.getChecksum();
    }
} pidSample;

#define IMU_NUM_CHANNELS 4
#define IMU_SAMPLE_SIZE (sizeof(pidSample) / 2) // 16-bit
#define IMU_DATA_DUMP_SIZE 30
#define IMU_BUFFER_SIZE 5000 // units are IMU_SAMPLE_SIZE
#define IMU_SAMPLE_FREQUENCY 5000 // Hz
#define IMU_SAMPLE_PERIOD (1000000/IMU_SAMPLE_FREQUENCY) // micros
#define IMU_DATA_READY_PIN 19

// Only variable visible should be packet related
extern volatile bool imuPacketReady;
extern volatile uint16_t* imuDumpPacket;
extern volatile uint16_t imuPacketChecksum;

void imuSetup();
void taskIMU();
void enterIMU();
void leaveIMU();
void imuPacketSent();
void recordPid(const volatile pidSample& s);
void imuHeartbeat();
#endif // IMU
