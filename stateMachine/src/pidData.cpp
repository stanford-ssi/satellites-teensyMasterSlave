#include <main.h>
#include <pidData.h>

// PID samples go here
// These are processed on the same main thread so hopefully no race conditions
// Indexes into pidSamples; Marks index of next data point to transmit to audacy
// If sentDataPointer == dataPointer, buffer is empty
// If dataPointer == sentDataPointer - PID_SAMPLE_SIZE, buffer is full
volatile unsigned int pidSentDataPointer = 0;
volatile unsigned int pidDataPointer = 0; // Marks index of next place to read into
volatile unsigned int pidSamplesSent = 0;
volatile pidSample pidSamples[PID_BUFFER_SIZE + 10]; // Add some extra space on the end in case we overflow
volatile bool sampling = false;
elapsedMicros timeSinceLastRead;

// This packet ships out directly to audacy
// This is more than we need; OUT_PACKET_OVERHEAD is measured in units of uint16_t, not pidSample
volatile expandedPidSample pidDumpPacketMemory[PID_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD + 10];
// We skip the first few uint16 because we want the packet body to begin at pidDumpPacketMemory[1], which has a good byte offset mod 32bit.
// Assume header size is less than PID_SAMPLE_SIZE
volatile uint32_t *pidDumpPacket = ((uint32_t *) pidDumpPacketMemory) + (6 * PID_SAMPLE_SIZE - OUT_PACKET_BODY_BEGIN);

// Where the telemetry goes
volatile expandedPidSample *pidDumpPacketBegin = (expandedPidSample *) (pidDumpPacket + OUT_PACKET_BODY_BEGIN);
// Where the samples go
volatile expandedPidSample *pidDumpPacketBody = pidDumpPacketBegin + 0;
volatile uint16_t pidPacketChecksum = 0;
volatile unsigned int pidPacketBodyPointer = 0;
volatile bool pidPacketReady = false;

// Telemetry
volatile unsigned int pidSamplesRead = 0;
volatile unsigned int pidSamplesQueued = 0;

// Runs in main's setup()
void pidDataSetup() {

    assert(((unsigned int) pidDumpPacketBody) % 4 == 0);  // Check offset; Misaligned data may segfault at 0x20000000
    (void) assert(((unsigned int) pidSamples) % 4 == 0);
    debugPrintf("pidSamples location (we want this to be far from 0x2000000): %p to %p\n", pidSamples, pidSamples + PID_BUFFER_SIZE);
    debugPrintf("pidDumpPacket location (we want this to be far from 0x2000000): memory begins %p, samples %p to %p\n", pidDumpPacketMemory, pidDumpPacket, pidDumpPacketMemory + PID_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD);
    pinMode(PID_DATA_READY_PIN, OUTPUT);
    for (int i = 0; i < 10; i++) {
        ((uint16_t *) &pidSamples[PID_BUFFER_SIZE])[i] = 0xbeef;
        pidDumpPacket[PID_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD + i] = 0xbeef;
    }
    assert (pidSamples[PID_BUFFER_SIZE].sample.axis1 == 0xbeefbeef);
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
    unsigned int pidPacketBodyPointerSave = pidPacketBodyPointer;
    unsigned int pidPacketReadySave = pidPacketReady;
    if(!assert((pidPacketBodyPointerSave == PID_DATA_DUMP_SIZE) == pidPacketReadySave)) {
        debugPrintf("packetBodyPointer %d, ready? %d\n", pidPacketBodyPointerSave, pidPacketReadySave);
    }
    if (pidPacketReady) {
        interrupts();
        return;
    }
    noInterrupts();
    if ((pidPacketBodyPointer < PID_DATA_DUMP_SIZE) && (pidSentDataPointer % PID_BUFFER_SIZE) != (pidDataPointer % PID_BUFFER_SIZE)) {
        pidSample sample = pidSamples[pidSentDataPointer];
        writeExpandedPidSampleWithChecksum(&sample, &(pidDumpPacketBody[pidPacketBodyPointer]), pidPacketChecksum);
        pidPacketBodyPointer++;
        pidSentDataPointer++;
    }
    pidSentDataPointer = pidSentDataPointer % PID_BUFFER_SIZE;
    assert(pidPacketBodyPointer <= PID_DATA_DUMP_SIZE);
    if (pidPacketBodyPointer >= PID_DATA_DUMP_SIZE) {
        // Prepare packet!
        noInterrupts();
        assert((sizeof(pidSample) * 8) % 16 == 0);
        pidPacketReady = true;

        digitalWriteFast(PID_DATA_READY_PIN, HIGH);
    }

    interrupts();
}

void recordPid(const volatile pidSample& s) {
    if (!sampling) {
        return;
    }
    ((pidSample *) pidSamples)[pidDataPointer] = s;
    pidDataPointer = (pidDataPointer + 1) % PID_BUFFER_SIZE;
    pidSamplesRead++;
    assert(pidDataPointer <= PID_BUFFER_SIZE);

    if (((pidDataPointer + 1) % PID_BUFFER_SIZE) == (pidSentDataPointer % PID_BUFFER_SIZE)) {
        sampling = false;
    }
}

// Runs in main loop
void taskPidData() {
    if (!assert (pidSamples[PID_BUFFER_SIZE].sample.axis1 == 0xbeefbeef)) {
        debugPrintf("End of buf is %x\n", pidSamples[PID_BUFFER_SIZE].sample.axis1);
    }
    assert (pidDataPointer <= PID_BUFFER_SIZE);
    assert (!(sampling && (pidDataPointer >= PID_BUFFER_SIZE)));
    checkDataDump();
}

void pidPacketSent() {
    digitalWrite(PID_DATA_READY_PIN, LOW);
    if ((assert(pidPacketReady))) {
        noInterrupts();
        pidPacketChecksum = 0;
        pidPacketBodyPointer = 0;
        pidPacketReady = false;
        pidSamplesSent += PID_DATA_DUMP_SIZE;
        interrupts();
    }
}

void enterPidData() {
    digitalWriteFast(PID_DATA_READY_PIN, LOW);
    noInterrupts();
    pidPacketChecksum = 0;
    pidPacketReady = false;
    pidSentDataPointer = 0;
    pidDataPointer = 0;
    timeSinceLastRead = 0;
    pidPacketBodyPointer = 0;
    sampling = true;
    interrupts();
}

void leavePidData() {
    digitalWriteFast(PID_DATA_READY_PIN, LOW);
    noInterrupts();
    pidPacketReady = false;
    sampling = false;
    pidDataPointer = 0;
    interrupts();
}

void pidDataHeartbeat() {
    Serial.printf("PID front of buffer %d, back of buffer %d, packet ready? %d, chksum %d, packet pointer %d, sampling %d, packet %d, sent %d\n", pidDataPointer, pidSentDataPointer, pidPacketReady, pidPacketChecksum, pidPacketBodyPointer, sampling, pidPacketBodyPointer, pidSamplesSent);
}
