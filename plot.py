import subprocess
import matplotlib.pyplot as plt
import numpy as np
import argparse
import numpy
import math
import time

def get_first_n_lines(f, args):
    if args.num_plot is None:
        return f.readlines()
    lines = []
    num_lines = 0
    while True:
        try:
            lines.append(f.readline())
            num_lines += 1
            if num_lines >= args.num_plot:
                break
        except Exception as e:
            print(e)
    return lines

parser = argparse.ArgumentParser()
parser.add_argument('--test', action='store_true')
parser.add_argument('--download', action='store_true')
parser.add_argument('--num', dest='num_plot', type=int)
print(parser.parse_args())
args = parser.parse_args()
if args.download:
    cmd = 'ssh pi "cd satellites-teensyMasterSlave && python loader.py"'
    loaderProcess = subprocess.Popen(cmd, shell=True)
    time.sleep(10)
    #cmd = 'ssh pi "cd satellites-teensyMasterSlave && ./serial 1"'
    #loaderProcess = subprocess.Popen(cmd, shell=True)
    #time.sleep(3)
    loaderProcess.terminate()
    subprocess.call("scp pi:satellites-teensyMasterSlave/log.csv .", shell=True)
csv_string = get_first_n_lines(open("log.csv"), args)
csv_string = np.array([[int(x) for x in line.split(',')] for line in csv_string])
print("Read {} samples".format(csv_string.shape[0]))
n = csv_string[:, 0].shape[0]
x = np.arange(n) / 4000.
# for i in range(0, 4):
    # print("Axis", i)
    # plt.plot(csv_string[:, i], label="ADC")
    # plt.plot(csv_string[:, 4 + i], label="INCOHERENT")
    # plt.plot(csv_string[:, 8 + i], label="PID")
    # plt.legend()
    # plt.show()
    # fft = np.fft.fft(csv_string[:, i])
    # fft[0] = 0
    # fftfreq = np.fft.fftfreq(csv_string.shape[0], d = 1.0/4000.)
    # plt.plot(fftfreq, fft, label='fft')
    # plt.legend()
    # plt.show()
print("ADC")
for i in range(12):
    print(i, np.count_nonzero(csv_string[:, i]))
a = csv_string[:, 0]
b = csv_string[:, 1]
c = csv_string[:, 2]
d = csv_string[:, 3]
x = csv_string[:, 0] + csv_string[:, 3] - csv_string[:, 1] - csv_string[:, 2]
y = csv_string[:, 0] + csv_string[:, 1] - csv_string[:, 2] - csv_string[:, 3]
dist = x * x + y * y
plt.plot(dist, label='distance loss')
plt.legend()
plt.show()
# plt.plot(a + d - b - c, label='x')
# plt.plot(a + b - c - d, label='y')
plt.plot(csv_string[:, 0], label='a')
plt.plot(csv_string[:, 1], label='b')
plt.plot(csv_string[:, 2], label='c')
plt.plot(csv_string[:, 3], label='d')
plt.legend()
plt.show()
print("Incoherent")
plt.plot(csv_string[:, 4], label='0')
plt.plot(csv_string[:, 5], label='1')
plt.plot(csv_string[:, 6], label='2')
plt.plot(csv_string[:, 7], label='3')
plt.legend()
plt.show()
print("Pid")
plt.plot(csv_string[:, 8], label='0')
plt.plot(csv_string[:, 9], label='1')
plt.plot(csv_string[:, 10], label='2')
plt.plot(csv_string[:, 11], label='3')
plt.plot()
plt.legend()
plt.show()


