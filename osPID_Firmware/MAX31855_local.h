#ifndef	MAX31855_H
#define MAX31855_H

#if	ARDUINO >= 100
	#include "Arduino.h"
#else  
	#include "WProgram.h"
#endif

template<int D> struct ospDecimalValue;

#define	FAULT_OPEN      -10000
#define	FAULT_SHORT_GND -10001
#define	FAULT_SHORT_VCC -10002

class MAX31855
{
public:
  MAX31855(unsigned char SCK, unsigned	char CS, unsigned char SO);

  ospDecimalValue<1> readThermocoupleCelsius();
  ospDecimalValue<2> readJunctionCelsius();

private:
  unsigned char so;
  unsigned char cs;
  unsigned char sck;

  unsigned long readData();
};

#endif
