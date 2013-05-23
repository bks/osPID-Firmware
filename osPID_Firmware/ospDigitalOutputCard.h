#ifndef OSPDIGITALOUTPUTCARD_H
#define OSPDIGITALOUTPUTCARD_H

#include "ospCards.h"
#include "ospDecimalValue.h"

class ospDigitalOutputCard : public ospBaseOutputCard {
private:
  enum { RelayPin = 5, SSRPin = 6 };
  enum { OUTPUT_RELAY = 0, OUTPUT_SSR = 1 };

  byte outputType;
  unsigned int outputWindowMilliseconds;

public:
  ospDigitalOutputCard() 
    : ospBaseOutputCard(),
    outputType(OUTPUT_SSR),
    outputWindowMilliseconds(5000)
  { }

  void initialize() {
    pinMode(RelayPin, OUTPUT);
    pinMode(SSRPin, OUTPUT);
  }

  const char *cardIdentifier() { return PSTR("OUT_DIGITAL"); }

  // how many settings does this card have
  byte settingsCount() { return 2; }

  // read settings from the card
  int readSetting(byte index) {
    if (index == 0)
      return outputType;
    else if (index == 1)
      return outputWindowMilliseconds;
    return -1;
  }

  // write settings to the card
  bool writeSetting(byte index, int val) {
    if (index == 0 && (val == OUTPUT_SSR || val == OUTPUT_RELAY)) {
      outputType = val;
      return true;
    }
    else if (index == 1) {
      outputWindowMilliseconds = val;
      return true;
    }
    return false;
  }

  // describe the available settings
  const char *describeSetting(byte index, byte *decimals) {
    *decimals = 0;
    if (index == 0) {
      return PSTR("Use RELAY (0) or SSR (1)");
    } else if (index == 1) {
      return PSTR("Output PWM window size in milliseconds");
    } else
      return 0;
  }

  // save and restore settings to/from EEPROM using the settings helper
  void saveSettings(ospSettingsHelper& settings) {
    settings.save(outputWindowMilliseconds);
    settings.save(outputType);
  }

  void restoreSettings(ospSettingsHelper& settings) {
    settings.restore(outputWindowMilliseconds);
    settings.restore(outputType);
  }

  void setOutputPercent(ospDecimalValue<1> percent) {
    int wind = millis() % outputWindowMilliseconds;

    // since |percent| is effectively integer thousandths, we can just
    // divide here to get the number of milliseconds that the output should
    // be ON
    int oVal = long(outputWindowMilliseconds) * percent.rawValue() / 1000L;

    if (outputType == OUTPUT_RELAY)
      digitalWrite(RelayPin, (oVal>wind) ? HIGH : LOW);
    else
      digitalWrite(SSRPin, (oVal>wind) ? HIGH : LOW);
  }
};

typedef ospDigitalOutputCard ospDigitalOutputCardV1_20;
typedef ospDigitalOutputCard ospDigitalOutputCardV1_50;

#endif

