#pragma once

#include "Semaphore.h"

#include <libusb-1.0/libusb.h>
#include <queue>
#include <thread>

// The AGCMonitor, once constructed, configured and started will have four
// threads working.
//
// WriteToFileThread handles the saving request and acts on them. A saving request
// will occur at the beginning and be timed for two minutes in, simply for
// diagnostic needs. Other than the first one, a saving request happens whenever
// the AGC goes below a certain predetermined threshold. That saving request
// is used to see what the GPS signal and the AGC looked like before and after
// the triggering event, to help determined where all the "extra" power in the
// spectrum came from. Saving refers to writing the past X milliseconds of
// IF data and AGC data to a file. X is given as a command line argument or the
// default is used.
//
// AGCAndOverrunThread periodically collects the AGC data (gain strength) from
// the SiGe module and checks if the USB buffer on the SiGe module was overran.
// It uses synchronous/blocking USB transfers when getting the required info,
// because low throughput is needed. The AGC data is stored in the AGC circular
// buffer which is ready to be written to a file on a saving request.
//
// AsyncUSBThread handles asynchronous USB transfers (a lot of IF data).
// Asynchronous USB transfers are used because there is a lot of data
// that needs to be transferred, so doing it synchronously/blocking would be
// too slow and the SiGe module would have its buffers overran. The actual
// callback function to handle the completed transfer events is outside of the
// AGCMonitor class, therefore whenever a transfer is collected, the IF data
// is passed on to an AGCMonitor instance and saved in a queue to be processed.
//
// IFPackingThread processes IF data from a a queue that the AsyncUSBThread has put it
// in. Processing includes packing a few samples into a byte (because each
// sample is two bits) and putting it in the IF circular buffer which is ready
// to be written to a file on a saving request.
//
//
// All the threads use blocking mechanisms such as semaphores, mutexes or timers
// to achieve thread safety and relatively low CPU usage. So many threads exist
// primarily because low latency is needed when handling the completed USB
// transfers -- no time to wait for other blocking USB transfers (AGC and
// overrun status) or to wait for file I/O to finish.

class AGCMonitor {

public:
    AGCMonitor();

    ~AGCMonitor();

    AGCMonitor(const AGCMonitor &) = delete;

    AGCMonitor operator=(const AGCMonitor &) = delete;

    // This function is used by the "external" USB transfer callback to push the
    // provided buffer into the IF queue, for further processing.
    void PushIFBufferIntoQueue(const uint8_t *buffer, const size_t size);

    void SetMode(const unsigned char mode);

    void SetLogName(const std::string &nm);

    void OpenDevice();

    void CloseDevice();

    void StartRecording();

    void StopRecording();

    void ActionLaunch();

private:

    // Needs to be done before actually initializing the SiGe module.
    // If done afterwards, the SiGe module will have its buffers overran because
    // there is a lot of GPS data.
    void AllocateAndSubmitIFTransfers();

    void WriteAGCAndAGCTSToFileThread();

    void WriteIFToFileThread();

    void AGCAndOverrunThread();

    void AsyncUSBThread();

    void IFPackingThread();

    uint64_t ReadAGC(const uint64_t buf_size, uint16_t *buf);

    // Next four are basic functions to dialog with the SiGe's firmware.
    void GetStatus(const uint16_t which, bool *trouble);

    void
    WriteCmd(const uint8_t request, const uint16_t value, const uint16_t index,
             const uint16_t len, uint8_t *bytes);

    void USRPTransfer(const uint8_t vrq_type, const uint16_t start);

    void USRPTransfer2(const uint8_t vrq_type, const uint16_t start,
                       const uint16_t len, uint8_t *buf);

    libusb_device_handle *device_handle_;
    bool is_device_init_;
    bool is_recording_;
    volatile bool stop_request_;
    volatile bool circular_if_file_;
    std::mutex mutex_agc_agcts_queue_;
    std::mutex mutex_packed_if_queue_;
    std::mutex mutex_unpacked_if_queue_;
    Semaphore semaphore_agc_agcts_queue_;
    Semaphore semaphore_packed_if_queue_;
    Semaphore semaphore_unpacked_if_queue_;
    std::queue<std::vector<uint16_t>> AGC_queue_;
    std::queue<std::vector<int64_t>> AGCTS_queue_;
    std::queue<std::vector<uint8_t>> packed_IF_queue_;
    std::queue<std::vector<uint8_t>> unpacked_IF_queue_;
    std::thread thread_agc_and_overrun_;
    std::thread thread_write_agc_to_file_;
    std::thread thread_write_if_to_file_;
    std::thread thread_async_usb_;
    std::thread thread_if_packing_;

    std::string name_log_;
    unsigned char fw_mode_;
    unsigned char pack_mode_;
    bool is_complex_data_;
};
