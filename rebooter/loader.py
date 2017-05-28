import RPi.GPIO as GPIO
import time
import subprocess

def reboot(pin, platformio_base, serialNumber):
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
	#subprocess.call("./teensy_loader_cli --mcu=TEENSY36 firmware.hex", shell=True)
	subprocess.call("cd " + platformio_base + " && platformio run && cd ../rebooter/ && ./teensy_loader_cli --mcu=TEENSY36 " + platformio_base + ".pioenvs/teensy36/firmware.hex", shell=True)
	time.sleep(1)
	#subprocess.call("screen -L /dev/ttyACM" + str(serialNumber) + " &")# + " > " + "ACM" + str(serialNumber) + ".txt &", shell=True)

reboot(2, "../teensyMaster/", 0) #master
reboot(3, "../stateMachine/", 1)
#reboot(3, "../teensyMaster/") #master
print("Serials available:")
subprocess.call("ls /dev/ttyACM*", shell=True)
#subprocess.call("screen -L /dev/ttyACM" + str(serialNumber) + " &")# + " > " + "ACM" + str(serialNumber) + ".txt &", shell=True)
