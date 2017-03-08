static bool use_serial_usb = false;
static bool use_serial_ext = false;

void log_begin(long baud_rate, bool use_serial_usb, bool use_serial_ext){
  ::use_serial_usb = use_serial_usb;
  ::use_serial_ext = use_serial_ext;
  if(use_serial_usb){
    Serial.begin(baud_rate);
  }
  if(use_serial_ext){
    Serial1.begin(baud_rate);
  }
}

template<typename T> void log(T data){
  if(use_serial_usb){
    Serial.print(data);
  }
  if(use_serial_ext){
    
    Serial1.print(data);
  }
}

template<typename T> void logln(T data){
  if(use_serial_usb){
    Serial.println(data);
  }
  if(use_serial_ext){
    Serial1.println(data);
  }
}
