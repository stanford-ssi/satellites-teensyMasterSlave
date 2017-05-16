/*
 * Copyright (c) 2010 by Cristian Maglie <c.maglie@bug.st>
 * SPI Master library for arduino.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

/* Modifications: changed #include "SPI.h" to "ssiSPI.h"
 * I also removed unused libraries in order to keep track of the code that
 * we're using.  Some of these libraries may infinite loop, for example.
 */

#include "ssiSpi.h"
#include "pins_arduino.h"

/**********************************************************/
/*     32 bit Teensy-3.5/3.6                              */
/**********************************************************/
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)

SPIClass SPI;
uint8_t SPIClass::interruptMasksUsed = 0;
uint32_t SPIClass::interruptMask[(NVIC_NUM_INTERRUPTS+31)/32];
uint32_t SPIClass::interruptSave[(NVIC_NUM_INTERRUPTS+31)/32];
#ifdef SPI_TRANSACTION_MISMATCH_LED
uint8_t SPIClass::inTransactionFlag = 0;
#endif

void SPIClass::begin()
{
	SIM_SCGC6 |= SIM_SCGC6_SPI0;
	SPI0_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
	SPI0_CTAR0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI0_CTAR1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI0_MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
	SPCR.enable_pins(); // pins managed by SPCRemulation in avr_emulation.h
}

void SPIClass::end() {
	SPCR.disable_pins();
	SPI0_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
}

void SPIClass::usingInterrupt(IRQ_NUMBER_t interruptName)
{
	uint32_t n = (uint32_t)interruptName;

	if (n >= NVIC_NUM_INTERRUPTS) return;

	//Serial.print("usingInterrupt ");
	//Serial.println(n);
	interruptMasksUsed |= (1 << (n >> 5));
	interruptMask[n >> 5] |= (1 << (n & 0x1F));
	//Serial.printf("interruptMasksUsed = %d\n", interruptMasksUsed);
	//Serial.printf("interruptMask[0] = %08X\n", interruptMask[0]);
	//Serial.printf("interruptMask[1] = %08X\n", interruptMask[1]);
	//Serial.printf("interruptMask[2] = %08X\n", interruptMask[2]);
}

void SPIClass::notUsingInterrupt(IRQ_NUMBER_t interruptName)
{
	uint32_t n = (uint32_t)interruptName;
	if (n >= NVIC_NUM_INTERRUPTS) return;
	interruptMask[n >> 5] &= ~(1 << (n & 0x1F));
	if (interruptMask[n >> 5] == 0) {
		interruptMasksUsed &= ~(1 << (n >> 5));
	}
}

const uint16_t SPISettings::ctar_div_table[23] = {
	2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40,
	56, 64, 96, 128, 192, 256, 384, 512, 640, 768
};
const uint32_t SPISettings::ctar_clock_table[23] = {
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_DBR | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(0) | SPI_CTAR_DBR | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(2) | SPI_CTAR_BR(0) | SPI_CTAR_DBR | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1),
	SPI_CTAR_PBR(2) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2),
	SPI_CTAR_PBR(2) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(4) | SPI_CTAR_CSSCK(3),
	SPI_CTAR_PBR(2) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2),
	SPI_CTAR_PBR(3) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(5) | SPI_CTAR_CSSCK(4),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(5) | SPI_CTAR_CSSCK(4),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(6) | SPI_CTAR_CSSCK(5),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(6) | SPI_CTAR_CSSCK(5),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(7) | SPI_CTAR_CSSCK(6),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(7) | SPI_CTAR_CSSCK(6),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(8) | SPI_CTAR_CSSCK(7),
	SPI_CTAR_PBR(2) | SPI_CTAR_BR(7) | SPI_CTAR_CSSCK(6),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(8) | SPI_CTAR_CSSCK(7)
};

static void updateCTAR(uint32_t ctar)
{
	if (SPI0_CTAR0 != ctar) {
		uint32_t mcr = SPI0_MCR;
		if (mcr & SPI_MCR_MDIS) {
			SPI0_CTAR0 = ctar;
			SPI0_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
		} else {
			SPI0_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
			SPI0_CTAR0 = ctar;
			SPI0_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
			SPI0_MCR = mcr;
		}
	}
}

void SPIClass::setBitOrder(uint8_t bitOrder)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI0;
	uint32_t ctar = SPI0_CTAR0;
	if (bitOrder == LSBFIRST) {
		ctar |= SPI_CTAR_LSBFE;
	} else {
		ctar &= ~SPI_CTAR_LSBFE;
	}
	updateCTAR(ctar);
}

void SPIClass::setDataMode(uint8_t dataMode)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI0;

	// TODO: implement with native code

	SPCR = (SPCR & ~SPI_MODE_MASK) | dataMode;
}

void SPIClass::setClockDivider_noInline(uint32_t clk)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI0;
	uint32_t ctar = SPI0_CTAR0;
	ctar &= (SPI_CTAR_CPOL | SPI_CTAR_CPHA | SPI_CTAR_LSBFE);
	if (ctar & SPI_CTAR_CPHA) {
		clk = (clk & 0xFFFF0FFF) | ((clk & 0xF000) >> 4);
	}
	ctar |= clk;
	updateCTAR(ctar);
}

uint8_t SPIClass::pinIsChipSelect(uint8_t pin)
{
	switch (pin) {
	  case 10: return 0x01; // PTC4
	  case 2:  return 0x01; // PTD0
	  case 9:  return 0x02; // PTC3
	  case 6:  return 0x02; // PTD4
	  case 20: return 0x04; // PTD5
	  case 23: return 0x04; // PTC2
	  case 21: return 0x08; // PTD6
	  case 22: return 0x08; // PTC1
	  case 15: return 0x10; // PTC0
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
	  case 26: return 0x01;
	  case 45: return 0x20; // CS5
#endif
	}

    return 0;
}

bool SPIClass::pinIsChipSelect(uint8_t pin1, uint8_t pin2)
{
	uint8_t pin1_mask, pin2_mask;
	if ((pin1_mask = (uint8_t)pinIsChipSelect(pin1)) == 0) return false;
	if ((pin2_mask = (uint8_t)pinIsChipSelect(pin2)) == 0) return false;
	//Serial.printf("pinIsChipSelect %d %d %x %x\n\r", pin1, pin2, pin1_mask, pin2_mask);
	if ((pin1_mask & pin2_mask) != 0) return false;
	return true;
}

// setCS() is not intended for use from normal Arduino programs/sketches.
uint8_t SPIClass::setCS(uint8_t pin)
{
	switch (pin) {
	  case 10: CORE_PIN10_CONFIG = PORT_PCR_MUX(2); return 0x01; // PTC4
	  case 2:  CORE_PIN2_CONFIG  = PORT_PCR_MUX(2); return 0x01; // PTD0
	  case 9:  CORE_PIN9_CONFIG  = PORT_PCR_MUX(2); return 0x02; // PTC3
	  case 6:  CORE_PIN6_CONFIG  = PORT_PCR_MUX(2); return 0x02; // PTD4
	  case 20: CORE_PIN20_CONFIG = PORT_PCR_MUX(2); return 0x04; // PTD5
	  case 23: CORE_PIN23_CONFIG = PORT_PCR_MUX(2); return 0x04; // PTC2
	  case 21: CORE_PIN21_CONFIG = PORT_PCR_MUX(2); return 0x08; // PTD6
	  case 22: CORE_PIN22_CONFIG = PORT_PCR_MUX(2); return 0x08; // PTC1
	  case 15: CORE_PIN15_CONFIG = PORT_PCR_MUX(2); return 0x10; // PTC0
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
	  case 26: CORE_PIN26_CONFIG = PORT_PCR_MUX(2);return 0x01;
	  case 45: CORE_PIN45_CONFIG = PORT_PCR_MUX(3);return 0x20;
#endif
	}
	return 0;
}
/*
SPI1Class SPI1;

uint8_t SPI1Class::interruptMasksUsed = 0;
uint32_t SPI1Class::interruptMask[(NVIC_NUM_INTERRUPTS+31)/32];
uint32_t SPI1Class::interruptSave[(NVIC_NUM_INTERRUPTS+31)/32];
#ifdef SPI_TRANSACTION_MISMATCH_LED
uint8_t SPI1Class::inTransactionFlag = 0;
#endif

void SPI1Class::begin()
{
	SIM_SCGC6 |= SIM_SCGC6_SPI1;
	SPI1_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
	SPI1_CTAR0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI1_CTAR1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI1_MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
	SPCR1.enable_pins(); // pins managed by SPCRemulation in avr_emulation.h
}

void SPI1Class::end() {
	SPCR1.disable_pins();
	SPI1_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
}

void SPI1Class::usingInterrupt(IRQ_NUMBER_t interruptName)
{
	uint32_t n = (uint32_t)interruptName;

	if (n >= NVIC_NUM_INTERRUPTS) return;

	//Serial.print("usingInterrupt ");
	//Serial.println(n);
	interruptMasksUsed |= (1 << (n >> 5));
	interruptMask[n >> 5] |= (1 << (n & 0x1F));
	//Serial.printf("interruptMasksUsed = %d\n", interruptMasksUsed);
	//Serial.printf("interruptMask[0] = %08X\n", interruptMask[0]);
	//Serial.printf("interruptMask[1] = %08X\n", interruptMask[1]);
	//Serial.printf("interruptMask[2] = %08X\n", interruptMask[2]);
}

void SPI1Class::notUsingInterrupt(IRQ_NUMBER_t interruptName)
{
	uint32_t n = (uint32_t)interruptName;
	if (n >= NVIC_NUM_INTERRUPTS) return;
	interruptMask[n >> 5] &= ~(1 << (n & 0x1F));
	if (interruptMask[n >> 5] == 0) {
		interruptMasksUsed &= ~(1 << (n >> 5));
	}
}


static void updateCTAR1(uint32_t ctar)
{
	if (SPI1_CTAR0 != ctar) {
		uint32_t mcr = SPI1_MCR;
		if (mcr & SPI_MCR_MDIS) {
			SPI1_CTAR0 = ctar;
			SPI1_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
		} else {
			SPI1_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
			SPI1_CTAR0 = ctar;
			SPI1_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
			SPI1_MCR = mcr;
		}
	}
}

void SPI1Class::setBitOrder(uint8_t bitOrder)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI1;
	uint32_t ctar = SPI1_CTAR0;
	if (bitOrder == LSBFIRST) {
		ctar |= SPI_CTAR_LSBFE;
	} else {
		ctar &= ~SPI_CTAR_LSBFE;
	}
	updateCTAR1(ctar);
}

void SPI1Class::setDataMode(uint8_t dataMode)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI1;

	// TODO: implement with native code
	SPCR1 = (SPCR1 & ~SPI_MODE_MASK) | dataMode;
}

void SPI1Class::setClockDivider_noInline(uint32_t clk)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI1;
	uint32_t ctar = SPI1_CTAR0;
	ctar &= (SPI_CTAR_CPOL | SPI_CTAR_CPHA | SPI_CTAR_LSBFE);
	if (ctar & SPI_CTAR_CPHA) {
		clk = (clk & 0xFFFF0FFF) | ((clk & 0xF000) >> 4);
	}
	ctar |= clk;
	updateCTAR1(ctar);
}

uint8_t SPI1Class::pinIsChipSelect(uint8_t pin)
{
	switch (pin) {
	  case 6:  return 0x01; // CS0
	  case 31: return 0x01; // CS0
	  case 58: return 0x02;	//CS1
	  case 62: return 0x01;	//CS0
	  case 63: return 0x04;	//CS2
	}
	return 0;
}

bool SPI1Class::pinIsChipSelect(uint8_t pin1, uint8_t pin2)
{
	uint8_t pin1_mask, pin2_mask;
	if ((pin1_mask = (uint8_t)pinIsChipSelect(pin1)) == 0) return false;
	if ((pin2_mask = (uint8_t)pinIsChipSelect(pin2)) == 0) return false;
	//Serial.printf("pinIsChipSelect %d %d %x %x\n\r", pin1, pin2, pin1_mask, pin2_mask);
	if ((pin1_mask & pin2_mask) != 0) return false;
	return true;
}

// setCS() is not intended for use from normal Arduino programs/sketches.
uint8_t SPI1Class::setCS(uint8_t pin)
{
	switch (pin) {
	  case 6:  CORE_PIN6_CONFIG  = PORT_PCR_MUX(7); return 0x01; // PTD4
	  case 31: CORE_PIN31_CONFIG = PORT_PCR_MUX(2); return 0x01; // PTD5
	  case 58: CORE_PIN58_CONFIG = PORT_PCR_MUX(2); return 0x02;	//CS1
	  case 62: CORE_PIN62_CONFIG = PORT_PCR_MUX(2); return 0x01;	//CS0
	  case 63: CORE_PIN63_CONFIG = PORT_PCR_MUX(2); return 0x04;	//CS2
	}
	return 0;
}*/


//  SPI2 Class
SPI2Class SPI2;

uint8_t SPI2Class::interruptMasksUsed = 0;
uint32_t SPI2Class::interruptMask[(NVIC_NUM_INTERRUPTS+31)/32];
uint32_t SPI2Class::interruptSave[(NVIC_NUM_INTERRUPTS+31)/32];
#ifdef SPI_TRANSACTION_MISMATCH_LED
uint8_t SPI2Class::inTransactionFlag = 0;
#endif

void SPI2Class::begin()
{
	SIM_SCGC3 |= SIM_SCGC3_SPI2;
	SPI2_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
	SPI2_CTAR0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI2_CTAR1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI2_MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
	SPCR2.enable_pins(); // pins managed by SPCRemulation in avr_emulation.h
}

void SPI2Class::end() {
	SPCR2.disable_pins();
	SPI2_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
}

void SPI2Class::usingInterrupt(IRQ_NUMBER_t interruptName)
{
	uint32_t n = (uint32_t)interruptName;

	if (n >= NVIC_NUM_INTERRUPTS) return;

	//Serial.print("usingInterrupt ");
	//Serial.println(n);
	interruptMasksUsed |= (1 << (n >> 5));
	interruptMask[n >> 5] |= (1 << (n & 0x1F));
	//Serial.printf("interruptMasksUsed = %d\n", interruptMasksUsed);
	//Serial.printf("interruptMask[0] = %08X\n", interruptMask[0]);
	//Serial.printf("interruptMask[1] = %08X\n", interruptMask[1]);
	//Serial.printf("interruptMask[2] = %08X\n", interruptMask[2]);
}

void SPI2Class::notUsingInterrupt(IRQ_NUMBER_t interruptName)
{
	uint32_t n = (uint32_t)interruptName;
	if (n >= NVIC_NUM_INTERRUPTS) return;
	interruptMask[n >> 5] &= ~(1 << (n & 0x1F));
	if (interruptMask[n >> 5] == 0) {
		interruptMasksUsed &= ~(1 << (n >> 5));
	}
}


static void updateCTAR2(uint32_t ctar)
{
	if (SPI2_CTAR0 != ctar) {
		uint32_t mcr = SPI2_MCR;
		if (mcr & SPI_MCR_MDIS) {
			SPI2_CTAR0 = ctar;
			SPI2_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
		} else {
			SPI2_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
			SPI2_CTAR0 = ctar;
			SPI2_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
			SPI2_MCR = mcr;
		}
	}
}

void SPI2Class::setBitOrder(uint8_t bitOrder)
{
	SIM_SCGC3 |= SIM_SCGC3_SPI2;
	uint32_t ctar = SPI2_CTAR0;
	if (bitOrder == LSBFIRST) {
		ctar |= SPI_CTAR_LSBFE;
	} else {
		ctar &= ~SPI_CTAR_LSBFE;
	}
	updateCTAR2(ctar);
}

void SPI2Class::setDataMode(uint8_t dataMode)
{
	SIM_SCGC3 |= SIM_SCGC3_SPI2;

	// TODO: implement with native code
	SPCR2 = (SPCR2 & ~SPI_MODE_MASK) | dataMode;
}

void SPI2Class::setClockDivider_noInline(uint32_t clk)
{
	SIM_SCGC3 |= SIM_SCGC3_SPI2;
	uint32_t ctar = SPI2_CTAR0;
	ctar &= (SPI_CTAR_CPOL | SPI_CTAR_CPHA | SPI_CTAR_LSBFE);
	if (ctar & SPI_CTAR_CPHA) {
		clk = (clk & 0xFFFF0FFF) | ((clk & 0xF000) >> 4);
	}
	ctar |= clk;
	updateCTAR2(ctar);
}

uint8_t SPI2Class::pinIsChipSelect(uint8_t pin)
{
	switch (pin) {
	  case 43:  return 0x01; // CS0
	  case 54: return 0x02; // CS1
	  case 55:  return 0x01; // CS0
	}
	return 0;
}

bool SPI2Class::pinIsChipSelect(uint8_t pin1, uint8_t pin2)
{
	uint8_t pin1_mask, pin2_mask;
	if ((pin1_mask = (uint8_t)pinIsChipSelect(pin1)) == 0) return false;
	if ((pin2_mask = (uint8_t)pinIsChipSelect(pin2)) == 0) return false;
	//Serial.printf("pinIsChipSelect %d %d %x %x\n\r", pin1, pin2, pin1_mask, pin2_mask);
	if ((pin1_mask & pin2_mask) != 0) return false;
	return true;
}

// setCS() is not intended for use from normal Arduino programs/sketches.
uint8_t SPI2Class::setCS(uint8_t pin)
{
	switch (pin) {
	  case 43: CORE_PIN43_CONFIG = PORT_PCR_MUX(2); return 0x01; // CS0
	  case 54: CORE_PIN54_CONFIG = PORT_PCR_MUX(2); return 0x02; // CS1
	  case 55: CORE_PIN55_CONFIG = PORT_PCR_MUX(2); return 0x01; // CS0
	}
	return 0;
}

#endif
