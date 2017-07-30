#include <main.h>
#include <pidData.h>

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
volatile uint32_t *imuDumpPacket = ((uint32_t *) imuDumpPacketMemory) + (6 * IMU_SAMPLE_SIZE - OUT_PACKET_BODY_BEGIN);

// Where the telemetry goes
volatile expandedPidSample *imuDumpPacketBegin = (expandedPidSample *) (imuDumpPacket + OUT_PACKET_BODY_BEGIN);
// Where the samples go
volatile expandedPidSample *imuDumpPacketBody = imuDumpPacketBegin + 0;
volatile uint16_t imuPacketChecksum = 0;
volatile unsigned int imuPacketBodyPointer = 0;
volatile bool imuPacketReady = false;

// Telemetry
volatile unsigned int imuSamplesRead = 0;
volatile unsigned int imuSamplesQueued = 0;

// Runs in main's setup()
void imuSetup() {

    assert(((unsigned int) imuDumpPacketBody) % 4 == 0);  // Check offset; Misaligned data may segfault at 0x20000000
    (void) assert(((unsigned int) imuSamples) % 4 == 0);
    debugPrintf("imuSamples location (we want this to be far from 0x2000000): %p to %p\n", imuSamples, imuSamples + IMU_BUFFER_SIZE);
    debugPrintf("imuDumpPacket location (we want this to be far from 0x2000000): memory begins %p, samples %p to %p\n", imuDumpPacketMemory, imuDumpPacket, imuDumpPacketMemory + IMU_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD);
    pinMode(IMU_DATA_READY_PIN, OUTPUT);
    for (int i = 0; i < 10; i++) {
        ((uint16_t *) &imuSamples[IMU_BUFFER_SIZE])[i] = 0xbeef;
        imuDumpPacket[IMU_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD + i] = 0xbeef;
    }
    assert (imuSamples[IMU_BUFFER_SIZE].sample.axis1 == 0xbeefbeef);
}

void writeExpandedPidSampleWithChecksum(const pidSample* in, volatile expandedPidSample* out, volatile uint16_t& pidBufferChecksum) {
    assert(sizeof(pidSample) * 2 == sizeof(expandedPidSample));
    assert(sizeof(pidSample) == 4 * 4 * 3);
    unsigned int num_uint32 = sizeof(pidSample) / 4;
    for (unsigned int i = 0; i < num_uint32; i++) {
        uint32_t num = ((uint32_t *) in)[i];
        ((uint32_t *) out)[2 * i] = num >> 16; // msb
        pidBufferChecksum += num >> 16;
        ((uint32_t *) out)[2 * i + 1] = num % (1 << 16); // lsb
        pidBufferChecksum += num % (1 << 16);
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
        writeExpandedPidSampleWithChecksum(&sample, &(imuDumpPacketBody[imuPacketBodyPointer]), imuPacketChecksum);
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

void recordPid(const volatile pidSample& s) {
    if (!sampling) {
        return;
    }
    ((pidSample *) imuSamples)[imuDataPointer] = s;
    imuDataPointer = (imuDataPointer + 1) % IMU_BUFFER_SIZE;
    imuSamplesRead++;
    assert(imuDataPointer <= IMU_BUFFER_SIZE);

    if (((imuDataPointer + 1) % IMU_BUFFER_SIZE) == (imuSentDataPointer % IMU_BUFFER_SIZE)) {
        sampling = false;
    }
}

// Runs in main loop
void taskIMU() {
    if (!assert (imuSamples[IMU_BUFFER_SIZE].sample.axis1 == 0xbeefbeef)) {
        debugPrintf("End of buf is %x\n", imuSamples[IMU_BUFFER_SIZE].sample.axis1);
    }
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
    noInterrupts();
    imuPacketChecksum = 0;
    imuPacketReady = false;
    imuSentDataPointer = 0;
    imuDataPointer = 0;
    timeSinceLastRead = 0;
    imuPacketBodyPointer = 0;
    sampling = true;
    interrupts();
}

void leaveIMU() {
    digitalWriteFast(IMU_DATA_READY_PIN, LOW);
    noInterrupts();
    imuPacketReady = false;
    sampling = false;
    imuDataPointer = 0;
    interrupts();
}

void imuHeartbeat() {
    Serial.printf("IMU front of buffer %d, back of buffer %d, packet ready? %d, chksum %d, packet pointer %d, sampling %d, packet %d, sent %d\n", imuDataPointer, imuSentDataPointer, imuPacketReady, imuPacketChecksum, imuPacketBodyPointer, sampling, imuPacketBodyPointer, imuSamplesSent);
}
