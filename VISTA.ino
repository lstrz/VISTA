#include <TimerThree.h>

#include <LSM6.h>
#include <LPS.h>
#include <Wire.h>

namespace{

constexpr long kSensorReadTimeoutMicroseconds = 1200;
constexpr long kBaudrate = 115200;

volatile bool do_read_sensors = false;
volatile LSM6 imu;
volatile LPS pres;

}  // namespace

void setup() {
  log_begin(kBaudrate, true /* use_serial_usb */, true /* use_serial_ext */);
  
  for(int i = 10; i >= 0; --i){
    log("Delay: ");
    logln(i);
    delay(1000);
  }

  sensorsInit();

  pinMode(7, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(7), [](){
    detachInterrupt(digitalPinToInterrupt(7));
    // TODO wake from sleep here
  }, FALLING);
  
  Timer3.initialize(kSensorReadTimeoutMicroseconds);
  Timer3.attachInterrupt( [](){
    do_read_sensors = true;
  });
}

void readSensors() {
  logln("Altimeter");
  logln(pres.pressureToAltitudeMeters(pres.readPressureMillibars(), 1006));

  logln("Accelerometer");
  imu.read();
  logln(imu.a.x);
  logln(imu.a.y);
  logln(imu.a.z);

  logln("Gyroscope");
  logln(imu.g.x);
  logln(imu.g.y);
  logln(imu.g.z);
}

void loop(){
  if(do_read_sensors){
    readSensors();
    do_read_sensors = false;
  }
}

