#include "RocketInterfaceMonitor.h"
#include <wiringPi.h>

RocketInterfaceMonitor::RocketInterfaceMonitor() {
    wiringPiSetupSys();
    SetStatus(false);
    digitalWrite(kPinLOb, 1);
}

void RocketInterfaceMonitor::SetStatus(bool status) {
    digitalWrite(kPinStatusPlus, !status);
}

bool RocketInterfaceMonitor::IsLaunched() {
    return !digitalRead(kPinLOa) || digitalRead(kPinIgnitPlus);
}