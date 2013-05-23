#ifndef OSPTEMPERATUREINPUTCARD_H
#define OSPTEMPERATUREINPUTCARD_H

#include "ospCards.h"
#include "ospDecimalValue.h"
#include "ospSettingsHelper.h"
#include "max6675_local.h"
#include "MAX31855_local.h"

template<typename TCType> class ospTemperatureInputCard : public ospBaseInputCard {
private:
  enum { thermistorPin = A6 };
  enum { thermocoupleCS = 10 };
  enum { thermocoupleSO = 12 };
  enum { thermocoupleCLK = 13 };

  enum { INPUT_THERMOCOUPLE = 0, INPUT_THERMISTOR = 1 };

  byte inputType;
  union {
    struct {
      ospDecimalValue<0> thermistorNominalOhms;
      ospDecimalValue<0> referenceResistorOhms;
      ospDecimalValue<1> BCoefficient;
      ospDecimalValue<1> thermistorReferenceTemperatureCelsius;
    };
    int settingsBlock[4];
  };

  TCType thermocouple;

public:
  ospTemperatureInputCard() :
    ospBaseInputCard(),
    inputType(INPUT_THERMOCOUPLE),
    thermistorNominalOhms(makeDecimal<0>(1000)),
    referenceResistorOhms(makeDecimal<0>(1000)),
    BCoefficient(makeDecimal<1>(4000)),
    thermistorReferenceTemperatureCelsius(makeDecimal<1>(200)),
    thermocouple(thermocoupleCLK, thermocoupleCS, thermocoupleSO)
  { }

  // setup the card
  void initialize() { }

  // return the card identifier
  const char *cardIdentifier();

private:
  // actually read the thermocouple
  ospDecimalValue<1> readThermocouple();

  // convert the thermistor voltage to a temperature
  ospDecimalValue<1> thermistorVoltageToTemperature(int voltage)
  {
    double R = referenceResistorOhms.toDouble() / (1024.0/(double)voltage - 1);
    double steinhart;
    steinhart = R / thermistorNominalOhms.toDouble();     // (R/Ro)
    steinhart = log(steinhart);                  // ln(R/Ro)
    steinhart /= BCoefficient.toDouble();                   // 1/B * ln(R/Ro)
    steinhart += 1.0 / (thermistorReferenceTemperatureCelsius.toDouble() + 273.15); // + (1/To)
    steinhart = 1.0 / steinhart;                 // Invert
    steinhart -= 273.15;                         // convert to C

    return makeDecimal<1>(int(steinhart * 10.0));
  }

public:
  // read the card
  ospDecimalValue<1> readInput() {
    if (inputType == INPUT_THERMISTOR) {
      int voltage = analogRead(thermistorPin);
      return thermistorVoltageToTemperature(voltage);
    }

    return readThermocouple();
  }

  // how many settings does this card have
  byte settingsCount() { return 5; }

  // read settings from the card
  int readSetting(byte index) {
    if (index == 0)
      return inputType;
    else if (index < 5)
      return settingsBlock[index];
    else
      return -1;
  }

  // write settings to the card
  bool writeSetting(byte index, int val) {
    if (index == 0 && (val == INPUT_THERMOCOUPLE || val == INPUT_THERMISTOR)) {
      inputType = val;
    } else if (index < 5)
      settingsBlock[index] = val;
    else
      return false;
    return true;
  }

  // describe the card settings
  const char * describeSetting(byte index, byte *decimals) {
    if (index < 3)
      *decimals = 0;
    else
      *decimals = 1;

    switch (index) {
    case 0:
      return PSTR("Use the THERMOCOUPLE (0) or THERMISTOR (1) reader");
    case 1:
      return PSTR("The thermistor nominal resistance (ohms)");
    case 2:
      return PSTR("The reference resistor value (ohms)");
    case 3:
      return PSTR("The thermistor B coefficient");
    case 4:
      return PSTR("The thermistor reference temperature (Celsius)");
    default:
      return 0;
    }
  }

  // save and restore settings to/from EEPROM using the settings helper
  void saveSettings(ospSettingsHelper& settings) {
    settings.save(inputType);
    settings.save(thermistorNominalOhms);
    settings.save(referenceResistorOhms);
    settings.save(BCoefficient);
    settings.save(thermistorReferenceTemperatureCelsius);
  }

  void restoreSettings(ospSettingsHelper& settings) {
    settings.restore(inputType);
    settings.restore(thermistorNominalOhms);
    settings.restore(referenceResistorOhms);
    settings.restore(BCoefficient);
    settings.restore(thermistorReferenceTemperatureCelsius);
  }
};

template<> ospDecimalValue<1> ospTemperatureInputCard<MAX6675>::readThermocouple() {
  return thermocouple.readCelsius();
}

template<> const char *ospTemperatureInputCard<MAX6675>::cardIdentifier() {
  return PSTR("IN_TEMP_V1.10");
}

template<> ospDecimalValue<1> ospTemperatureInputCard<MAX31855>::readThermocouple() {
   ospDecimalValue<1> val = thermocouple.readThermocoupleCelsius();

   if (val == makeDecimal<1>(FAULT_OPEN) || val == makeDecimal<1>(FAULT_SHORT_GND) || val == makeDecimal<1>(FAULT_SHORT_VCC))
     val = makeDecimal<1>(-10000);

   return val;
}

template<> const char *ospTemperatureInputCard<MAX31855>::cardIdentifier() {
  return PSTR("IN_TEMP_V1.20");
}

typedef ospTemperatureInputCard<MAX6675> ospTemperatureInputCardV1_10;
typedef ospTemperatureInputCard<MAX31855> ospTemperatureInputCardV1_20;

#endif

