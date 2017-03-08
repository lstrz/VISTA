void ok(){
  logln("OK!");
}

void notOkStop(){
  logln("NOT OK! Stopping!");
  while(true);
}

void maybeOk(){
  logln("Maybe OK! No return value.");
}

void LSM6init(){
  log("IMU auto-init: ");
  if(imu.init()) ok();
  else notOkStop();
   
  log(" - enabling default mode: ");
  imu.enableDefault();
  if(imu.last_status) notOkStop();
  else ok();
  
  log(" - enabling high performance mode for accelerometer: ");
  uint8_t reg_ctrl6_c = imu.readReg(LSM6::CTRL6_C);
  if(imu.last_status) notOkStop();
  reg_ctrl6_c |= 0x10;
  imu.writeReg(LSM6::CTRL6_C, reg_ctrl6_c);
  if(imu.last_status) notOkStop();
  else ok();
  
  log(" - setting accelerometer to 833 Hz output data rate, 16 g sensitivity and 400 Hz filter: ");
  imu.writeReg(LSM6::CTRL1_XL, 0x74);
  if(imu.last_status) notOkStop();
  else ok();
  
  log(" - enabling high performance mode for gyro: ");
  uint8_t reg_ctrl7_g = imu.readReg(LSM6::CTRL7_G);
  if(imu.last_status) notOkStop();
  reg_ctrl7_g |= 0x80;
  imu.writeReg(LSM6::CTRL7_G, reg_ctrl7_g);
  if(imu.last_status) notOkStop();
  else ok();
  
  log(" - setting gyroscope to 833 Hz output data rate and 2000 dps sensitivity: ");
  imu.writeReg(LSM6::CTRL2_G, 0x7C);
  if(imu.last_status) notOkStop();
  else ok();
}

void LPSinit(){
  log("Barometer auto-init: ");
  if(pres.init()) ok();
  else noOkStop();
    
  log(" - enabling default mode: ");
  pres.enableDefault();
  maybeOk();
}

void sensorsInit() {
  Wire.begin();
  LSM6init();
  LPSinit();
}
