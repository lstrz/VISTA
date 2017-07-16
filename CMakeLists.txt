cmake_minimum_required(VERSION 3.1)
project(SiGeDumperLite-wiringPi)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -fsigned-char -Wall -Wextra -Werror -O3")

set(SOURCE_FILES AGCMonitor.cpp main.cpp Semaphore.cpp RocketInterfaceMonitor.cpp)
add_executable(SiGeDumperLite-wiringPi ${SOURCE_FILES})
target_link_libraries(SiGeDumperLite-wiringPi gflags usb-1.0 wiringPi pthread crypt)