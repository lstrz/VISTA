// Define picks between the built-in USB serial communication or the
// serial over pins 0 and 1.
// If SERIAL_USB is defined, serial over USB is used.
// If SERIAL_EXT is defined, external (TX/RX pins) serial is used.
// They are not exclusive.
#define SERIAL_USB
#define SERIAL_EXT

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
