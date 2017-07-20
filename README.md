# satellites-teensyMasterSlave

Current setup as of 7/19:  

loader.py: programs platformio slave mode onto teensy and opens serial port
make: opens master mode serial and reads in commands to send

SPI0: interrupt-based adc read. 4 kHz, 4 channels per read, 32 bit per channel.
- 4 independent adc -> individual mux using chip selects
- Shared capture sample pin, set to 4kHz with pwm
- SPI register read 16 bits at a time

SPI1: dma-based slave mode communications.
- Master sends fixed-length command with header/footer/checksum, slave sends variable-length response
- Chip select must cycle between commands
- Interrupts on command read and response sent

SPI2: blocking writes to mirror driver.  This should be moved to interrupts, as per SPI0

Slave can handle continuous sampling indefinitely, with some hiccups after long operation
uint32s insufficient after long-term logging (1 hour).  Payload should not be powered on for longer than a few minutes

* Known issues:

Small spi words not getting cleared on chip select low?

Interrupt speed for chip select low

Master must clock out extra spi words for response sent interrupt to fire
