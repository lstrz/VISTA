# VISTA

VISTA (Vibrations Inherent System for Tracking and Analysis) is a payload to be flown on SERA-3, a sounding rocket mission flown in April 2017 from Esrange Space Center in Kiruna (Sweden), as part of the PERSEUS project. The purpose of the VISTA payload is to record the trajectory of the rocket throughout its flight using GPS and an IMU module.

Essentially, it is a battery-powered Arduino reading data from an IMU and a GPS. The data is stored on an SD carde to be post processed on ground to calculate a accurate trajectory of the rocket.

Hardware used (incomplete list):
* [Arduino Micro](https://www.arduino.cc/en/Main/ArduinoBoardMicro)
* [AltIMU-10 v4](https://www.pololu.com/product/2470) (Arduino libraries available in the link)

At the moment, the code is incomplete and supports only the listed hardware.
