// this library is public domain. enjoy!
// www.ladyada.net/learn/sensors/thermocouple

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdlib.h>
#include "max6675_local.h"
#include "ospDecimalValue.h"

MAX6675::MAX6675(int8_t SCLK, int8_t CS, int8_t MISO) {
  sclk = SCLK;
  cs = CS;
  miso = MISO;

  //define pin modes
  pinMode(cs, OUTPUT);
  pinMode(sclk, OUTPUT);
  pinMode(miso, INPUT);

  digitalWrite(cs, HIGH);
}

int16_t MAX6675::readQuarterCelsius(void) {
  uint16_t v;

  digitalWrite(cs, LOW);
  _delay_ms(1);

  v = spiread();
  v <<= 8;
  v |= spiread();

  digitalWrite(cs, HIGH);

  if (v & 0x4) {
    // uh oh, no thermocouple attached!
    return -4000; // -1000.0 is not a valid temperature
  }

  v >>= 3;
  return v;
}

ospDecimalValue<1> MAX6675::readCelsius(void) {
  int16_t v = readQuarterCelsius();

  if (v == -4000)
    return makeDecimal<1>(-10000);

  // v is in quarter-units, now we want to convert it to tenths
  // we multiply it by 10 to put it in 40ths, and then divide by 4 to put it in
  // 10ths
  ospDecimalValue<1> quarters = makeDecimal<1>(v * 10);
  return (quarters / makeDecimal<2>(400)).rescale<1>();
}


byte MAX6675::spiread(void) {
  int i;
  byte d = 0;

  for (i=7; i>=0; i--)
  {
    digitalWrite(sclk, LOW);
    _delay_ms(1);
    if (digitalRead(miso)) {
      //set the bit to 0 no matter what
      d |= (1 << i);
    }

    digitalWrite(sclk, HIGH);
    _delay_ms(1);
  }

  return d;
}
