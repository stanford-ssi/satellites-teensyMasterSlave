#include <main.h>
#include <pidData.h>

/* *** Circular buffer for PID samples queued for logging *** */
// Indexes into pidSamples; Marks index of next data point to transmit to audacy
volatile unsigned int pidSentDataPointer = 0;
// If sentDataPointer == dataPointer, buffer is empty
// If dataPointer == sentDataPointer - 1, buffer is full
volatile unsigned int pidDataPointer = 0; // Marks index of next place to read into
volatile unsigned int pidSamplesSent = 0;
volatile pidSample pidSamples[PID_BUFFER_SIZE + 10]; // Add some extra space on the end in case we overflow
volatile bool sampling = false;
elapsedMicros timeSinceLastRead;

volatile pidDumpPacket_t pidDumpPacket;
volatile uint32_t* pidDumpPacketUints = (uint32_t *) &(pidDumpPacket.header);

volatile uint16_t pidPacketChecksum = 0;
volatile unsigned int pidPacketBodyPointer = 0;
volatile bool pidPacketReady = false;

// Telemetry
volatile unsigned int pidSamplesRead = 0;
volatile unsigned int pidSamplesQueued = 0;

void write32WithChecksum(const uint16_t& in, volatile uint32_t& out, volatile uint16_t& pidBufferChecksum) {
    pidBufferChecksum += in;
    out = in;
}

// Checksum is updated as samples are pushed into this buffer
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

void populateHeader() {
    write32WithChecksum(0, pidDumpPacket.p_x, pidPacketChecksum);
    write32WithChecksum(1, pidDumpPacket.i_x, pidPacketChecksum);
    write32WithChecksum(2, pidDumpPacket.d_x, pidPacketChecksum);
    write32WithChecksum(3, pidDumpPacket.p_y, pidPacketChecksum);
    write32WithChecksum(4, pidDumpPacket.i_y, pidPacketChecksum);
    write32WithChecksum(5, pidDumpPacket.d_y, pidPacketChecksum);
    write32WithChecksum(6, pidDumpPacket.last_x, pidPacketChecksum);
    write32WithChecksum(7, pidDumpPacket.last_y, pidPacketChecksum);
    write32WithChecksum(8, pidDumpPacket.set_x, pidPacketChecksum);
    write32WithChecksum(9, pidDumpPacket.set_y, pidPacketChecksum);
}

bool pidBufferEmpty() {
    return (pidSentDataPointer % PID_BUFFER_SIZE) == (pidDataPointer % PID_BUFFER_SIZE);
}

bool pidBufferFull() {
    return ((pidDataPointer + 1) % PID_BUFFER_SIZE) == (pidSentDataPointer % PID_BUFFER_SIZE);
}

// Runs in main's setup()
void pidDataSetup() {
    (void) assert(((unsigned int) pidDumpPacket.body) % 4 == 0);  // Check offset; Misaligned data may segfault at 0x20000000
    (void) assert(((unsigned int) pidSamples) % 4 == 0);
    debugPrintf("pidSamples location (we want this to be far from 0x2000000): %p to %p\n", pidSamples, pidSamples + PID_BUFFER_SIZE);
    debugPrintf("pidDumpPacket location (we want this to be far from 0x2000000): memory begins %p, samples %p to %p\n", &pidDumpPacket, pidDumpPacket.body, pidDumpPacket.abcdFooter + ABCD_BUFFER_SIZE);
    pinMode(PID_DATA_READY_PIN, OUTPUT);
    for (int i = 0; i < 10; i++) {
        ((uint16_t *) &pidSamples[PID_BUFFER_SIZE])[i] = 0xbeef;
        ((uint16_t *) pidDumpPacket.abcdFooter)[ABCD_BUFFER_SIZE + i] = 0xbeef;
    }
    for (int i = 0; i < ABCD_BUFFER_SIZE; i++) {
        pidDumpPacket.abcdHeader[i] = 0xabcd;
        pidDumpPacket.abcdFooter[i] = 0xabcd;
    }
    assert ((uint32_t) pidSamples[PID_BUFFER_SIZE].sample.axis1 == 0xbeefbeef);
}

/*void restartSamplingIfApplicable() {
    noInterrupts();
    if (pidBufferEmpty() && !sampling && !tryingToRestartSampling && !pidPacketReady) {
        // Start sampling again!
        pidPacketBodyPointer = 0;
        pidPacketChecksum = 0;
        tryingToRestartSampling = true;
    }
    interrupts();
}

void sentTriggerPacket() {
}*/

/* *** Dequeues from pidSamples and moves samples into next packet *** */
void checkDataDump() {
    noInterrupts();
    unsigned int pidPacketBodyPointerSave = pidPacketBodyPointer;
    unsigned int pidPacketReadySave = pidPacketReady;
    if(!assert((pidPacketBodyPointerSave == PID_DATA_DUMP_SIZE) == pidPacketReadySave)) {
        // Likely a race condition
        debugPrintf("packetBodyPointer %d, ready? %d\n", pidPacketBodyPointerSave, pidPacketReadySave);
    }
    if (pidPacketReady) {
        interrupts();
        return;
    }
    noInterrupts();
    if ((pidPacketBodyPointer < PID_DATA_DUMP_SIZE) && !pidBufferEmpty()) {
        // Move a sample from large buffer to packet buffer
        if (pidPacketBodyPointer == 0) {
            populateHeader();
        }
        pidSample sample = pidSamples[pidSentDataPointer];
        writeExpandedPidSampleWithChecksum(&sample, &(pidDumpPacket.body[pidPacketBodyPointer]), pidPacketChecksum);
        pidPacketBodyPointer++;
        pidSentDataPointer++;
    }
    pidSentDataPointer = pidSentDataPointer % PID_BUFFER_SIZE;
    assert(pidPacketBodyPointer <= PID_DATA_DUMP_SIZE);
    if (pidPacketBodyPointer >= PID_DATA_DUMP_SIZE) {
        // Packet is ready!
        noInterrupts();
        assert((sizeof(pidSample) * 8) % 16 == 0);
        pidPacketReady = true;

        digitalWriteFast(PID_DATA_READY_PIN, HIGH);
    }

    interrupts();
}

// Enqueue pid sample for logging
void recordPid(const volatile pidSample& s) {
    if (!sampling) {
        return;
    }
    ((pidSample *) pidSamples)[pidDataPointer] = s;
    pidDataPointer = (pidDataPointer + 1) % PID_BUFFER_SIZE;
    pidSamplesRead++;
    assert(pidDataPointer <= PID_BUFFER_SIZE);

    if (pidBufferFull()) {
        sampling = false;
    }
}

// Runs in main loop
void taskPidData() {
    if (!assert ((uint32_t) pidSamples[PID_BUFFER_SIZE].sample.axis1 == 0xbeefbeef)) {
        debugPrintf("End of buf is %x\n", pidSamples[PID_BUFFER_SIZE].sample.axis1);
    }
    assert (pidDataPointer <= PID_BUFFER_SIZE);
    assert (!(sampling && (pidDataPointer >= PID_BUFFER_SIZE)));
    checkDataDump();
}

// Called from dma completion - clears packet buffer so we can prepare the next packet
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

// Called from enterTracking
void enterPidData() {
    noInterrupts();
    pidPacketChecksum = 0;
    pidPacketReady = false;
    pidSentDataPointer = 0;
    pidDataPointer = 0;
    timeSinceLastRead = 0;
    pidPacketBodyPointer = 0;
    sampling = true;
    digitalWriteFast(PID_DATA_READY_PIN, LOW);
    interrupts();
}

// Called from leaveTracking
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
