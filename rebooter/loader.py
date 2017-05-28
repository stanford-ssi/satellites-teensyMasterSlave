import RPi.GPIO as GPIO
import time
import subprocess

def reboot(pin):
	GPIO.setmode(GPIO.BCM)
	GPIO.setwarnings(False)
	GPIO.setup(pin,GPIO.OUT)
	print "LED on"
	GPIO.output(pin,GPIO.HIGH)
	time.sleep(1)
	print "LED off"
	GPIO.output(pin,GPIO.LOW)
	time.sleep(1)
	GPIO.output(pin,GPIO.HIGH)
	time.sleep(1)
	subprocess.call("./teensy_loader_cli --mcu=TEENSY36 firmware.hex", shell=True)

reboot(2) #master
reboot(3)


