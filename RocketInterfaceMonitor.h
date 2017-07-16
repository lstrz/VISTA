#pragma once

class RocketInterfaceMonitor {
public:
    RocketInterfaceMonitor();

    void SetStatus(bool status);

    bool IsLaunched();

private:
    static constexpr int kPinStatusPlus = 6;
    // constexpr int kPinStatusReturn = 5;
    static constexpr int kPinIgnitPlus = 26;
    // static constexpr int kPinIgnitReturn = 5;  // Connected to GND.
    static constexpr int kPinLOa = 19;
    static constexpr int kPinLOb = 13;
};
