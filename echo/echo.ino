// Reads in a byte, then writes it back.

#include <t3spi.h>

//Initialize T3SPI class as SPI_SLAVE
T3SPI SPI_SLAVE;

void setup() {

  Serial.begin(115200);

  //Begin SPI in SLAVE (SCK pin, MOSI pin, MISO pin, CS pin)
  SPI_SLAVE.begin_SLAVE(SCK, MOSI, MISO, CS0);

  //Set the CTAR0_SLAVE0 (Frame Size, SPI Mode)
  //SPI_SLAVE.setCTAR_SLAVE(8, SPI_MODE0);
  SPI_SLAVE.setCTAR_SLAVE(16, SPI_MODE0);

  //Enable the SPI0 Interrupt
  NVIC_ENABLE_IRQ(IRQ_SPI0);

}

void loop() {

  
}


//Interrupt Service Routine to handle incoming data
void spi0_isr(void) {

  //Function to handle data
  uint16_t peeked = SPI_SLAVE.peek16();
  SPI_SLAVE.tx16 (peeked); // Echo previous byte
  /*if (peeked != received) {
    Serial.println("Result of peek and read are different!");
    Serial.printf("Peek: %d, read, %d\n", peeked, received);
  }*/
}
