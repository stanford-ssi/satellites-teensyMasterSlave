# satellites-teensyMasterSlave

Current setup as of 9/6:

loader.py: programs platformio slave mode onto teensy and opens serial port for slave.  Building hex file occurs using platformio's serial interface, and programming uses the teensy\_loader\_cli.  Serial ports are /dev/ttyACM[01], and we use the wiringpi library to toggle gpios to hit the program pins.

port: a python script to replace your port in ~/.ssh/config with the correct port (see about a page down for details).  This assumes that your .ftpconfig file in this directory exists; just make sure the port listed in .ftpconfig matches the one in ~/.ssh/config in order for it to work.  This script is really just for convenience though (for keeping your ~/.ssh/config up to date with the pi's port, since the port changes on reboot due to ngrok)

deploy.sh: if your ssh config is up to date, this copies the entire contents of this directory up to the pi.  Of course, this will kill any changes on the pi so I recommend developing locally and uploading as opposed to developing on the pi and accidentally overwriting those changes

make: opens master mode spi and reads in commands to send from command line. Command 'l' gives a list of commands, which are basically uint16\_t buffers containing raw commands.  Command 'p' polls for reverse chip select and sends COMMAND REPORT TRACKING when the flag goes high.  When it does so, it generates the log.csv file which contains adc/incoherent/pid samples that can be plotted later

plot.py: downloads the log.csv from the pi and plots all the channels.

antagonist/: all the antagonist code, using platformio

statemachine/: all the slave mode code, using platformio

statemachine/main.cpp: contains the main loop, setup function, and debug heartbeat print messages

statemachine/mirrorDriver.cpp: Michael's blocking spi DAC writers to drive the mirrors.  Also, we currently have 3 preset buffers to set sweeps for the mirror in calibration mode.  This allows you to move the mirror in a predefined way and see the impact of that sweep on the adc values that you read from the quad cell.  See the interface control doc, COMMAND\_CALIBRATE, for ways to pick sweeps and configure the amplitude of the sweep.  If you have a configuration that you want, change the value in master.cpp, line 63.  As of this commit, the command will pick buffer #1 (which is called 'corners' in mirrorDriver.cpp), send commands 7 times per second, with an amplitude of 1/2 of the maximum.

statemachine/modules.cpp: has a list of all the modules. We initialize a single instance of each class as part of the object-oriented rewrite

statemachine/packet.cpp: this one's a doozy.  We use dma twice here.  Dma is cleared on chip select down (*Important:* we may need a millisecond or two to let this interrupt fire before sending spi). Then, dma reads 10 spi words before firing an interrupt (command packets are constant size - 10 words as of this writing).  The payload will process the command in the isr and reinitialize dma, this time with an response buffer pushing to SPI\_PUSHR, going to MISO. This processing includes switch statements for all possible command numbers.

statemachine/pidData.cpp: This guy contains the large sample buffer (2500 samples) and the small packet buffer (~400 spi words). Functions from here are called from tracking.cpp.  Essentially, whenever there is a sample, tracking will let pidData.cpp know and pidData will put them into packets and toggle reverse chip select accordingly.  The stuff here should not be critical for actually tracking -- only for reporting data back to Audacy over SPI1.

statemachine/spiMaster.cpp: Mostly code for reading the quad cell on SPI0 interrupts.  If the adc data ready line goes low then it triggers a stream of spi reads, interweaving chip selects. I've had trouble with chip select 3 (0-indexed, so the last chip select) giving bad data.  The exact configuration here works fine but if you swap around chip selects or add long blocking interrupts you may have a bad time, idk.  There is also a mode toggled by *internalInterruptAdcReading* which switches between triggers from data ready line (the proper way) versus an internal timer as a trigger (the way to use if you don't have an adc and want to test data logging)

statemachine/tests.cpp: Mostly useless (but if any of y'all are interested in testing, we would really appreciate it).  These tests all run on boot and are mostly for small internal stuff.

statemachine/tracking.cpp: Where the fun is.  If we have a new sample from spiMaster.cpp, we process it, which includes logging, incoherent detection, and mirror output.  In tracking mode, we use pid, but in calibration mode we grab mirror output values from a preset buffer.  See mirrorDriver.cpp for details.

statemachine/incoherent.cpp: Stephen's incoherent detector functions, which are called from tracking.cpp

statemachine/pid.cpp: Stephen's incoherent detector functions, which are called from tracking.cpp

SPI0: interrupt-based adc read. 4 kHz, 4 channels per read, 32 bit per channel.
- 4 independent adc -> individual mux using chip selects
- Shared capture sample pin, set to 4kHz with pwm
- SPI register read 16 bits at a time

SPI1: dma-based slave mode communications.
- Master sends fixed-length command with header/footer/checksum, slave sends variable-length response
- Chip select must cycle between commands
- Interrupts on command read and response sent

SPI2: blocking writes to the high-voltage mirror driver in main loop

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
