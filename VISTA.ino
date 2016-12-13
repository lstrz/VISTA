#include <TimerThree.h>

#include <L3G.h>
#include <LSM303.h>
#include <LPS.h>
#include <Wire.h>

const long sensorReadTimeoutMicroseconds = 100000;
volatile bool doReadSensors = false;
volatile L3G gyro;
volatile LSM303 acce;
volatile LPS pres;

void setup() {
  Serial.begin(115200);
  
  for(int i = 10; i >= 0; --i){
    Serial.print("Delay: ");
    Serial.println(i);
    delay(1000);
  }

  sensorsInit();
  Timer3.initialize(sensorReadTimeoutMicroseconds);
  Timer3.attachInterrupt( [](){doReadSensors = true;} );
}

void readSensors() {
  Serial.println("Altimeter");
  Serial.print(pres.pressureToAltitudeMeters(pres.readPressureMillibars(), 1006));
  Serial.print("\n");

  Serial.println("Accelerometer");
  acce.read();
  Serial.println(acce.a.x);
  Serial.println(acce.a.y);
  Serial.println(acce.a.z);

  Serial.println("Gyroscope");
  gyro.read();
  Serial.println(gyro.g.x);
  Serial.println(gyro.g.y);
  Serial.println(gyro.g.z);
}

void loop(){
  if(doReadSensors){
    readSensors();
    doReadSensors = false;
  }
}

