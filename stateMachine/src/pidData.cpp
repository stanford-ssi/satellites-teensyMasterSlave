#include <main.h>
#include <pidData.h>

// Imu code includes a lot of buffers moving around
// Currently not circular
// Very bug prone; needs more testing
// Maybe add timestamps?

// Imu samples go here
// These are processed on the same main thread so hopefully no race conditions
// Indexes into imuSamples; Marks index of next data point to transmit to audacy
// If sentDataPointer == dataPointer, buffer is empty
// If dataPointer == sentDataPointer - IMU_SAMPLE_SIZE, buffer is full
volatile unsigned int imuSentDataPointer = 0;
volatile unsigned int imuDataPointer = 0; // Marks index of next place to read into
volatile unsigned int imuSamplesSent = 0;
volatile pidSample imuSamples[IMU_BUFFER_SIZE + 10]; // Add some extra space on the end in case we overflow
volatile bool sampling = false;
elapsedMicros timeSinceLastRead;

// This packet ships out directly to audacy
// This is more than we need; OUT_PACKET_OVERHEAD is measured in units of uint16_t, not pidSample
volatile expandedPidSample imuDumpPacketMemory[IMU_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD + 10];
// We skip the first few uint16 because we want the packet body to begin at imuDumpPacketMemory[1], which has a good byte offset mod 32bit.
// Assume header size is less than IMU_SAMPLE_SIZE
volatile uint32_t *imuDumpPacket = ((uint32_t *) imuDumpPacketMemory) + (2 * IMU_SAMPLE_SIZE - OUT_PACKET_BODY_BEGIN);
volatile expandedPidSample *imuDumpPacketBody = (expandedPidSample *) (imuDumpPacket + OUT_PACKET_BODY_BEGIN);
volatile uint16_t imuPacketChecksum = 0;
volatile unsigned int imuPacketBodyPointer = 0;
volatile bool imuPacketReady = false;

// Telemetry
volatile unsigned int imuSamplesRead = 0;
volatile unsigned int imuSamplesQueued = 0;

// Runs in main's setup()
void imuSetup() {

    assert(((unsigned int) imuDumpPacketBody) % 4 == 0);  // Check offset; Misaligned data may segfault at 0x20000000
    // assert(((unsigned int) imuSamples) % 4 == 0); // GCC says this is statically checkable
    debugPrintf("imuSamples location (we want this to be far from 0x2000000): %p to %p\n", imuSamples, imuSamples + IMU_BUFFER_SIZE);
    debugPrintf("imuDumpPacket location (we want this to be far from 0x2000000): memory begins %p, samples %p to %p\n", imuDumpPacketMemory, imuDumpPacket, imuDumpPacketMemory + IMU_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD);
    pinMode(IMU_DATA_READY_PIN, OUTPUT);
    /* TODO: put check back in // for (int i = 0; i < 10; i++) {
        imuSamples[IMU_BUFFER_SIZE + i] = 0xbeef;
        imuDumpPacket[IMU_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD + i] = 0xbeef;
    }
    assert (imuSamples[IMU_BUFFER_SIZE] == 0xbeef);*/
}

void writeExpandedPidSample(const pidSample* in, volatile expandedPidSample* out) {
    assert(sizeof(pidSample) * 2 == sizeof(expandedPidSample));
    assert(sizeof(pidSample) == 4 * 4 * 3);
    unsigned int num_uint32 = sizeof(pidSample) / 4;
    for (unsigned int i = 0; i < num_uint32; i++) {
        uint32_t num = ((uint32_t *) in)[i];
        ((uint32_t *) out)[2 * i] = num >> 16; // msb
        ((uint32_t *) out)[2 * i + 1] = num % (1 << 16); // lsb
    }
}

void checkDataDump() {
    noInterrupts();
    unsigned int imuPacketBodyPointerSave = imuPacketBodyPointer;
    unsigned int imuPacketReadySave = imuPacketReady;
    if(!assert((imuPacketBodyPointerSave == IMU_DATA_DUMP_SIZE) == imuPacketReadySave)) {
        debugPrintf("packetBodyPointer %d, ready? %d\n", imuPacketBodyPointerSave, imuPacketReadySave);
    }
    if (imuPacketReady) {
        interrupts();
        return;
    }
    noInterrupts();
    if ((imuPacketBodyPointer < IMU_DATA_DUMP_SIZE) && (imuSentDataPointer % IMU_BUFFER_SIZE) != (imuDataPointer % IMU_BUFFER_SIZE)) {
        pidSample sample = imuSamples[imuSentDataPointer];
        imuPacketChecksum += sample.getChecksum();
        writeExpandedPidSample(&sample, &(imuDumpPacketBody[imuPacketBodyPointer]));
        imuPacketBodyPointer++;
        imuSentDataPointer++;
    }
    imuSentDataPointer = imuSentDataPointer % IMU_BUFFER_SIZE;
    assert(imuPacketBodyPointer <= IMU_DATA_DUMP_SIZE);
    if (imuPacketBodyPointer >= IMU_DATA_DUMP_SIZE) {
        // Prepare packet!
        noInterrupts();
        assert((sizeof(pidSample) * 8) % 16 == 0);
        imuPacketReady = true;

        digitalWriteFast(IMU_DATA_READY_PIN, HIGH);
    }

    interrupts();
}
/*
bool shouldSample() {
    if ((imuDataPointer + IMU_SAMPLE_SIZE) % IMU_BUFFER_SIZE == imuSentDataPointer) {
        // Buffer is full
        sampling = false;
    }
    if (!sampling) {
        return false;
    }
    assert(timeSinceLastRead < 1.05 * IMU_SAMPLE_PERIOD);
    if (!(assert(timeSinceLastRead < 2 * IMU_SAMPLE_PERIOD))) { // ruh roh
        timeSinceLastRead = 0;
        return true;
    }
    if (timeSinceLastRead > IMU_SAMPLE_PERIOD) {
        timeSinceLastRead -= IMU_SAMPLE_PERIOD;
        return true;
    }

    return false;
}*/

void recordPid(const volatile pidSample& s) {
    //debugPrintf("Sampling %d imuDataPointer %d IMU_BUFFER_SIZE %d\n", sampling, imuDataPointer, IMU_BUFFER_SIZE);
    //assert(sampling);
    if (!sampling) {
        //debugPrintf("Warning: buffer is full!\n");
        return;
    }
    //assert(imuDataPointer % IMU_NUM_CHANNELS == 0);
    ((pidSample *) imuSamples)[imuDataPointer] = s;
    imuDataPointer = (imuDataPointer + 1) % IMU_BUFFER_SIZE;
    imuSamplesRead++;
    //debugPrintf("Base array %p, writing to %p, end of array %p\n", &((pidSample *) imuSamples)[0], &((pidSample *) imuSamples)[imuDataPointer], &imuSamples[IMU_BUFFER_SIZE]);
    assert(imuDataPointer <= IMU_BUFFER_SIZE);

    if (((imuDataPointer + 1) % IMU_BUFFER_SIZE) == (imuSentDataPointer % IMU_BUFFER_SIZE)) {
        sampling = false;
    }
}

// Runs in main loop
void taskIMU() {
    // TODO: replace // assert (imuSamples[IMU_BUFFER_SIZE] == 0xbeef);
    /* // TODO: replace // if (imuSamples[IMU_BUFFER_SIZE] != 0xbeef) {
        debugPrintf("End of buf is %x\n", imuSamples[IMU_BUFFER_SIZE]);
    } */
    //assert (imuDumpPacket[IMU_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD] == 0xbeef);
    // assert (imuDataPointer % IMU_SAMPLE_SIZE == 0);
    assert (imuDataPointer <= IMU_BUFFER_SIZE);
    assert (!(sampling && (imuDataPointer >= IMU_BUFFER_SIZE)));
    checkDataDump();
}

void imuPacketSent() {
    digitalWrite(IMU_DATA_READY_PIN, LOW);
    if ((assert(imuPacketReady))) {
        noInterrupts();
        imuPacketChecksum = 0;
        imuPacketBodyPointer = 0;
        imuPacketReady = false;
        imuSamplesSent += IMU_DATA_DUMP_SIZE;
        interrupts();
    }
}

void enterIMU() {
    digitalWriteFast(IMU_DATA_READY_PIN, LOW);
    imuPacketChecksum = 0;
    imuPacketReady = false;
    imuSentDataPointer = 0;
    imuDataPointer = 0;
    timeSinceLastRead = 0;
    imuPacketBodyPointer = 0;
    sampling = true;
}

void leaveIMU() {
    digitalWriteFast(IMU_DATA_READY_PIN, LOW);
    imuPacketReady = false;
    sampling = false;
    imuDataPointer = 0;
}

void imuHeartbeat() {
    Serial.printf("IMU front of buffer %d, back of buffer %d, packet ready? %d, chksum %d, packet pointer %d, sampling %d, packet %d, sent %d\n", imuDataPointer, imuSentDataPointer, imuPacketReady, imuPacketChecksum, imuPacketBodyPointer, sampling, imuPacketBodyPointer, imuSamplesSent);
}
