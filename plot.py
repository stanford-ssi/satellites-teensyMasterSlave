import subprocess
import matplotlib.pyplot as plt
import numpy as np
import argparse
import numpy

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
parser.add_argument('--download', action='store_true')
parser.add_argument('--num', dest='num_plot', type=int)
print(parser.parse_args())

args = parser.parse_args()
if args.download:
    subprocess.call("scp pi:satellites-teensyMasterSlave/log.csv .", shell=True)
csv_string = get_first_n_lines(open("log.csv"), args)
csv_string = np.array([[int(x) for x in line.split(',')] for line in csv_string])
print("Read {} samples".format(csv_string.shape[0]))
n = csv_string[:, 0].shape[0]
x = np.arange(n) / 4000.
for i in range(0, 4):
    print("Axis", i)
    plt.plot(csv_string[:, i], label="ADC")
    plt.plot(csv_string[:, 4 + i], label="INCOHERENT")
    plt.plot(csv_string[:, 8 + i], label="PID")
    plt.legend()
    plt.show()
    fft = np.fft.fft(csv_string[:, i])
    fft[0] = 0
    fftfreq = np.fft.fftfreq(csv_string.shape[0], d = 1.0/4000.)
    plt.plot(fftfreq, fft, label='fft')
    plt.legend()
    plt.show()
print("ADC")
# for i in range(12):
    # print(i, np.count_nonzero(csv_string[:, i]))
a = csv_string[:, 2]
b = csv_string[:, 3]
c = csv_string[:, 0]
d = csv_string[:, 1]
plt.plot(a + d - b - c, label='x')
plt.plot(a + b - c - d, label='y')
plt.plot(csv_string[:, 0], label='0')
plt.plot(csv_string[:, 1], label='1')
plt.plot(csv_string[:, 2], label='2')
plt.plot(csv_string[:, 3], label='3')
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
plt.legend()
plt.show()

