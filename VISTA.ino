#include <TimerThree.h>

#include <L3G.h>
#include <LSM303.h>
#include <LPS.h>
#include <Wire.h>

// Define picks between the built-in USB serial communication or the
// serial over pins 0 and 1.
// If defined, Serial uses USB.
#define SERIAL_USB

// Macro for Serial.begin() or Serial1.begin(), depending on the SERIAL_USB (no)define.
#ifdef SERIAL_USB
#define log_begin(baud_rate)  \
  do{                         \
    Serial.begin(baud_rate);  \
  }while(false)
#else
#define log_begin(baud_rate)   \
  do{                          \
    Serial1.begin(baud_rate);  \
  }while(false)
#endif

// Macro for Serial.print() or Serial1.print(), depending on the SERIAL_USB (no)define.
#ifdef SERIAL_USB
#define log(str)        \
  do{                   \
    Serial.print(str);  \
  }while(false)
#else
#define log(str)         \
  do{                    \
    Serial1.print(str);  \
  }while(false)
#endif

// Macro for Serial.println() or Serial1.println(), depending on the SERIAL_USB (no)define.
#ifdef SERIAL_USB
#define logln(str)        \
  do{                     \
    Serial.println(str);  \
  }while(false)
#else
#define logln(str)         \
  do{                      \
    Serial1.println(str);  \
  }while(false)
#endif

const long sensorReadTimeoutMicroseconds = 100000;
volatile bool doReadSensors = false;

volatile L3G gyro;
volatile LSM303 acce;
volatile LPS pres;

void setup() {
  log_begin(115200);
  
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
  
  Timer3.initialize(sensorReadTimeoutMicroseconds);
  Timer3.attachInterrupt( [](){
    doReadSensors = true;
  });
}

void readSensors() {
  logln("Altimeter");
  logln(pres.pressureToAltitudeMeters(pres.readPressureMillibars(), 1006));

  logln("Accelerometer");
  acce.read();
  logln(acce.a.x);
  logln(acce.a.y);
  logln(acce.a.z);

  logln("Gyroscope");
  gyro.read();
  logln(gyro.g.x);
  logln(gyro.g.y);
  logln(gyro.g.z);
}

void loop(){
  if(doReadSensors){
    readSensors();
    doReadSensors = false;
  }
}

