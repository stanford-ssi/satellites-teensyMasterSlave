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

* Instructions
Antagonist and master/slave programming is all set up.  I recommend exclusively using the setup from now on just to have more people familiarize themselves with it.  However, if you are using it ping the chat so we don't have any conflicts.

First ssh into the pi (port is on ngrok, user ozeng@stanford.edu password ----------)

Next upload your code, slave is in stateMachine/, antagonist is at antagonist/.  These are both platformio directories

I recommend using (rsync in deploy.sh) or (git commits and atom plugin remote-ftp) to synchronize these directories.

Then, `./loader.py` in the root of the git directory will program both guys (edited)

There will be two serial ports, /dev/ttyACM0 and /dev/ttyACM1. If things work right, slave is 0 and antagonist is 1.

`./serial <x>` opens serial port `/dev/ttyACM<x>`.  `./serial` with no arguments defaults to 0 (slave).  These also generate log files 0.log and 1.log respectively.  You use this command to read the debug output of slave and to send commands to antagonist (edited)

There is a Makefile; `make` will build the master spi code which handles sending commands to the slave and reading data off of it.  The command `l` lists all the possible commands.  The most common one is 6p, which enters pointing and tracking and immediately starts polling for data off the guy.  This generated a log.csv file which contains the adc/incoherent/pid data during the pointing and tracking.  First 4 columns are adc, second 4 are incoherent, third 4 are pid (but pid will change to 2 later, 4 axes of pid dont make sense), last two are two checksums that we expect to match. (edited)

Finally, `python3 plot.py` will take a log.csv file and plot all your log.csv data over time.  This stuff is fairly pretty so I recommend taking a look at it.  `python3 plot.py --download` will download the log.csv file from the pi.  However, this requires that you have pi listed under your ssh config, which I will explain below.

I have a file `~/.ssh/config`, which contians
Host pi
    User pi
    Port 12435
    IdentitiesOnly yes
    HostName 0.tcp.ngrok.io
    IdentityFile ~/.ssh/pi
with the port replaced with the pi's ssh port.  This allows the command `ssh pi` to work and also allows `python3 plot.py --download` to work.  `./port <portnum>` will change the port in the sshconfig to <portnum> if the pi reboots (edited)
