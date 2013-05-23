// this library is public domain. enjoy!
// www.ladyada.net/learn/sensors/thermocouple

// we have modified the library to use our decimal fixed-point numbers
// rather than floating-point

#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

template<int D> struct ospDecimalValue;

class MAX6675 {
 public:
  MAX6675(int8_t SCLK, int8_t CS, int8_t MISO);

  ospDecimalValue<1> readCelsius(void);
 private:
  int8_t sclk, miso, cs;
  uint8_t spiread(void);
  int16_t readQuarterCelsius(void);
};
