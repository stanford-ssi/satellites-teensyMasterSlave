# satellites-teensyMasterSlave

Current setup as of 8/8:

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

SPI2: interrupt-based writes to the high-voltage mirror driver

Slave can handle continuous sampling indefinitely, with some hiccups after long operation
uint32s for telemetry overflow long-term logging (1 hour), but the payload should not be powered on for longer than a few minutes.
The overflow doesn't cause any internal error

Common functions:
- setup() functions are called once upon initialization
- enter() and leave() functions are called before entering and after exiting a state when commanded by the host
- Heartbeat() functions print internal telemetry at regular intervals (<= 1Hz) for debugging

* Known issues:

Print statements in interrupts

Small spi words not getting cleared on chip select low?

Interrupt speed for chip select low

Master must clock out extra spi words for response sent interrupt to fire
