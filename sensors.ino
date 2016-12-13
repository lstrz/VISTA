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

    Serial.print(" - setting range to +-2000deg/s from +-245deg/s: ");
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

void sensorsInit() {
  Wire.begin();
  L3Ginit();
  LSM303init();
  LPSinit();
}
