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
