
#include <t3spi.h>
#include <Wire.h>
#include <ssiSpi.h>

//Initialize T3SPI class as SPI_SLAVE
T3SPI SPI_SLAVE;

//The number of integers per data packet
//MUST be the same as defined on the MASTER device
#define dataLength  256
#define SLAVE_CHIP_SELECT 10

//Initialize the arrays for incoming data
volatile uint8_t data[dataLength] = {};
//volatile uint16_t data[dataLength] = {};

int k = 0;
volatile int bytesReceived = 0;
volatile int packetsReceived = 0;
volatile int timesCleared = 0;
volatile int timesBlocked = 0;
volatile int timesSent = 0;
elapsedMillis em;
elapsedMillis runtime;
IntervalTimer timer;
uint8_t checksum = 0;

SPISettings settingsA(6250000, MSBFIRST, SPI_MODE0);
void setup() {
  Serial.begin(115200);

  //Begin SPI in SLAVE (SCK pin, MOSI pin, MISO pin, CS pin)
  SPI_SLAVE.begin_SLAVE(SCK, MOSI, MISO, T3_CS0);
  //SPI_SLAVE.begin_SLAVE(SCK, MOSI, MISO, T3_CS0);

  //Set the CTAR0_SLAVE0 (Frame Size, SPI Mode)
  SPI_SLAVE.setCTAR_SLAVE(8, T3_SPI_MODE0);
  //SPI_SLAVE.setCTAR_SLAVE(16, SPI_MODE0);

  //Enable the SPI0 Interrupt
  NVIC_ENABLE_IRQ(IRQ_SPI0);
  analogReadResolution(16);
  runtime = 0;
  em = 0;
  SPI1.begin();
  attachInterrupt(SLAVE_CHIP_SELECT, clearBuffer, FALLING);
  //attachInterrupt(SLAVE_CHIP_SELECT, clearBuffer2, RISING);
  //SPI1.beginTransaction(settingsA);
}

void loop() {
  int j = analogRead(0);
  int cs_state = digitalRead(10);
  /*SPI1.transfer(0xde);
  SPI1.transfer(0xde);
  SPI1.transfer(0xde);
  SPI1.transfer(0xde);*/
  if (cs_state == 1) {
    int iters;
    noInterrupts();
    for (iters = 0; iters < 5 && (digitalRead(10) == 1); iters++) {
      SPI1.transfer(0xde);
      SPI1.transfer(0xde);
    }
    interrupts();
    /*if (iters != 5) {
      Serial.println("Comms cut!");
    }*/
    timesSent++;
  } else {
    timesBlocked++;
  }

  if (em > 2000) {
    noInterrupts();
    em = 0;
    bytesReceived = 0;
    packetsReceived = 0;
    timesCleared = 0;
    k = 0;
    timesBlocked = 0;
    timesSent = 0;
    interrupts();
  }

  //Capture the time before receiving data
  if (SPI_SLAVE.dataPointer == 0 && SPI_SLAVE.packetCT == 0) {
    SPI_SLAVE.timeStamp1 = micros();
  }

  //Capture the time when transfer is done
  if (SPI_SLAVE.packetCT == 1) {
    if (k % 1 == 0) {
      SPI_SLAVE.timeStamp2 = micros();


      //Print data received & data sent
      for (int i = 0; i < dataLength; i++) {
        if (i < dataLength - 1) {
          checksum += data[i];
        }

      }
      if (checksum != data[dataLength - 1]) {
        /*for (int i = 0; i < dataLength; i+=17) {
          Serial.print("data[");
          Serial.print(i);
          Serial.print("]: ");
          Serial.println(data[i]);
          Serial.flush();
        }*/
//        unsigned totalTime = runtime;
//        Serial.printf("Bad checksum: first byte %d, received %d, calculated %d, runtime %d\n", data[0], data[dataLength - 1], checksum, totalTime);
//        Serial.flush();
        //while (1);
        //Serial.printf("Checksum ok? %d\n", checksum == data[dataLength - 1]);
      }


      //Print statistics for the previous transfer
      //SPI_SLAVE.printStatistics(dataLength);

    }
    //Reset the packet count
    SPI_SLAVE.packetCT = 0;
    packetsReceived++;
  }
  if (k % 20000 == 0) {
    float end_ = em / 1000.0;
    Serial.printf("analogRead %d, sent %d pcent, numLoops %d, bytesReceived %d,\n timesCleared %d, packetsReceived %d, seconds %f, per sec %d\n", 
      j, (timesSent * 100)/k, k, bytesReceived, timesCleared, packetsReceived, end_, (int) (k / end_));
    if ((bytesReceived / 256) != packetsReceived) {
      Serial.println("Missed a packet!");
    }
  }
  k++;

}

//Interrupt Service Routine to handle incoming data
void spi0_isr(void) {

  //Function to handle data
  SPI_SLAVE.rx8 (data, dataLength);
  bytesReceived++;
  //  SPI_SLAVE.rx16(data, dataLength);

}

void clearBuffer(void) {
  checksum = 0;
  if (SPI_SLAVE.dataPointer != 0) {
    Serial.printf("Clearing %d bytes of data\n", SPI_SLAVE.dataPointer);
  }
  SPI_SLAVE.clearBuffer();
  timesCleared++;
}

void clearBuffer2(void) {
  SPI_SLAVE.clearBuffer();
}


