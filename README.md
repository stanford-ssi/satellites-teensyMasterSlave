# satellites-teensyMasterSlave

Current setup as of 4/14:  

Fixed bugs in existing library (https://github.com/btmcmahan/Teensy-3.0-SPI-Master---Slave) and configured it to work with our Teensies (works for 3.6 SPI).  

Known issues:  

* Data sent by master is not in the same order as data seen by the slave
* Packet counts are off
