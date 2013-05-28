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
  unsigned int minimumToggleMilliseconds;
  unsigned int toggleEpoch;
  bool hasBeenOnThisEpoch;
  byte oldState;

public:
  ospDigitalOutputCard() 
    : ospBaseOutputCard(),
    outputType(OUTPUT_SSR),
    outputWindowMilliseconds(5000),
    minimumToggleMilliseconds(50),
    toggleEpoch(0),
    hasBeenOnThisEpoch(false),
    oldState(LOW)
  { }

  void initialize() {
    pinMode(RelayPin, OUTPUT);
    pinMode(SSRPin, OUTPUT);
  }

  const __FlashStringHelper *cardIdentifier() { return F("OUT_DIGITAL"); }

  // how many settings does this card have
  byte settingsCount() { return 2; }

  // read settings from the card
  int readSetting(byte index) {
    if (index == 0)
      return outputType;
    else if (index == 1)
      return outputWindowMilliseconds;
    else if (index == 2)
      return minimumToggleMilliseconds;
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
    } else if (index == 2) {
      minimumToggleMilliseconds = val;
      return true;
    }
    return false;
  }

  // describe the available settings
  const __FlashStringHelper *describeSetting(byte index, byte *decimals) {
    *decimals = 0;
    if (index == 0) {
      return F("Use RELAY (0) or SSR (1)");
    } else if (index == 1) {
      return F("Output PWM window size in milliseconds");
    } else if (index == 2) {
      return F("Minimum time between PWM edges in milliseconds");
    } else
      return 0;
  }

  // save and restore settings to/from EEPROM using the settings helper
  void saveSettings(ospSettingsHelper& settings) {
    settings.save(outputWindowMilliseconds);
    settings.save(outputType);
    settings.save(minimumToggleMilliseconds);
  }

  void restoreSettings(ospSettingsHelper& settings) {
    settings.restore(outputWindowMilliseconds);
    settings.restore(outputType);
    settings.restore(minimumToggleMilliseconds);
  }

  void setOutputPercent(ospDecimalValue<1> percent) {
    unsigned long t = millis();
    int wind = t % outputWindowMilliseconds;
    unsigned int epoch = t / outputWindowMilliseconds;

    // every epoch, we output at most one pulse
    if (epoch != toggleEpoch)
    {
      hasBeenOnThisEpoch = false;
      toggleEpoch = epoch;
    }

    // since |percent| is effectively integer thousandths, we can just
    // divide here to get the number of milliseconds that the output should
    // be ON
    int oVal = long(outputWindowMilliseconds) * percent.rawValue() / 1000L;

    if (oVal < minimumToggleMilliseconds)
      oVal = 0;
    if (outputWindowMilliseconds - oVal < minimumToggleMilliseconds)
      oVal = outputWindowMilliseconds;

    byte newState = (oVal > wind) ? HIGH : LOW;

    // don't try to turn back ON if the command changes in the middle of a PWM period
    if (oldState != newState && newState == HIGH && hasBeenOnThisEpoch)
      return;

    if (newState == HIGH)
      hasBeenOnThisEpoch = true;

    oldState = newState;

    if (outputType == OUTPUT_RELAY)
      digitalWrite(RelayPin, newState);
    else
      digitalWrite(SSRPin, newState);
  }
};

typedef ospDigitalOutputCard ospDigitalOutputCardV1_20;
typedef ospDigitalOutputCard ospDigitalOutputCardV1_50;

#endif

