#include "packet.h"
#include "main.h"
#include "states.h"

const volatile bool SPI2_LOOPBACK = false;

IntervalTimer timer, timer2, timer3, timer4;
volatile uint16_t state = SETUP_STATE;
volatile uint16_t previousState = SETUP_STATE;
volatile bool changingState = false;
volatile unsigned int timeAlive = 0;
volatile unsigned int errors = 0;
volatile unsigned int bugs = 0;
long startOfLoop = 0;
volatile bool ignoreLoopTime = false;
volatile unsigned int lastLoopTime = 0;
volatile unsigned int maxLoopTime = 0;
volatile unsigned int lastLoopState = state;
volatile unsigned int lastAnalogRead = 0;
volatile unsigned int numLoops = 0; // Mostly just to count % of loops that we need to clear dma buffer


volatile unsigned int numFastLoops = 0;
volatile unsigned int numSlowLoops = 0;
volatile unsigned int numMediumLoops = 0;
unsigned int SYNC=3;
unsigned int SMPL_CLK=29;
unsigned int CS0=35;
unsigned int CS1=37;
unsigned int CS2=7;
unsigned int CS3=2;
unsigned int DRL=26;
unsigned int derp=0;
signed long SAMPLE=0;
const unsigned int control_word = 0b1000011100100000;

void capture(void) {
  digitalWrite(CS2, LOW);
  derp = SPI.transfer16(0x0000);
  SAMPLE = (derp << 16);
  derp = SPI.transfer16(0x0000);
  SAMPLE = SAMPLE + derp;
  derp = 0;
  Serial.printf("Sample A %d\n", SAMPLE);
  digitalWrite(CS2, HIGH);
  delay(1);
  digitalWrite(CS3, LOW);
  derp = SPI.transfer16(0x0000);
  SAMPLE = (derp << 16);
  derp = SPI.transfer16(0x0000);
  SAMPLE = SAMPLE + derp;
  derp = 0;
  Serial.printf("Sample B %d\n", SAMPLE);
  digitalWrite(CS3, HIGH);
  delay(1);
  digitalWrite(CS0, LOW);
  derp = SPI.transfer16(0x0000);
  SAMPLE = (derp << 16);
  derp = SPI.transfer16(0x0000);
  SAMPLE = SAMPLE + derp;
  derp = 0;
  Serial.printf("Sample C %d\n", SAMPLE);
  digitalWrite(CS0, HIGH);
  delay(1);
  digitalWrite(CS1, LOW);
  derp = SPI.transfer16(0x0000);
  SAMPLE = (derp << 16);
  derp = SPI.transfer16(0x0000);
  SAMPLE = SAMPLE + derp;
  derp = 0;
  Serial.printf("Sample D %d\n", SAMPLE);
  digitalWrite(CS1, HIGH);
}

void jklol() {
  Serial.begin(115200);
  pinMode(54, OUTPUT);
  pinMode(52, OUTPUT);
  pinMode(53, OUTPUT);
  digitalWrite(54, HIGH);
  digitalWrite(52, HIGH);
  digitalWrite(53, HIGH);
  pinMode(SYNC, OUTPUT);
  pinMode(SMPL_CLK, OUTPUT);
  pinMode(CS0, OUTPUT);
  pinMode(CS1, OUTPUT);
  pinMode(CS2, OUTPUT);
  pinMode(CS3, OUTPUT);
  digitalWriteFast(CS0, HIGH);
  digitalWriteFast(CS1, HIGH);
  digitalWriteFast(CS2, HIGH);
  digitalWriteFast(CS3, HIGH);
  pinMode(DRL, INPUT);
  attachInterrupt(DRL, capture, FALLING);
  Serial.println("Wassup bby");
  SPI.begin();
  digitalWrite(SYNC, LOW);
  digitalWrite(SYNC, HIGH);
  digitalWrite(SYNC, LOW);
  digitalWrite(CS0, LOW);
  Serial.printf("%x\n", SPI.transfer16(control_word));
  digitalWrite(CS0, HIGH);
  delay(1);
  digitalWrite(CS1, LOW);
  Serial.printf("%x\n", SPI.transfer16(control_word));
  digitalWrite(CS1, HIGH);
  delay(1);
  digitalWrite(CS2, LOW);
  Serial.printf("%x\n", SPI.transfer16(control_word));
  digitalWrite(CS2, HIGH);
  delay(1);
  digitalWrite(CS3, LOW);
  Serial.printf("%x\n", SPI.transfer16(control_word));
  digitalWrite(CS3, HIGH);
  analogWriteFrequency(SMPL_CLK, 40);
  analogWrite(SMPL_CLK, 128);
    while (true) {
      delay(1);
      //Serial.println("hey");
      /*digitalWrite(SMPL_CLK, HIGH);
      delay(1);
      digitalWrite(SMPL_CLK, LOW);*/
      if (Serial.available()) {
        char c = Serial.read();
        if (c == 't') {
          //digitalWrite(SYNC, HIGH);
          //digitalWrite(SYNC, LOW);
          digitalWrite(SMPL_CLK, HIGH);
          digitalWrite(SMPL_CLK, LOW);
        }
        if (c == 's') {
          digitalWrite(CS3, LOW);
          SPI.transfer16(control_word);
          digitalWrite(CS3, HIGH);
        }
      }
      if (micros() % 3000000 == 0) {
        Serial.println("Hey");
      }
  }
}


void setup() {
  //jklol();
  Serial.begin(115200);
  debugPrintf("Serial is on\n");
  delay(500);
  // Every 2.75 seconds - integer quantity of seconds prevents us from seeing
  // dma offset change, as sampling rate is a clean multiple of buffer size
  const int heartRate = 2751234;
  timer.begin(heartbeat, heartRate);
  delay(1);
  timer2.begin(heartbeat2, heartRate);
  delay(1);
  timer3.begin(heartbeat3, heartRate);
  delay(1);
  timer4.begin(heartbeat4, heartRate);
  debugPrintf("Set up heartbeat interrupt\n");
  analogReadResolution(16);
  debugPrintf("Begin Spi2\n");
  debugPrintf("Setting up packet:\n");
  packet_setup();
  debugPrintf("Done.\n");
  debugPrintf("Setting up dma:\n");
  spiMasterSetup();
  debugPrintf("Done!\n");
  debugPrintf("Setting up tracking:\n");
  trackingSetup();
  debugPrintf("Done!\n");
  state = IDLE_STATE;
  delay(1000);
  debugPrintf("Running tests:\n");
  runTests();
  //debugPrintf("Hey %d\n", assert(state != IDLE_STATE));
}

bool assertionError(const char* file, int line, const char* assertion) {
    errors++;
    bugs++;
    Serial.printf("%s, %d: assertion 'assertion' failed, total errors %d, bugs %d\n", file, line, errors, bugs);
    return false;
}

extern uint16_t packet[];
extern uint16_t spi_tx_out[];
extern DMAChannel dma_rx;
extern DMAChannel dma_tx;
extern DMAChannel dma_tx2;
void heartbeat() {
    debugPrintf("State %d,", state);
    debugPrintf("transmitting %d, packetsReceived %d, ", transmitting, packetsReceived);
    debugPrintf("%d errors, bugs %d, last loop %d micros, max loop time %d micros", errors, bugs, lastLoopTime, maxLoopTime);
    maxLoopTime = 0;
}

extern volatile unsigned int frontOfBuffer, backOfBuffer;
void heartbeat2() {
    debugPrintf(", last state %d, last read %d, dma offset %d %d %d", lastLoopState, lastAnalogRead, adcGetOffset(), backOfBuffer, frontOfBuffer);
    debugPrintf(", %d loops, time alive %d, numSamplesRead %d\n", numLoops, timeAlive, numSamplesRead);
    debugPrintf("%d last, %d max, %d fast, %d med, %d slow loops\n", lastLoopTime, maxLoopTime, numFastLoops, numMediumLoops, numSlowLoops);
}

void heartbeat3() {
    if (state == TRACKING_STATE) {
        trackingHeartbeat();
    } else if (state == CALIBRATION_STATE) {
        calibrationHeartbeat();
    }
}

extern volatile int numFail;
extern volatile int numSuccess;
extern volatile int numStartCalls;
extern volatile int numSpi0Calls;
void heartbeat4() {
    debugPrintf("DMA Fail %d success %d starts %d spi0s %d\n", numFail, numSuccess, numStartCalls, numSpi0Calls);
    debugPrintf("-----------------------\n");
}

void taskIdle(void) {
}

void leaveCurrentState() {
    assert(previousState != SHUTDOWN_STATE);
    if (previousState == IDLE_STATE) {
        //leaveIdle(); // Nothing to do here
    } else if (previousState == TRACKING_STATE) {
        leaveTracking();
    } else if (previousState == CALIBRATION_STATE) {
        leaveCalibration();
    }
}

void enterNextState() {
    if (state == IDLE_STATE) {
        //enterIdle(); // Nothing to do here
    } else if (state == TRACKING_STATE) {
        enterTracking();
    } else if (state == CALIBRATION_STATE) {
        enterCalibration();
    }
}

void checkTasks(void) {
    long startOfLoop = micros();
    assert(state <= MAX_STATE);
    if (changingState) {
        noInterrupts();
        leaveCurrentState();
        enterNextState();
        changingState = false;
        interrupts();
    }
    if (state == IDLE_STATE) {
        taskIdle();
    } else if (state == TRACKING_STATE) {
        taskTracking();
    } else if (state == CALIBRATION_STATE) {
        taskCalibration();
    }

    // Save this into a long because elapsedMillis is not guaranteed in interrupts
    timeAlive = millis();
    if (ignoreLoopTime) {
        ignoreLoopTime = false;
    } else {
        lastLoopTime = micros() - startOfLoop;
        lastLoopState = state;
        if (lastLoopTime > maxLoopTime) {
            maxLoopTime = lastLoopTime;
        }
        if (lastLoopTime < 10) {
            numFastLoops++;
        } else if (lastLoopTime < 100) {
            numMediumLoops++;
        } else {
            numSlowLoops++;
        }
        // For now just assert, there isn't really a way to recover from a long main loop
        assert(lastLoopTime <= 1000000.0);
    }
    numLoops++;
}

void loop() {
    checkTasks();
}
