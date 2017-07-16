# VISTA

VISTA (Vibrations Inherent System for Tracking and Analysis) is a payload launched on SERA-3, a sounding rocket mission flown in April 2017 from Esrange Space Center in Kiruna (Sweden), as part of the PERSEUS project.
The purpose of the VISTA payload was to record the trajectory of the rocket throughout its flight using GPS.

Essentially, it is a battery-powered Rasberry Pi Zero system reading data from a SiGe GPS RF module developed by Colorado Center of Astrodynamics Research and SiGe.
The data is stored on an SD card to be post processed on ground and calculate a trajectory of the rocket.

Hardware used:
 - Raspberry Pi Zero
 - SiGe GPS module

## Instructions for Ubuntu

1. Install g++

`sudo apt install g++`

2. Install gflags from the repositories

`sudo apt install libgflags-dev libgflags2v5`

3. Install libusb

`sudo apt install libusb-1.0-0-dev libusb-1.0-0`

4. Compile
Position yourself into the directory where the source code is.
```
cmake .
make
```

5. Set udev rules so that you user account has access to the SiGe module
Go to /etc/udev/rules.d/ and create a file named "sige-module.rules".
Any filename is ok, but it must end in ".rules".
Add the following line

`ATTRS{idVendor}=="1781", ATTRS{idProduct}=="0b3f", OWNER="username"`

username inside the quotation marks is replaced by your username.
If this step is skipped, run the program as root.

5. Run

`./start_sige.sh`


## Some notes about SiGe module

IF stands for intermediate frequency. IF data is the sampled IF waveform.
There is a lot of IF data! A few minutes of sampling easily generates a
gigabyte of data! For example, 16 MHz sampling, where each sample is 2 bits of
data has a throughput of 32 Mbit or 4 MB per second, if the data is packed.

AGC stands for automatic gain control. When the signal is weak, the gain must
be high. When the signal is strong, the gain doesn't have to be as high.
The AGC readout can tell us whether someone is putting a lot more power into
the spectrum (when jamming, for example), or if something is obstructing
the space between the antenna and the GPS satellite or if another scenario
occurred. The AGC data is sent out at a frequency of a little bit less than 100
samples per second (each sample a byte).

The SiGe module has a vendor id 0x1781 and the product id 0x0b3f.

It has two configurations. The default configuration 0 which all USB devices
have and configuration 1 which is used for actual operation of the module.

Configuration 1 has three interfaces, namely the command and status interface 0,
transmit interface 1 and receive interface 2. None of the interfaces has more
than a single alternative interface 0. I am not sure what are interfaces 0 and 1
used for, because the code claims and uses only interface 2.

Interface 2 has three endpoints. An IF endpoint 0x86 which is used to get the
IF data. Another input interface 0x84 which isn't used in the code and an output
"transmit" interface 0x02 which isn't used in the code. I have no idea why
they exist and what do they do. AGC data and overrun status are grabbed with
control transfers.

This info is subject to change in future SiGe revisions.
