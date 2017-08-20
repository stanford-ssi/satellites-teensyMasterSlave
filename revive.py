#!/usr/bin/python
import RPi.GPIO as GPIO
import time
import subprocess

reset_master = 4

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(reset_master,GPIO.OUT)

def showSerial():
    print('-------------')
    print("Serials available:")
    subprocess.call("ls /dev/ttyACM*", shell=True)
    print('-------------')

GPIO.output(reset_master, GPIO.HIGH) # No power to FET - antagonist
time.sleep(0.5)
showSerial()
