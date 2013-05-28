// This header defines the base and utility classes for input and output cards
#ifndef OSPCARDS_H
#define OSPCARDS_H

template<int D> struct ospDecimalValue;
class ospSettingsHelper;

// a base class for both input and output cards
class ospBaseCard {
public:
  ospBaseCard() { }

  // setup the card
  void initialize() { }

  // return an identifying name for this card, as a PSTR
  const __FlashStringHelper *cardIdentifier() { return F(""); }

  // how many settings does this card have
  byte settingsCount() const { return 0; }

  // read settings from the card
  int readSetting(byte index) { return -1; }

  // write settings to the card
  bool writeSetting(byte index, int val) { return false; }

  // return a text description of the N'th setting, as a PSTR
  // also returns the number of decimal places
  const __FlashStringHelper *describeSetting(byte index, byte *decimals) { *decimals = 0; return 0; }

  // save and restore settings to/from EEPROM using the settings helper
  void saveSettings(ospSettingsHelper& settings) { }
  void restoreSettings(ospSettingsHelper& settings) { }
};

class ospBaseInputCard : public ospBaseCard {
public:
  ospBaseInputCard() { ospBaseCard(); }

  ospDecimalValue<1> readInput();
};

class ospBaseOutputCard : public ospBaseCard {
public:
  ospBaseOutputCard() { ospBaseCard(); }

  void setOutputPercent(ospDecimalValue<1> percentage);
};

#endif
