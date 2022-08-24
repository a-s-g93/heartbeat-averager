ECG Signal Averager

this program identifies heartbeats utilizing the Pan-Tompkins algorithm and averages to reveal a clear QRS complex



Requires 2 nucleos
nucleos must be docked on provided BME463 board
SD card containing ECG signals must be inserted into SD card reader connected to BME463 board



SENDER NUCLEO
connected to SD card reader
contains SA_SENDER.cpp code modified for MIT-BIH Noise Stress Test Database

RECEIVER NUCLEO
contains signal averager code [SA_RECEIVER.cpp]



OUTPUTS
A3: raw signal
A4: averaged signal
D13: heartbeat detection pulse


SELECTING MAX EPOCHS (n)
max_epochs is assigned on line 116 of SA_RECEIVER.cpp
enter desired number of epochs to be averaged
compile SA_RECEIVER.cpp and load onto receiver nucleo

max_epochs default = 32 epochs 

n               :   4   16  32  64  100 
SNR improvement :   2   4   5.7 8   10



SELECTING ECG SIGNAL
connect SA_SENDER nucleo to putty interface
select either 118e00.txt [1:1 SNR] or 118e12.txt [12:1 SNR]
enter the provided sample intervals for clean or noise



TEST CASE
118e00/12.txt  clean   0 to 100,000          averaged signal will be nearly identical 
118e12.txt     noise   108,322 to 151,648    averaged signal will contain clear QRS complex most of the time. rerun may be required.  


METRICS
improvement in SNR was visually confirmed and compared between multiple trials of SNR 1:1, 6:1, 12:1


