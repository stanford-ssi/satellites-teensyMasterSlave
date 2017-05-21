#include <main.h>

// Imu code includes a lot of buffers moving around
// Currently not circular
// Very bug prone; needs more testing

// Imu samples go here
// These are processed on the same main thread so hopefully no race conditions
// Indexes into imuSamples; Marks index of next data point to transmit to audacy
// If sentDataPointer == dataPointer, buffer is empty
// If dataPointer == sentDataPointer - IMU_SAMPLE_SIZE, buffer is full
volatile unsigned int imuSentDataPointer = 0;
volatile unsigned int imuDataPointer = 0; // Marks index of next place to read into
volatile uint16_t imuSamples[IMU_BUFFER_SIZE + 10]; // Add some extra space on the end in case we overflow
volatile bool sampling = false;
elapsedMicros timeSinceLastRead;

// This packet ships out directly to audacy
volatile uint16_t imuDumpPacket[IMU_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD + 10];
volatile uint16_t *imuDumpPacketBody = imuDumpPacket + OUT_PACKET_BODY_BEGIN;
volatile uint16_t imuPacketChecksum = 0;
volatile unsigned int imuPacketBodyPointer = 0;
volatile bool imuPacketReady = false;

// Telemetry
volatile unsigned int imuSamplesRead = 0;
volatile unsigned int imuSamplesQueued = 0;

// Runs in main's setup()
void imuSetup() {
    pinMode(IMU_CS_PIN, OUTPUT);
    for (int i = 0; i < 10; i++) {
        imuSamples[IMU_BUFFER_SIZE + i] = 0xbeef;
        imuDumpPacket[IMU_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD + i] = 0xbeef;
    }
}

void checkDataDump() {
    assert(imuSentDataPointer % IMU_SAMPLE_SIZE == 0);
    assert(imuDataPointer % IMU_SAMPLE_SIZE == 0);
    assert((imuPacketBodyPointer == IMU_DATA_DUMP_SIZE) == imuPacketReady);
    if ((imuPacketBodyPointer < IMU_DATA_DUMP_SIZE) && (imuSentDataPointer % IMU_BUFFER_SIZE) != (imuDataPointer % IMU_BUFFER_SIZE)) {
        for (int i = 0; i < IMU_NUM_CHANNELS; i++) {
            uint16_t sample = imuSamples[imuSentDataPointer];
            imuPacketChecksum += sample;
            imuDumpPacketBody[imuPacketBodyPointer] = sample;
            imuPacketBodyPointer++;
            imuSentDataPointer++;
        }
    }
    imuSentDataPointer = imuSentDataPointer % IMU_BUFFER_SIZE;
    assert(imuPacketBodyPointer <= IMU_DATA_DUMP_SIZE);
    if (imuPacketBodyPointer >= IMU_DATA_DUMP_SIZE) {
        // Prepare packet!
        noInterrupts();
        //setupTransmissionWithChecksum(RESPONSE_IMU_DATA, IMU_DATA_DUMP_SIZE, imuPacketChecksum, imuDumpPacket);
        imuPacketReady = true;
        digitalWriteFast(IMU_DATA_READY_PIN, HIGH);
        interrupts();
    }
}

bool shouldSample() {
    if ((imuDataPointer + IMU_SAMPLE_SIZE) % IMU_BUFFER_SIZE == imuSentDataPointer) {
        // Buffer is full
        sampling = false;
    }
    if (!sampling) {
        return false;
    }
    assert(timeSinceLastRead < 1.01 * IMU_SAMPLE_PERIOD);
    if (!(assert(timeSinceLastRead < 2 * IMU_SAMPLE_PERIOD))) { // ruh roh
        timeSinceLastRead = 0;
        return true;
    }
    if (timeSinceLastRead > IMU_SAMPLE_PERIOD) {
        timeSinceLastRead -= IMU_SAMPLE_PERIOD;
        return true;
    }

    return false;
}

void sample() {
    digitalWrite(IMU_CS_PIN, LOW);
    for (int i = 0; i < IMU_NUM_CHANNELS; i++) {
        uint16_t channelRead = SPI2.transfer16(0xffff);
        (void) channelRead; // TODO: right now we log increasing counter to check correct circular buffer
        imuSamples[imuDataPointer] = (micros() % 65536);
        imuDataPointer++;
        imuSamplesRead++;
    }
    assert(imuDataPointer <= IMU_BUFFER_SIZE);
    if (imuDataPointer >= IMU_BUFFER_SIZE) {
        sampling = false;
    }
    digitalWrite(IMU_CS_PIN, HIGH);
}

// Runs in main loop
void taskIMU() {
    assert (imuSamples[IMU_BUFFER_SIZE] == 0xbeef);
    assert (imuDumpPacket[IMU_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD] == 0xbeef);
    assert (imuDataPointer % IMU_SAMPLE_SIZE == 0);
    assert (imuDataPointer <= IMU_BUFFER_SIZE);
    assert (!(sampling && (imuDataPointer >= IMU_BUFFER_SIZE)));
    if (shouldSample()) {
        sample();
    }
    checkDataDump();
}

void imuPacketSent() {
    if ((assert(imuPacketReady))) {
        noInterrupts();
        imuPacketChecksum = 0;
        imuPacketBodyPointer = 0;
        imuPacketReady = false;
        imuPacketChecksum = 0;
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
    digitalWriteFast(IMU_CS_PIN, HIGH);
    sampling = true;
}

void leaveIMU() {
    digitalWriteFast(IMU_DATA_READY_PIN, LOW);
    imuPacketReady = false;
    sampling = false;
    imuDataPointer = 0;
}

void imuHeartbeat() {
    Serial.printf("IMU front of buffer %d, back of buffer %d, packet ready? %d, chksum %d, packet pointer %d, sampling %d, packet %d\n", imuDataPointer, imuSentDataPointer, imuPacketReady, imuPacketChecksum, imuPacketBodyPointer, sampling, imuPacketBodyPointer);
}
