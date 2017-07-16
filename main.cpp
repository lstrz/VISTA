#include <chrono>
#include <iostream>
#include <libusb-1.0/libusb.h>
#include <csignal>
#include <gflags/gflags.h>
#include <memory>
#include <unistd.h>

#include "AGCMonitor.h"
#include "RocketInterfaceMonitor.h"

DEFINE_string(logname, "rec",
              "Name to give to the device, will be used as prefix for generated files and tag in logs.");

static bool ValidateDevMode(const char *flagname, int32_t devmode) {
    if (devmode >= 1 && devmode <= 8) {   // devmode is ok.
        return true;
    }
    printf("Invalid value for --%s: %d\n", flagname, devmode);
    return false;
}

DEFINE_int32(devmode, 8,
             "Mode used to set up the frontend (please refer to http://ccar.colorado.edu/gnss).");
DEFINE_validator(devmode, ValidateDevMode);

volatile bool stop_signal_caught = false;

//this function is called when SIGINT signal is received to clean up all dynamics allocations and terminate properly.
//SIGINT can be received when Ctrl+C in a terminal
void SIG_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM)
        stop_signal_caught = true;
}

namespace {
    constexpr int64_t kRecordDurationSeconds = 60 * 60 * 2 + 60 * 30;
}

int main(int argc, char *argv[]) {
    usleep(10000000);
    if (!devmode_validator_registered) {
        std::cerr << "There was a problem with gflags. Exiting." << std::endl;
        exit(1);
    }
    ///Deals with all the options
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    ///Signal handling SIGINT = Ctrl+C and SIGTERM is the default signal sent by KILL linux command, both will stop properly the programme
    signal(SIGINT, SIG_handler);
    signal(SIGTERM, SIG_handler);
    signal(SIGPIPE, SIG_IGN);

    ///Read args
    //read
    std::string logname = FLAGS_logname;
    uint64_t devmode = FLAGS_devmode;

    //Looking for device, here we take the last device that correspond to
    // our PID/VID, (...)in future will consider every connected device that correspond
    libusb_init(nullptr /* context */);
    //announce the PID (easier to send a SIGNAL)
    std::cerr << time(nullptr) << " Process ID = " << getpid() << std::endl;

    AGCMonitor monitor;
    RocketInterfaceMonitor rocket_monitor;

    //set all parameters using args
    monitor.SetMode(static_cast<unsigned char>(devmode));
    monitor.SetLogName(logname);

    //open device and start recording
    monitor.OpenDevice();
    monitor.StartRecording();
    rocket_monitor.SetStatus(true);

    //enter a while waiting loop
    while (!stop_signal_caught) {
        if (rocket_monitor.IsLaunched()) {
            break;
        }
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);
    }

    std::cerr << time(nullptr)  << " Detected launch." << std::endl;

    auto start = std::chrono::system_clock::now();
    while (!stop_signal_caught) {
        auto end = std::chrono::system_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(
                end - start).count() >= kRecordDurationSeconds) {
            stop_signal_caught = true;
            std::cerr << time(nullptr) << " Time's up! Quitting." << std::endl;
        }
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(2s);
    }

    ///Terminate properly
    monitor.StopRecording();
    monitor.CloseDevice();

    libusb_exit(nullptr /* context */);
    exit(0);
}
