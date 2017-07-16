#include "AGCMonitor.h"

#include <algorithm>
#include <csignal>
#include <cstring>
#include <gflags/gflags.h>
#include <iostream>
#include <fstream>
#include <sstream>

static bool
ValidateLookupTable(const char *flagname, const std::string &lookup_table_str) {
    std::stringstream ss(lookup_table_str);
    std::vector<char> lookup_table(4);
    for (unsigned i = 0; i < lookup_table.size(); ++i) {
        int num;
        ss >> num;
        lookup_table[i] = static_cast<char>(num);
    }
    std::sort(lookup_table.begin(), lookup_table.end());
    if (ss.eof() && lookup_table[0] == -3 && lookup_table[1] == -1 &&
        lookup_table[2] == 1 && lookup_table[3] == 3) {  // Lookup table ok.
        return true;
    }
    std::cerr << "--" << flagname
              << " must include numbers -3, -1, 1 and 3 once each."
              << std::endl;
    return false;
}

DEFINE_string(lookuptable, "1 3 -3 -1",
              "Lookup table to use when mapping two bits to bytes in unpacked mode. Defaults to '1 3 -3 -1'");
DEFINE_validator(lookuptable, ValidateLookupTable);
DEFINE_bool(skipagc, false, "Skips the collection of AGC data.");

#define CHECK_LIBUSB_ERR(error)                                                         \
    do{                                                                                 \
        int err = error;                                                                \
        if(err < 0) {                                                                   \
            std::cerr << libusb_strerror(static_cast<libusb_error>(err)) << std::endl;  \
            std::cerr << __FUNCTION__ << ":" << __LINE__ << ": Exit." << std::endl;     \
            raise(SIGTERM);                                                             \
        }                                                                               \
    } while(0)

#define ERROR_EXIT(msg)                                                                   \
    do{                                                                                   \
        std::cerr << std::chrono::duration_cast<std::chrono::seconds>(                    \
                     std::chrono::system_clock::now().time_since_epoch()).count();        \
        std::cerr << ":" << __FUNCTION__ << ":" << __LINE__ << ": " << msg << std::endl;  \
        raise(SIGTERM);                                                                   \
    } while(0)

namespace {
    // USB info.
    constexpr uint16_t kVendorId = 0x1781;
    constexpr uint16_t kProductId = 0x0b3f;

    // USB configurations.
    constexpr int kConfigurationZero = 0;
    constexpr int kConfiguration = 1;

    // USB interfaces.
    constexpr int kCommandAndStatusInterface = 0;
    constexpr int kTransmitInterface = 1;
    constexpr int kReceiveInterface = 2;
    constexpr int kAlternateInterface = 0;  // No alternatives to any interface.

    // USB endpoints.
    constexpr unsigned char kIFEndpoint = 0x86;
    constexpr unsigned char kAGCEndpoint = 0x84;  // Not the AGC endpoint?
    constexpr unsigned char kTransmitEndpoint = 0x02;

    // USB request types.
    constexpr uint8_t kInVendorDeviceRequestType = 0xC0;
    constexpr uint8_t kOutVendorDeviceRequestType = 0x40;

    // USB requests IN.
    constexpr uint8_t kInVendorDeviceRequestFlags = 0x90;
    constexpr uint8_t kInVendorDeviceRequestStatus = 0x80;
    constexpr uint8_t kInVendorDeviceRequestAGC = 0x88;

    // USB requests OUT.
    constexpr uint8_t kOutVendorDeviceRequestINTransfer = 0x01;
    constexpr uint8_t kOutVendorDeviceRequestOUTTransfer = 0x02;
    constexpr uint8_t kOutVendorDeviceRequestMode = 0x04;
    constexpr uint8_t kOutVendorDeviceRequestAGC = 0x08;
    constexpr uint8_t kOutVendorDeviceRequestCMode = 0x0F;

    // USB control transfers indices.
    constexpr uint16_t GS_kControlTransferIndexIsTXOverrun = 0x0000;
    constexpr uint16_t GS_kControlTransferIndexIsRXOverrun = 0x0001;

    // Transfer settings.
    // We need to have a lot of transfers queued because of the high throughput.
    // It needs to be high enough to survive any scheduling "hiccups" that the
    // OS might have. Our process might not be scheduled every time we want it
    // to be. With 256 transfers a normal priority 12h test failed, but a max
    // priority test ran ok. With 768, a normal priority 12h test ran ok.
    constexpr unsigned int kNumberOfTransfers = 768;
    // Apart from having many transfers queued, the transfers need to have a big
    // enough buffer that they don't bash the callback too often, causing a
    // higher CPU usage and increasing latency requirements somewhat.
    constexpr unsigned int kIFTransferBufferSize = 16384;
    constexpr unsigned int kAGCTransferBufferSize = 32;
    constexpr unsigned int kTimeout = 1000;  // Timeout for blocking USB transfers.
    constexpr unsigned int kBulkTransferTimeout = 0;  // No timeout for IF data.
    constexpr unsigned int kAGCReadTimeout = 200;
    // Timeout on the USB handling call. The timeout helps with reducing CPU
    // usage because the USB handling call is done in a while true loop.
    constexpr unsigned int kUSBHandleTimeout = 1000;

    constexpr int64_t kMaxCircularIFSize = 1024ll * 1024 * 1024 * 50;  // 50 GB

    // Masks
    constexpr uint8_t k2BitMask = 0x03;
    constexpr uint8_t k4BitMask = 0x0F;

    // Loookup table for unpacked mode.
    char lut[] = {0, 1, 2, 3};

    void SetLookupTable() {
        std::stringstream ss(FLAGS_lookuptable);
        for (unsigned i = 0; i < sizeof lut; ++i) {
            int num;
            ss >> num;
            lut[i] = static_cast<char>(num);
        }
    }
}  // namespace

// Callback that handles asynchronous USB transfer events (IF data).
// Simply copies the transfer's buffer and puts it into AGCMonitor's IF queue.
static void IFTransferCallback(libusb_transfer *transfer) {
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        auto time = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        std::cerr << "IF transfer error at " << time
                  << ". Transfer not completed." << std::endl;
        raise(SIGTERM);
    }

    auto actual_length = transfer->actual_length;
    if (actual_length != kIFTransferBufferSize) {
        auto time = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        std::cerr << "IF transfer error at " << time << ". Got "
                  << actual_length << " instead of "
                  << kIFTransferBufferSize
                  << " bytes." << std::endl;
        raise(SIGTERM);
    }

    AGCMonitor *monitor = (AGCMonitor *) transfer->user_data;
    monitor->PushIFBufferIntoQueue(transfer->buffer, kIFTransferBufferSize);
    CHECK_LIBUSB_ERR(libusb_submit_transfer(transfer));
}

AGCMonitor::AGCMonitor() {
    if (!lookuptable_validator_registered) {
        // Do nuthn.
    }
    // Set parameters to their default values
    SetMode(8);
    name_log_ = "data/test";

    //initialization of variables
    is_device_init_ = false;
    is_recording_ = false;
    circular_if_file_ = true;
    stop_request_ = false;
    SetLookupTable();
}

AGCMonitor::~AGCMonitor(void) {
    //make sure we have closed the device and free the dynamic memory
    StopRecording();
    CloseDevice();
}

void
AGCMonitor::PushIFBufferIntoQueue(const uint8_t *buffer, const size_t size) {
    std::vector<uint8_t> unpacked_if(buffer, buffer + size);
    mutex_unpacked_if_queue_.lock();
    unpacked_IF_queue_.push(unpacked_if);
    mutex_unpacked_if_queue_.unlock();
    semaphore_unpacked_if_queue_.notify();
}

void AGCMonitor::SetMode(const unsigned char mode) {
    switch (mode) {
        // Modes 1, 3, 5, 7 have an IF of 4.1304e6.
        // Modes 2, 4, 6, 8 have an IF of 4.092e6.
        // Lower four modes are wideband. Upper narrowband.
        case 1: //	mode 1
            fw_mode_ = 32;
            // freqsampling_ = 16367600;
            is_complex_data_ = false;
            pack_mode_ = 4;
            break;
        case 2: //	mode 2
            fw_mode_ = 36;
            // freqsampling_ = 8183800;
            is_complex_data_ = true;
            pack_mode_ = 2;
            break;
        case 3: //	mode 3
            fw_mode_ = 38;
            // freqsampling_ = 5455867;
            is_complex_data_ = false;
            pack_mode_ = 4;
            break;
        case 4: //  mode 4
            fw_mode_ = 42;
            // freqsampling_ = 4091900;
            is_complex_data_ = true;
            pack_mode_ = 2;
            break;
        case 5: //	mode 5
            fw_mode_ = 132;
            // freqsampling_ = 16367600;
            is_complex_data_ = false;
            pack_mode_ = 4;
            break;
        case 6: //	mode 6
            fw_mode_ = 136;
            // freqsampling_ = 8183800;
            is_complex_data_ = true;
            pack_mode_ = 2;
            break;
        case 7: // 	mode 7
            fw_mode_ = 138;
            // freqsampling_ = 5455867;
            is_complex_data_ = false;
            pack_mode_ = 4;
            break;
        case 8: //  mode 8
            fw_mode_ = 142;
            // freqsampling_ = 4091900;
            is_complex_data_ = true;
            pack_mode_ = 2;
            break;
        default:
            ERROR_EXIT("Invalid devmode!");
    }
    // freqagc_ = 97.5;
}

void AGCMonitor::SetLogName(const std::string &nm) {
    name_log_ = nm;
}

void AGCMonitor::OpenDevice() {
    device_handle_ = libusb_open_device_with_vid_pid(nullptr /* context */,
                                                     kVendorId, kProductId);
    if (device_handle_ == nullptr) {
        std::cerr << "No device found." << std::endl;
        exit(1);
    }
    CHECK_LIBUSB_ERR(libusb_set_configuration(device_handle_, kConfiguration));
    CHECK_LIBUSB_ERR(libusb_claim_interface(device_handle_, kReceiveInterface));
    CHECK_LIBUSB_ERR(
            libusb_set_interface_alt_setting(device_handle_, kReceiveInterface,
                                             kAlternateInterface));
    AllocateAndSubmitIFTransfers();
    is_device_init_ = true;
}

void AGCMonitor::CloseDevice(void) {
    if (is_device_init_) {
        CHECK_LIBUSB_ERR(
                libusb_release_interface(device_handle_, kReceiveInterface));
        CHECK_LIBUSB_ERR(
                libusb_set_configuration(device_handle_, kConfigurationZero));
        CHECK_LIBUSB_ERR(libusb_reset_device(device_handle_));
        libusb_close(device_handle_);
        is_device_init_ = false;
    }
}

void AGCMonitor::StartRecording() {
    if (!is_recording_) {
        unsigned char uc_flags[5];

        // Initialize frontend.
        USRPTransfer(kOutVendorDeviceRequestAGC, 1);

        USRPTransfer(kOutVendorDeviceRequestCMode, (char) 132);
        USRPTransfer(kOutVendorDeviceRequestINTransfer, 0);
        USRPTransfer(kOutVendorDeviceRequestINTransfer, 1);
        USRPTransfer2(kInVendorDeviceRequestFlags, 0, 5, uc_flags);
        USRPTransfer(kOutVendorDeviceRequestINTransfer, 0);
        USRPTransfer(kOutVendorDeviceRequestCMode, fw_mode_);
        USRPTransfer(kOutVendorDeviceRequestINTransfer, 1);
        USRPTransfer(kOutVendorDeviceRequestAGC, 2);

        USRPTransfer(kOutVendorDeviceRequestAGC, 1);
        USRPTransfer(kOutVendorDeviceRequestAGC, 2);
        USRPTransfer2(kInVendorDeviceRequestFlags, 0, 5, uc_flags);
        USRPTransfer(kOutVendorDeviceRequestAGC, 2);

        // Start all threads.
        stop_request_ = false;
        thread_agc_and_overrun_ = std::thread(&AGCMonitor::AGCAndOverrunThread,
                                              this);
        thread_write_agc_to_file_ = std::thread(
                &AGCMonitor::WriteAGCAndAGCTSToFileThread,
                this);
        thread_write_if_to_file_ = std::thread(
                &AGCMonitor::WriteIFToFileThread,
                this);
        thread_async_usb_ = std::thread(&AGCMonitor::AsyncUSBThread, this);
        thread_if_packing_ = std::thread(&AGCMonitor::IFPackingThread, this);
        is_recording_ = true;

        std::cerr << "[" << name_log_ << "]" << "Start recording." << std::endl;
    }
}

void AGCMonitor::StopRecording() {
    if (is_recording_) {
        stop_request_ = true;
        semaphore_agc_agcts_queue_.notify();
        semaphore_packed_if_queue_.notify();
        semaphore_unpacked_if_queue_.notify();

        thread_agc_and_overrun_.join();
        thread_write_agc_to_file_.join();
        thread_write_if_to_file_.join();
        thread_async_usb_.join();
        thread_if_packing_.join();

        is_recording_ = false;

        std::cerr << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        std::cerr << ": [" << name_log_ << "]" << "Stop recording."
                  << std::endl;
    }
}

void AGCMonitor::AllocateAndSubmitIFTransfers() {
    for (unsigned i = 0; i < kNumberOfTransfers; ++i) {
        libusb_transfer *if_transfer = libusb_alloc_transfer(
                0 /* iso packets num */);
        if (if_transfer == nullptr) {
            std::cerr << "Couldn't allocate an IF transfer." << std::endl;
            exit(1);
        }
        libusb_fill_bulk_transfer(if_transfer, device_handle_, kIFEndpoint,
                                  new uint8_t[kIFTransferBufferSize],
                                  kIFTransferBufferSize, IFTransferCallback,
                                  this /* user data */, kBulkTransferTimeout);
        CHECK_LIBUSB_ERR(libusb_submit_transfer(if_transfer));
    }
}

void AGCMonitor::WriteAGCAndAGCTSToFileThread(void) {
    auto time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream filename;
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H-%M-%S",
             gmtime(reinterpret_cast<time_t *>(&time)));
    filename << name_log_ << "_AGC_" << buf << ".bin";

    std::ofstream file(filename.str());
    if (!file.is_open()) {
        ERROR_EXIT("Couldn't open AGC/AGCTS file.");
    }
    if (!file.good()) {
        ERROR_EXIT("AGC/AGCTS file not good.");
    }

    while (!stop_request_ || !AGC_queue_.empty() || !AGCTS_queue_.empty()) {
        semaphore_agc_agcts_queue_.wait();
        if (stop_request_ && AGC_queue_.empty() && AGCTS_queue_.empty()) {
            break;
        }
        mutex_agc_agcts_queue_.lock();
        auto agc = AGC_queue_.front();
        auto agcts = AGCTS_queue_.front();
        AGC_queue_.pop();
        AGCTS_queue_.pop();
        mutex_agc_agcts_queue_.unlock();

        uint32_t tmp1 = 0;
        uint32_t tmp2 = 0;
        for (unsigned int i = 0; i < agc.size(); i++) {
            tmp1 = agc[i];
            tmp2 = static_cast<uint32_t>(agcts[i]);
            file.write(reinterpret_cast<char *>(&tmp1), sizeof(uint32_t));
            file.write(reinterpret_cast<char *>(&tmp2), sizeof(uint32_t));
        }
    }
}

void AGCMonitor::WriteIFToFileThread(void) {
    auto time_v = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream filename;
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H-%M-%S",
             gmtime(reinterpret_cast<time_t *>(&time_v)));

    std::ofstream file;
    file.open("/" + name_log_ + "_IF_" + buf + ".bin",
              std::ios_base::out | std::ios_base::binary);
    if (!file.is_open() || !file.good()) {
        std::cerr << time(nullptr) << " Couldn't open file." << std::endl;
    } else {
        std::cerr << time(nullptr) << " Could open file." << std::endl;
    }

    while (!stop_request_ || !packed_IF_queue_.empty()) {
        semaphore_packed_if_queue_.wait();
        if (stop_request_ && packed_IF_queue_.empty()) {
            break;
        }
        mutex_packed_if_queue_.lock();
        auto if_data = packed_IF_queue_.front();
        packed_IF_queue_.pop();
        mutex_packed_if_queue_.unlock();

        file.write(reinterpret_cast<char *>(if_data.data()), if_data.size());
        if (circular_if_file_ && file.tellp() > kMaxCircularIFSize) {
            std::cerr << time(nullptr) << " A new circle. Last tellp location: "
                      << file.tellp() << std::endl;
            file.seekp(0);
        }
        file.flush();
    }
    std::cerr << time(nullptr) << " Stopping write. Tellp location: "
              << file.tellp() << std::endl;
}

void AGCMonitor::AGCAndOverrunThread() {
    bool b_err = false;

    // Check if the device is already overrun -- can't continue if so.
    GetStatus(GS_kControlTransferIndexIsRXOverrun, &b_err);
    if (b_err) {
        ERROR_EXIT("Buffer overrun at start please RESTART!");
    }

    while (!stop_request_) {
        // Get AGC.
        std::vector<uint16_t> raw_agc_data(kAGCTransferBufferSize);
        uint64_t read_count = ReadAGC(raw_agc_data.size(), raw_agc_data.data());
        raw_agc_data.resize(read_count);
        int64_t time = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        std::vector<int64_t> agcts_data(read_count, time);

        if (!FLAGS_skipagc) {
            mutex_agc_agcts_queue_.lock();
            AGC_queue_.push(raw_agc_data);
            AGCTS_queue_.push(agcts_data);
            mutex_agc_agcts_queue_.unlock();
            semaphore_agc_agcts_queue_.notify();
        }

        // Get overrun status.
        GetStatus(GS_kControlTransferIndexIsRXOverrun, &b_err);
        if (b_err) {
            ERROR_EXIT("Overrun detected. Quitting.");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kAGCReadTimeout));
    }
}

void AGCMonitor::AsyncUSBThread() {
    // Constant is in ms, timeval has us, so multiply by 1000.
    timeval tv = {0, kUSBHandleTimeout * 1000};
    while (!stop_request_) {
        libusb_handle_events_timeout_completed(nullptr /* context */, &tv,
                                               nullptr /* completed */);
    }
}

void AGCMonitor::IFPackingThread() {
    while (!stop_request_) {
        semaphore_unpacked_if_queue_.wait();
        if (stop_request_) {
            break;
        }
        mutex_unpacked_if_queue_.lock();
        auto unpacked_if = unpacked_IF_queue_.front();
        unpacked_IF_queue_.pop();
        mutex_unpacked_if_queue_.unlock();

        // Packs 1x4 samples in 1 byte (for real data)
        std::vector<uint8_t> packed_if(kIFTransferBufferSize / pack_mode_);
        if (pack_mode_ == 4 && !is_complex_data_) {
            for (unsigned offset = 0, buffer_index = 0;
                 offset < kIFTransferBufferSize; offset += 4, ++buffer_index) {
                packed_if[buffer_index] = (unpacked_if[offset] & k2BitMask) |
                                          ((unpacked_if[offset + 1] & k2BitMask)
                                                  << 2) |
                                          ((unpacked_if[offset + 2] & k2BitMask)
                                                  << 4) |
                                          ((unpacked_if[offset + 3] & k2BitMask)
                                                  << 6);
            }
            // Packs 2x2 samples in 1 byte (for complex data)
        } else if (pack_mode_ == 2 && is_complex_data_) {
            for (unsigned offset = 0, buffer_index = 0;
                 offset < kIFTransferBufferSize; offset += 2, ++buffer_index) {
                packed_if[buffer_index] = (unpacked_if[offset] & k4BitMask) |
                                          ((unpacked_if[offset + 1] & k4BitMask)
                                                  << 4);
            }
        } else {
            ERROR_EXIT("Uncompatible settings for packmode and complex data");
        }

        mutex_packed_if_queue_.lock();
        packed_IF_queue_.push(packed_if);
        mutex_packed_if_queue_.unlock();
        semaphore_packed_if_queue_.notify();
    }
}

uint64_t AGCMonitor::ReadAGC(const uint64_t buf_size, uint16_t *buf) {
    unsigned char *cp_agc_data = new unsigned char[buf_size * 2];
    unsigned char uc_flags[5];
    uint16_t buf16 = 0;

    memset(cp_agc_data, 0, buf_size * 2);

    // Get AGC.
    USRPTransfer2(kInVendorDeviceRequestFlags, 0, 5, uc_flags);
    USRPTransfer2(kInVendorDeviceRequestAGC, 0,
                  static_cast<uint16_t>(buf_size * 2), cp_agc_data);

    if (uc_flags[2] > buf_size) {
        uc_flags[2] = static_cast<unsigned char>(buf_size);
    } else if (uc_flags[2] == 32) {
        ERROR_EXIT("AGC samples probably lost. Quitting.");
    }

    unsigned j = 0;
    for (unsigned i = 0; i < uc_flags[2]; i++) {
        buf16 = cp_agc_data[2 * i + 1] << 8;
        buf16 |= (cp_agc_data[2 * i] & 0xFF);
        buf16 = buf16 & static_cast<uint16_t>(0x0FFF);
        if (buf16 > 0) {
            buf[j++] = buf16;
        }
    }

    delete[] cp_agc_data;

    return j;
}

void AGCMonitor::GetStatus(const uint16_t which, bool *trouble) {
    unsigned char status;
    *trouble = true;
    WriteCmd(kInVendorDeviceRequestStatus, 0, which, sizeof(status), &status);
    *trouble = status;
}

void AGCMonitor::WriteCmd(const uint8_t request, const uint16_t value,
                          const uint16_t index, const uint16_t len,
                          uint8_t *bytes) {
    uint8_t requesttype = (request & 0x80) ? kInVendorDeviceRequestType
                                           : kOutVendorDeviceRequestType;
    CHECK_LIBUSB_ERR(
            libusb_control_transfer(device_handle_, requesttype, request, value,
                                    index, bytes, len, kTimeout));
}

void AGCMonitor::USRPTransfer(const uint8_t vrq_type, const uint16_t start) {
    WriteCmd(vrq_type, start, 0, 0, 0);
}

void AGCMonitor::USRPTransfer2(const uint8_t vrq_type, const uint16_t start,
                               const uint16_t len, uint8_t *buf) {
    WriteCmd(vrq_type, start, 0, len, buf);
}
