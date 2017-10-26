import subprocess
import matplotlib.pyplot as plt
import numpy as np
import argparse
import numpy
import paramiko
import sys


user="pi"
host="10.34.199.125"
password="dankmemes"
port=22

command= "cd satellites-teensyMasterSlave/ ; make stream"
command="echo this is a test"
client=paramiko.SSHClient()
client.load_system_host_keys()
client.set_missing_host_key_policy(paramiko.WarningPolicy)
client.connect(host, port, user, password)

stdin, stdout, stderror= client.exec_command(command)
stdin.close()
stderror.close()


csv_string=""
plot_array=np.array([[]])
for line in iter(lambda: stdout.readline(2048), "") :
    csv_string+=line
    plot_array=np.array([[int(x) for x in line.split(',')] for line in csv_string])
    plot_adc(plot_array)
    print(line)
    plt.pause(1)
    #sys.stdout.flush() #?

stdout.close()

print("Read {} samples".format(plot_array.shape[0]))
#n = plot_array[:, 0].shape[0]
#x = np.arange(n) / 4000.
# for i in range(0, 4):
    # print("Axis", i)
    # plt.plot(plot_array[:, i], label="ADC")
    # plt.plot(plot_array[:, 4 + i], label="INCOHERENT")
    # plt.plot(plot_array[:, 8 + i], label="PID")
    # plt.legend()
    # plt.show()
    # fft = np.fft.fft(plot_array[:, i])
    # fft[0] = 0
    # fftfreq = np.fft.fftfreq(plot_array.shape[0], d = 1.0/4000.)
    # plt.plot(fftfreq, fft, label='fft')
    # plt.legend()
    # plt.show()

def plot_adc(plot_array) :

    print("ADC")
    for i in range(12):
        print(i, np.count_nonzero(plot_array[:, i]))
    a = plot_array[:, 0]
    b = plot_array[:, 1]
    c = plot_array[:, 2]
    d = plot_array[:, 3]
    # plt.plot(a + d - b - c, label='x')
    # plt.plot(a + b - c - d, label='y')
    plt.plot(plot_array[:, 0], label='a')
    plt.plot(plot_array[:, 1], label='b')
    plt.plot(plot_array[:, 2], label='c')
    plt.plot(plot_array[:, 3], label='d')
    plt.legend()
    plt.show()


def plot_incoherent(plot_array) :
    print("Incoherent")
    plt.plot(plot_array[:, 4], label='0')
    plt.plot(plot_array[:, 5], label='1')
    plt.plot(plot_array[:, 6], label='2')
    plt.plot(plot_array[:, 7], label='3')
    plt.legend()
    plt.show()
def plot_pid(plot_array) :
    print("Pid")
    plt.plot(plot_array[:, 8], label='0')
    plt.plot(plot_array[:, 9], label='1')
    plt.plot(plot_array[:, 10], label='2')
    plt.plot(plot_array[:, 11], label='3')
    plt.legend()
    plt.show()

