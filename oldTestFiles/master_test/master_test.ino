
#include <t3spi.h>

//Initialize T3SPI class as SPI_MASTER
T3SPI SPI_MASTER;

//The number of integers per data packet
#define dataLength 256

//Initialize the arrays for outgoing data
volatile uint8_t  data[dataLength] = {};
//volatile uint16_t data[dataLength] = {};

int bytesSent = 0;
int s;
uint8_t checksum = 0;
int CHIPSELECT = 16;

void setup() {

  Serial.begin(115200);
  pinMode(CHIPSELECT, OUTPUT);

  //Begin SPI in MASTER (SCK pin, MOSI pin, MISO pin, CS pin, Active State)
  SPI_MASTER.begin_MASTER(SCK, MOSI, MISO, T3_CS1, CS_ActiveLOW);

  //Set the CTAR (CTARn, Frame Size, SPI Mode, Shift Mode, Clock Divider)
  SPI_MASTER.setCTAR(T3_CTAR_1, 8, T3_SPI_MODE0, LSB_FIRST, T3_SPI_CLOCK_DIV8);
  //SPI_MASTER.setCTAR(CTAR_0,16,SPI_MODE0,LSB_FIRST,SPI_CLOCK_DIV2);
}

void loop() {

  digitalWrite(CHIPSELECT, LOW);
  delayMicroseconds(100);
  //Send n number of data packets
  
  for (int j = 0; j < 255; j++) { // Last of 256 bytes will be a checksum
    //SPI.transfer(0);
    uint8_t toSend = (17*bytesSent)%255; // Arbitrary data
    checksum += toSend;
    SPI_MASTER.tx8(toSend, T3_CTAR_1, T3_CS1);
    bytesSent++;  
  }

  SPI_MASTER.tx8(checksum, T3_CTAR_1, T3_CS1);
  bytesSent++;
  
  delayMicroseconds(100);
  digitalWrite(CHIPSELECT, HIGH);

  if (bytesSent % 100000 == 0) {
    //Serial.println(x);
    Serial.println(bytesSent);
    Serial.println(millis() - s);
  }

  //delay(10);
}

