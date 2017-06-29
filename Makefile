all: master
	./master | tee masterLog.txt

install: master
	./master

master: master.cpp
	g++ master.cpp -std=c++11 -lwiringPi -o master
