#ifndef IMU_H
#define IMU_H
#include <main.h>
#include <packet.h>

typedef struct pidSample {
    adcSample sample;
    adcSample incoherentOutput;
    mirrorOutput out;

    pidSample(): sample(), incoherentOutput(), out() {
    }

    pidSample(const volatile pidSample& s) {
        copy(s);
    }

    pidSample(const volatile adcSample& sampleAdc, const volatile adcSample& incoherent, const volatile mirrorOutput& o) {
        sample.copy(sampleAdc);
        incoherentOutput.copy(incoherent);
        out.copy(o);
    }

    void copy(const volatile pidSample& s) {
        sample = s.sample;
        incoherentOutput = s.incoherentOutput;
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

// Twice the size
typedef struct expandedPidSample {
    pidSample a;
    pidSample b;
} expandedPidSample;

void writeExpandedPidSampleWithChecksum(const pidSample* in, expandedPidSample* out, volatile uint16_t& pidBufferChecksum);

#define IMU_NUM_CHANNELS 4
#define IMU_SAMPLE_SIZE (sizeof(pidSample) / 2) // 16-bit
#define IMU_DATA_DUMP_SIZE 20
#define IMU_DATA_DUMP_SIZE_UINT16 (IMU_DATA_DUMP_SIZE * sizeof(pidSample) * 8 / 16)
#define IMU_BUFFER_SIZE 3000 // units are IMU_SAMPLE_SIZE
// #define IMU_SAMPLE_FREQUENCY 5000 // Hz
// #define IMU_SAMPLE_PERIOD (1000000/IMU_SAMPLE_FREQUENCY) // micros
#define IMU_DATA_READY_PIN 19

// Only variable visible should be packet related
extern volatile bool imuPacketReady;
extern volatile uint32_t* imuDumpPacket;
extern volatile uint16_t imuPacketChecksum;

extern volatile unsigned int imuDataPointer, imuSentDataPointer, imuSamplesSent;

void imuSetup();
void taskIMU();
void enterIMU();
void leaveIMU();
void imuPacketSent();
void recordPid(const volatile pidSample& s);
void imuHeartbeat();
#endif // IMU
