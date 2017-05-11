#include "t3spi.h"

// todo: consider swapping this out with kinetis.h
#include "mk20dx128.h"
#include "core_pins.h"
#include "arduino.h"


T3SPI::T3SPI() {
	SIM_SCGC6 |= SIM_SCGC6_SPI1;	// enable clock to SPI.
	dataPointer=0;
	packetCT=0;
	ctar=0;
	delay(1000);
}

void T3SPI::clearBuffer() {
    dataPointer = 0;
}

void T3SPI::begin_MASTER() {
	setMCR(MASTER);
	setCTAR(T3_CTAR_0,8, T3_SPI_MODE0,LSB_FIRST,T3_SPI_CLOCK_DIV8);
	enablePins(SCK, MOSI, MISO, T3_CS0, CS_ActiveLOW);
}

void T3SPI::begin_MASTER(uint8_t sck, uint8_t mosi, uint8_t miso, uint8_t cs, bool activeState){
	setMCR(MASTER);
	setCTAR(T3_CTAR_0,8, T3_SPI_MODE0,LSB_FIRST, T3_SPI_CLOCK_DIV8);
	enablePins(sck, mosi, miso, cs, activeState);
}

void T3SPI::begin_SLAVE() {
	setMCR(SLAVE);
	setCTAR_SLAVE(8, T3_SPI_MODE0);
	SPI1_RSER = 0x00020000;
	enablePins_SLAVE(SCK, MOSI, MISO, T3_CS0);
}

void T3SPI::begin_SLAVE(uint8_t sck, uint8_t mosi, uint8_t miso, uint8_t cs) {
	setMCR(SLAVE);
	setCTAR_SLAVE(8, T3_SPI_MODE0);
	SPI1_RSER = 0x00020000;
	enablePins_SLAVE(sck, mosi, miso, cs);
}

void T3SPI::setMCR(bool mode){
	stop();
	if (mode==1){
		SPI1_MCR=0x80000000;}
	else{
		SPI1_MCR=0x00000000;}
	start();
}

void T3SPI::setCTAR(bool CTARn, uint8_t size, uint8_t dataMode, uint8_t bo, uint8_t cdiv){
	if (CTARn == 0){
		SPI1_CTAR0=0;}
	if (CTARn == 1){
		SPI1_CTAR1=0;}
	setFrameSize(CTARn, (size - 1));
	setMode(CTARn, dataMode);
	setBitOrder(CTARn, bo);
	setClockDivider(CTARn, cdiv);
}

void T3SPI::setCTAR_SLAVE(uint8_t size, uint8_t dataMode){
	SPI1_CTAR0_SLAVE=0;
	setFrameSize(T3_CTAR_SLAVE, (size - 1));
	setMode(T3_CTAR_SLAVE, dataMode);
}

void T3SPI::enablePins(uint8_t sck, uint8_t mosi, uint8_t miso, uint8_t cs, bool activeState){
	if (sck == 0x0D){
		CORE_PIN13_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);}	//Set Pin13 Output & SCK
	if (sck == 0x0E){
		CORE_PIN13_CONFIG = PORT_PCR_MUX(1);
		CORE_PIN14_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);}	//Set Pin14 Output & SCK
	if (mosi == 0x0B){
		CORE_PIN11_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);}	//Set Pin11 Output & MOSI
	if (mosi ==  0x07){
		CORE_PIN7_CONFIG  = PORT_PCR_DSE | PORT_PCR_MUX(2);}	//Set Pin7  Output & MOSI
	if (miso == 0x0C){
		CORE_PIN12_CONFIG = PORT_PCR_MUX(2);}					//Set Pin12 Input & MISO
	if (miso ==  0x08){
		CORE_PIN8_CONFIG  = PORT_PCR_MUX(2);}					//Set Pin8  Input & MISO
	enableCS(cs, activeState);
}

void T3SPI::enableCS(uint8_t cs, bool activeState){
	if (cs   == 0x01){
		CORE_PIN10_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);		//Set Pin10 Output & CS0
		if (activeState == 1){
			setCS_ActiveLOW(CS0_ActiveLOW);}}
	if (cs   ==  0x02){
		CORE_PIN9_CONFIG  = PORT_PCR_DSE | PORT_PCR_MUX(2);		//Set Pin9  Output & CS1
		if (activeState == 1){
			setCS_ActiveLOW(CS1_ActiveLOW);}}
	if (cs   == 0x04){
		CORE_PIN20_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);		//Set Pin20 Output & CS2
		if (activeState == 1){
			setCS_ActiveLOW(CS2_ActiveLOW);}}
	if (cs   == 0x08){
		CORE_PIN21_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);		//Set Pin21 Output & CS3
		if (activeState == 1){
			setCS_ActiveLOW(CS3_ActiveLOW);}}
	if (cs   == 0x10){
		CORE_PIN15_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);		//Set Pin15 Output & CS4
		if (activeState == 1){
			setCS_ActiveLOW(CS4_ActiveLOW);}}
	if (cs   ==  0x81){
		CORE_PIN2_CONFIG  = PORT_PCR_DSE | PORT_PCR_MUX(2);		//Set Pin2  Output & (alt) CS0
		if (activeState == 1){
			setCS_ActiveLOW(CS0_ActiveLOW);}}
	if (cs   ==  0x82){
		CORE_PIN6_CONFIG  = PORT_PCR_DSE | PORT_PCR_MUX(2);		//Set Pin6  Output & (alt) CS1
		if (activeState == 1){
			setCS_ActiveLOW(CS1_ActiveLOW);}}
	if (cs   == 0x84){
		CORE_PIN23_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);		//Set Pin23 Output & (alt) CS2
		if (activeState == 1){
			setCS_ActiveLOW(CS2_ActiveLOW);}}
	if (cs   == 0x88){
		CORE_PIN22_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);		//Set Pin22 Output & (alt) CS3
		if (activeState == 1){
			setCS_ActiveLOW(CS3_ActiveLOW);}}
}

void T3SPI::enablePins_SLAVE(uint8_t sck, uint8_t mosi, uint8_t miso, uint8_t cs) {
    if (sck == SCK){
		CORE_PIN13_CONFIG = PORT_PCR_MUX(2);}
	if (sck == ALT_SCK){
		CORE_PIN14_CONFIG = PORT_PCR_MUX(2);}
    if (sck == SCK1){
		CORE_PIN32_CONFIG = PORT_PCR_MUX(2);}
	if (mosi == MOSI){
		CORE_PIN11_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);}
	if (mosi == ALT_MOSI){
		CORE_PIN7_CONFIG  = PORT_PCR_DSE | PORT_PCR_MUX(2);}
    if (mosi == MOSI1){
		CORE_PIN0_CONFIG  = PORT_PCR_DSE | PORT_PCR_MUX(2);}
	if (miso == MISO){
		CORE_PIN12_CONFIG = PORT_PCR_MUX(2);}
	if (miso == ALT_MISO){
		CORE_PIN8_CONFIG  = PORT_PCR_MUX(2);}
    if (miso == MISO1){
		CORE_PIN1_CONFIG  = PORT_PCR_MUX(2);}
	if (cs == T3_CS0){
		CORE_PIN10_CONFIG = PORT_PCR_MUX(2);}
	if (cs == ALT_CS0){
		CORE_PIN2_CONFIG  = PORT_PCR_MUX(2);}
    if (cs == T3_SPI1_CS0){
		CORE_PIN31_CONFIG  = PORT_PCR_MUX(2);}
}

void T3SPI::setCS_ActiveLOW(uint32_t pin){
	stop();
	SPI1_MCR |= (pin);
	start();
}

void T3SPI::setFrameSize(uint8_t CTARn, uint8_t size) {
	stop();
	if (CTARn==0){
		SPI1_CTAR0 |= SPI_CTAR_FMSZ(size);}
	if (CTARn==1){
		SPI1_CTAR1 |= SPI_CTAR_FMSZ(size);}
	if (CTARn==2){
		SPI1_CTAR0_SLAVE |= SPI_CTAR_FMSZ(size);}
	start();
}
//(((size) & 31) << 27);

void T3SPI::setMode(uint8_t CTARn, uint8_t dataMode) {
	stop();
	if (CTARn==0){
		SPI1_CTAR0 = (SPI1_CTAR0 & ~(SPI_CTAR_CPOL | SPI_CTAR_CPHA)) | dataMode << 25;}
	if (CTARn==1){
		SPI1_CTAR1 = (SPI1_CTAR1 & ~(SPI_CTAR_CPOL | SPI_CTAR_CPHA)) | dataMode << 25;}
	if (CTARn==2){
		SPI1_CTAR0_SLAVE = (SPI1_CTAR0_SLAVE & ~(SPI_CTAR_CPOL | SPI_CTAR_CPHA)) | dataMode << 25;}
	start();
}

void T3SPI::setBitOrder(bool CTARn, uint8_t bo) {
	stop();
	if (CTARn==0){
		if (bo == LSBFIRST) {
			SPI1_CTAR0 |= SPI_CTAR_LSBFE;}
		if (bo == MSBFIRST) {
			SPI1_CTAR0 &= ~SPI_CTAR_LSBFE;}}
	if (CTARn==1){
		if (bo == LSBFIRST) {
			SPI1_CTAR1 |= SPI_CTAR_LSBFE;}
		if (bo == MSBFIRST) {
			SPI1_CTAR1 &= ~SPI_CTAR_LSBFE;}}
	start();
}

void T3SPI::setClockDivider(bool CTARn, uint8_t cdiv) {
	stop();
	if (CTARn==0){
		SPI1_CTAR0 |= SPI_CTAR_DBR | SPI_CTAR_CSSCK(cdiv) | SPI_CTAR_BR(cdiv);}
	if (CTARn==1){
		SPI1_CTAR1 |= SPI_CTAR_DBR | SPI_CTAR_CSSCK(cdiv) | SPI_CTAR_BR(cdiv);}
	start();
}

void T3SPI::start() {
	SPI1_MCR &= ~SPI_MCR_HALT & ~SPI_MCR_MDIS;
}

void T3SPI::stop() {
	SPI1_MCR |= SPI_MCR_HALT | SPI_MCR_MDIS;
}

void T3SPI::end() {
	SPI1_SR &= ~SPI_SR_TXRXS;
	stop();
}

//TRANSMIT PACKET OF 8 BIT DATA
void T3SPI::tx8(volatile uint8_t *dataOUT,   int length, bool CTARn, uint8_t PCS){
	ctar=CTARn;
	for (int i=0; i < length; i++){
		SPI_WRITE_8(dataOUT[i], CTARn, PCS);
		SPI_WAIT();}
	packetCT++;
}

//TRANSMIT 8 BITS - NO BUFFER
void T3SPI::tx8(uint8_t dataOUT, bool CTARn, uint8_t PCS){
	ctar=CTARn;
    SPI_WRITE_8(dataOUT, CTARn, PCS);
    SPI_WAIT();
}

//TRANSMIT PACKET OF 16 BIT DATA
void T3SPI::tx16(volatile uint16_t *dataOUT, int length, bool CTARn, uint8_t PCS){
	//ctar=CTARn;
	for (int i=0; i < length; i++){
		SPI_WRITE_16(dataOUT[i], CTARn, PCS);
		SPI_WAIT();}
	packetCT++;
}

//TRANSMIT & RECEIVE PACKET OF 8 BIT DATA
void T3SPI::txrx8(volatile uint8_t *dataOUT, volatile uint8_t *dataIN, int length, bool CTARn, uint8_t PCS){
	ctar=CTARn;
	for (int i=0; i < length; i++){
		SPI1_MCR |= SPI_MCR_CLR_RXF;
		SPI_WRITE_8(dataOUT[i], CTARn, PCS);
		SPI_WAIT();
		delayMicroseconds(1);
		dataIN[i]=SPI1_POPR;
		}
	packetCT++;
}

//TRANSMIT & RECEIVE PACKET OF 16 BIT DATA
void T3SPI::txrx16(volatile uint16_t *dataOUT, volatile uint16_t *dataIN, int length, bool CTARn, uint8_t PCS){
	ctar=CTARn;
	for (int i=0; i < length; i++){
		SPI1_MCR |= SPI_MCR_CLR_RXF;
		SPI_WRITE_16(dataOUT[i], CTARn, PCS);
		SPI_WAIT();
		delayMicroseconds(1);
		dataIN[i]=SPI1_POPR;
		}
	packetCT++;
}

void T3SPI::rx8(volatile uint8_t *dataIN, int length){
	dataIN[dataPointer] = SPI1_POPR;
	dataPointer++;
	if (dataPointer == length){
		dataPointer=0;
		packetCT++;}
	SPI1_SR |= SPI_SR_RFDF;
}

void T3SPI::rx16(volatile uint16_t *dataIN, int length){
	dataIN[dataPointer] = SPI1_POPR;
	dataPointer++;
	if (dataPointer == length){
		dataPointer=0;
		packetCT++;}
	SPI1_SR |= SPI_SR_RFDF;
}

void T3SPI::rxtx8(volatile uint8_t *dataIN, volatile uint8_t *dataOUT, int length){
	dataIN[dataPointer] = SPI1_POPR;
	dataPointer++;
	if (dataPointer == length){
		dataPointer=0;
		packetCT++;}
	SPI1_PUSHR_SLAVE = dataOUT[dataPointer];
	SPI1_SR |= SPI_SR_RFDF;
}

void T3SPI::rxtx16(volatile uint16_t *dataIN, volatile uint16_t *dataOUT, int length){
	dataIN[dataPointer] = SPI1_POPR;
	dataPointer++;
	if (dataPointer == length){
		dataPointer=0;
		packetCT++;}
  SPI1_PUSHR_SLAVE = dataOUT[dataPointer];
  SPI1_SR |= SPI_SR_RFDF;
}
/*
uint8_t T3SPI::peek8(){
    return SPI1_POPR;
}

uint16_t T3SPI::peek16(){
    return SPI1_POPR;
}
*/

// Slave mode: transmit dataOUT, ignoring SPI_POPR
void T3SPI::tx8(uint8_t dataOUT){
	SPI1_PUSHR_SLAVE = dataOUT;
	SPI1_SR |= SPI_SR_RFDF;
}

// Slave mode: transmit dataOUT, ignoring SPI_POPR
void T3SPI::tx16(uint16_t dataOUT){
  SPI1_PUSHR_SLAVE = dataOUT;
  SPI1_SR |= SPI_SR_RFDF;
}

uint8_t T3SPI::rxtx8(uint8_t dataOUT){
	uint8_t dataIn = SPI1_POPR;
    tx8(dataOUT);
    return dataIn;
}

uint16_t T3SPI::rxtx16(uint16_t dataOUT){
  uint16_t dataIn = SPI1_POPR;
  tx16(dataOUT);
  return dataIn;
}
