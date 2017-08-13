#!/usr/bin/python
import RPi.GPIO as GPIO
import time
import subprocess

reset_master = 21
program_master = 2
program_slave = 3

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(reset_master,GPIO.OUT)
GPIO.setup(program_master,GPIO.OUT)
GPIO.setup(program_slave,GPIO.OUT)

def cycle(pin):
    GPIO.output(pin, GPIO.HIGH)
    GPIO.output(pin, GPIO.LOW)
    time.sleep(0.5)
    GPIO.output(pin, GPIO.HIGH)
    time.sleep(1)

def showSerial():
    print('-------------')
    print("Serials available:")
    subprocess.call("ls /dev/ttyACM*", shell=True)
    print('-------------')

def program(platformio_base, mcu):
    time.sleep(1)
    command = "cd " + platformio_base + " && platformio run && cd ../ && ./teensy_loader_cli --mcu=" + mcu + " " + platformio_base + ".pioenvs/teensy36/firmware.hex"
    print(command)
    subprocess.call(command, shell=True)
    time.sleep(1)
    #subprocess.call("g++ spitest.cpp -std=c++11 -lwiringPi -o spitest", shell=True) # compile master mode

cycle(program_slave)
cycle(program_master)
cycle(reset_master)
print("Antagonist serial:")
subprocess.call("ls /dev/ttyACM*", shell=True)
program("stateMachine/", "TEENSY36")
print("Both serial:")
subprocess.call("ls /dev/ttyACM*", shell=True)
cycle(program_master)
program("antagonist/", "TEENSY32")
showSerial()
subprocess.call("./serial");
#subprocess.call("g++ master.cpp -std=c++11 -lwiringPi -o master && ./master", shell=True) # compile master mode
