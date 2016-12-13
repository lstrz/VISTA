#include <L3G.h>
#include <LSM303.h>
#include <LPS.h>
#include <Wire.h>

L3G gyro;
LSM303 acce;
LPS pres;

void ok(){
  Serial.println("OK!");
}

void notOkStop(){
  Serial.println("NOT OK! Stopping!");
  while(true);
}

void L3Ginit(){
  Serial.print("Gyroscope auto-init: ");
  if(gyro.init()){
    ok();
    
    Serial.print(" - enabling default mode: ");
    gyro.enableDefault();
    if(acce.last_status) notOkStop();
    else ok();

    Serial.print(" - enabling low data rate: ");
    gyro.writeReg(L3G::LOW_ODR, 0x01);
    if(acce.last_status) notOkStop();
    else ok();

    Serial.print(" - setting data rate to 12.5Hz from 200Hz: ");
    gyro.writeReg(L3G::CTRL_REG1, 0x3F);
    if(acce.last_status) notOkStop();
    else ok();

    Serial.print(" - setting range to +-2000deg/s from +-245g: ");
    gyro.writeReg(L3G::CTRL_REG4, 0x30);
    if(acce.last_status) notOkStop();
    else ok();
    
  }else{
    notOkStop();
  }
}

void LSM303init(){
  Serial.print("Accelerometer auto-init: ");
  if(acce.init()){
    ok();
    
    Serial.print(" - enabling default mode: ");
    acce.enableDefault();
    if(acce.last_status) notOkStop();
    else ok();
      
    Serial.print(" - setting data rate to 12.5Hz from 50Hz: ");
    acce.writeReg(LSM303::CTRL_REG1_A, 0x37);
    if(acce.last_status) notOkStop();
    else ok();
    
    Serial.print(" - setting range to +-16g from +-2g: ");
    acce.writeReg(LSM303::CTRL_REG2_A, 0x20);
    if(acce.last_status) notOkStop();
    else ok();
    
  }else{
    notOkStop();
  }
}

void LPSinit(){
  Serial.print("Barometer auto-init: ");
  if(pres.init()){
    ok();
    
    Serial.print(" - enabling default mode: ");
    pres.enableDefault();
    if(acce.last_status) notOkStop();
    else ok();
    
  }else{
    notOkStop();
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  for(int i = 10; i >= 0; --i){
    Serial.print("Delay: ");
    Serial.println(i);
    delay(1000);
  }

  L3Ginit();
  LSM303init();
  LPSinit();
}

void loop() {
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
  
  delay(100);
}
