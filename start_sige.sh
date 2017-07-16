#!/bin/sh

gpio -g mode 6 out
gpio -g write 6 1
gpio -g mode 13 out
gpio -g write 13 1
gpio -g mode 19 in
gpio -g mode 26 in

gpio export 6 out
gpio export 13 out
gpio export 19 down
gpio export 26 down

nohup ./SiGeDumperLite-wiringPi --devmode 6 >sdl.log 2>&1 &
echo PID=$!
