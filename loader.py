#!/usr/bin/python
import RPi.GPIO as GPIO
import time
import subprocess

reset = 2

def reboot(pin, platformio_base, serialNumber):
    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)
    GPIO.setup(pin,GPIO.OUT)
    GPIO.setup(reset,GPIO.OUT)
    print "turning LED on"
    GPIO.output(pin,GPIO.HIGH)
    GPIO.output(reset,GPIO.HIGH)
    print "LED on"
    time.sleep(1)
    print "LED off"
    GPIO.output(pin,GPIO.LOW)
    time.sleep(1)
    GPIO.output(pin,GPIO.HIGH)
    print('-------------')
    print("Serials available after killing ", platformio_base, ":")
    subprocess.call("ls /dev/ttyACM*", shell=True)
    print('-------------')
    time.sleep(1)
    subprocess.call("cd " + platformio_base + " && platformio run && cd ../ && ./teensy_loader_cli --mcu=TEENSY36 " + platformio_base + ".pioenvs/teensy36/firmware.hex", shell=True)
    time.sleep(1)
    #subprocess.call("g++ spitest.cpp -std=c++11 -lwiringPi -o spitest", shell=True) # compile master mode

#reboot(2, "teensyMaster/", 0) #master
reboot(3, "stateMachine/", 1)
print("Serials available:")
subprocess.call("ls /dev/ttyACM*", shell=True)
subprocess.call("g++ master.cpp -std=c++11 -lwiringPi -o master && ./master", shell=True) # compile master mode
