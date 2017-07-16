#pragma once

// Stolen from Maxim Egorushkin on stackoverflow.
// http://stackoverflow.com/questions/4792449/c0x-has-no-semaphores-how-to-synchronize-threads

#include <condition_variable>
#include <mutex>

class Semaphore {
public:
    Semaphore();

    void notify();

    void wait();

    bool try_wait();

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    uint64_t count_;
};

