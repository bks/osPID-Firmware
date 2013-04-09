/* This file contains the setup() and loop() logic for the controller. */

#include <LiquidCrystal.h>
#include <Arduino.h>
#include "AnalogButton_local.h"
#include "PID_v1_local.h"
#include "PID_AutoTune_v0_local.h"
#include "ospProfile.h"
#include "ospCardSimulator.h"
#include "ospTemperatureInputCard.h"
#include "ospDigitalOutputCard.h"

#undef BUGCHECK
#define BUGCHECK() ospBugCheck(PSTR("MAIN"), __LINE__);

/*******************************************************************************
* The osPID Kit comes with swappable IO cards which are supported by different
* device drivers & libraries. For the osPID firmware to correctly communicate with
* your configuration, you must specify the type of |theInputCard| and |theOutputCard|
* below.
*
* Please note that only 1 input card and 1 output card can be used at a time. 
* List of available IO cards:
*
* Input Cards
* ===========
* 1. ospTemperatureInputCardV1_10:
*    Temperature Basic V1.10 with 1 thermistor & 1 type-K thermocouple (MAX6675)
*    interface.
* 2. ospTemperatureInputCardV1_20:
*    Temperature Basic V1.20 with 1 thermistor & 1 type-K thermocouple 
*    (MAX31855KASA) interface.
* 3. (your subclass of ospBaseInputCard here):
*    Generic prototype card with input specified by user. Please add necessary
*    input processing in the section below.
*
* Output Cards
* ============
* 1. ospDigitalOutputCardV1_20: 
*    Output card with 1 SSR & 2 relay output.
* 2. ospDigitalOutputCardV1_50: 
*    Output card with 1 SSR & 2 relay output. Similar to V1.20 except LED mount
*    orientation.
* 3. (your subclass of ospBaseOutputCard here):
*    Generic prototype card with output specified by user. Please add necessary
*    output processing in the section below.
*
* For firmware development, there is also the ospCardSimulator which acts as both
* the input and output cards and simulates the controller being attached to a
* simple system.
*******************************************************************************/

#undef USE_SIMULATOR
#ifndef USE_SIMULATOR
ospTemperatureInputCardV1_20 theInputCard;
ospDigitalOutputCardV1_50 theOutputCard;
#else
ospCardSimulator theInputCard
#define theOutputCard theInputCard
#endif

// we use the LiquidCrystal library to drive the LCD screen
LiquidCrystal theLCD(A1, A0, 4, 7, 8, 9);

// our AnalogButton library provides debouncing and interpretation
// of the multiplexed theButtonReader
AnalogButton theButtonReader(A3, 0, 253, 454, 657);

// an in-memory buffer that we use when receiving a profile over USB
ospProfile profileBuffer;

// the 0-based index of the active profile while a profile is executing
byte activeProfileIndex;
boolean runningProfile = false;

// the name of this controller unit (can be queried and set over USB)
char controllerName[17] = { 'o', 's', 'P', 'I', 'D', ' ',
       'C', 'o', 'n', 't', 'r', 'o', 'l', 'l', 'e', 'r', '\0' };

// the gain coefficients of the PID controller
double kp = 2, ki = 0.5, kd = 2;

// the direction flag for the PID controller
byte ctrlDirection = 0;

// whether the controller is executing a PID law or just outputting a manual
// value
byte modeIndex = 0;

// the 4 setpoints we can easily switch between
double setPoints[4] = { 25.0f, 75.0f, 150.0f, 300.0f };

// the index of the selected setpoint
byte setpointIndex = 0;

// the variables to which the PID controller is bound
double setpoint=250,input=250,output=50, pidInput=250;

// what to do on power-on
enum {
    POWERON_GOTO_MANUAL,
    POWERON_GOTO_SETPOINT,
    POWERON_RESUME_PROFILE
};

byte powerOnBehavior = POWERON_GOTO_MANUAL;

// the paremeters for the autotuner
double aTuneStep = 20, aTuneNoise = 1;
unsigned int aTuneLookBack = 10;
byte ATuneModeRemember = 0;
PID_ATune aTune(&pidInput, &output);

// whether the autotuner is active
bool tuning = false;

// Pin assignments on the controller card (_not_ the I/O cards)
enum { buzzerPin = 3, systemLEDPin = A2 };

unsigned long now, lcdTime, buttonTime,ioTime, serialTime;
boolean sendInfo=true, sendDash=true, sendTune=true, sendInputConfig=true, sendOutputConfig=true;

PID myPID(&pidInput, &output, &setpoint,kp,ki,kd, DIRECT);

byte curProfStep=0;
byte curType=0;
double curVal=0;
double helperVal=0;
unsigned long helperTime=0;
boolean helperflag=false;
unsigned long curTime=0;

/*Profile declarations*/
const unsigned long profReceiveTimeout = 10000;
unsigned long profReceiveStart=0;
boolean receivingProfile=false;
const int nProfSteps = 15;
char profname[] = {
  'N','o',' ','P','r','o','f'};
byte proftypes[nProfSteps];
unsigned long proftimes[nProfSteps];
double profvals[nProfSteps];

// FIXME: configurable serial parameters?

void setup()
{
  Serial.begin(9600);
  lcdTime=10;
  buttonTime=1;
  ioTime=5;
  serialTime=6;
  //windowStartTime=2;
  theLCD.begin(8, 2);

  theLCD.setCursor(0,0);
  theLCD.print(F(" osPID   "));
  theLCD.setCursor(0,1);
  theLCD.print(F(" v2.00bks"));
  delay(1000);

  theInputCard.initialize();
  theOutputCard.initialize();

  setupEEPROM();

  // show the controller name?

  myPID.SetSampleTime(1000);
  myPID.SetOutputLimits(0, 100);
  myPID.SetTunings(kp, ki, kd);
  myPID.SetControllerDirection(ctrlDirection);
  myPID.SetMode(modeIndex);
}

byte heldButton;
unsigned long buttonPressTime;

// test the buttons and look for button presses or long-presses
void checkButtons()
{
  byte button = theButtonReader.get();
  byte executeButton = BUTTON_NONE;

  if (button != BUTTON_NONE)
  {
    if (heldButton == BUTTON_NONE)
      buttonPressTime = now + 125; // auto-repeat delay
    else if (heldButton == BUTTON_OK)
      ; // OK does long-press/short-press, not auto-repeat
    else if ((now - buttonPressTime) > 250)
    {
      // auto-repeat
      executeButton = button;
      buttonPressTime = now;
    }
    heldButton = button;
  }
  else if (heldButton != BUTTON_NONE)
  {
    if (now < buttonPressTime)
    {
      // the button hasn't triggered auto-repeat yet; execute it
      // on release
      executeButton = heldButton;
    }
    else if (heldButton == BUTTON_OK && (now - buttonPressTime) > 125)
    {
      // BUTTON_OK was held for at least 250 ms: execute a long-press
      okKeyLongPress();
    }
    heldButton = BUTTON_NONE;
  }

  switch (executeButton)
  {
  case BUTTON_NONE:
    break;

  case BUTTON_RETURN:
    backKeyPress();
    break;

  case BUTTON_UP:
    updownKeyPress(true);
    break;

  case BUTTON_DOWN:
    updownKeyPress(false);
    break;

  case BUTTON_OK:
    okKeyPress();
    break;
  }
}

void loop()
{
  now = millis();

  if (now >= buttonTime)
  {
    checkButtons();
    buttonTime += 50;
  }

  bool doIO = now >= ioTime;
  //read in the input
  if(doIO)
  { 
    ioTime+=250;
    input = theInputCard.readInput();
    if (!isnan(input))pidInput = input;
  }

  if(tuning)
  {
    byte val = (aTune.Runtime());

    if(val != 0)
    {
      tuning = false;
    }

    if(!tuning)
    { 
      // We're done, set the tuning parameters
      kp = aTune.GetKp();
      ki = aTune.GetKi();
      kd = aTune.GetKd();
      myPID.SetTunings(kp, ki, kd);
      AutoTuneHelper(false);
      saveEEPROMSettings();
    }
  }
  else
  {
    if(runningProfile) ProfileRunTime();
    //allow the pid to compute if necessary
    myPID.Compute();
  }

  if(doIO)
  {
    theOutputCard.setOutputPercent(output);
  }

  if(now>lcdTime)
  {
    drawMenu();
    lcdTime+=250; 
  }
  if(millis() > serialTime)
  {
    //if(receivingProfile && (now-profReceiveStart)>profReceiveTimeout) receivingProfile = false;
    SerialReceive();
    SerialSend();
    serialTime += 500;
  }
}

// a program invariant has been violated: suspend the controller and
// just flash a debugging message until the unit is power cycled
void ospBugCheck(const char *block, int line)
{
    // note that block is expected to be PROGMEM

    theLCD.noCursor();

    theLCD.setCursor(0, 0);
    for (int i = 0; i < 4; i++)
      theLCD.print(pgm_read_byte_near(&block[i]));
    theLCD.print(F(" Err"));

    theLCD.setCursor(0, 1);
    theLCD.print(F("Lin "));
    theLCD.print(line);

    // just lock up, flashing the error message
    while (true)
    {
      theLCD.display();
      delay(500);
      theLCD.noDisplay();
      delay(500);
    }
}

void changeAutoTune()
{
  if(!tuning)
  {
    //initiate autotune
    AutoTuneHelper(true);
    aTune.SetNoiseBand(aTuneNoise);
    aTune.SetOutputStep(aTuneStep);
    aTune.SetLookbackSec((int)aTuneLookBack);
    tuning = true;
  }
  else
  { //cancel autotune
    aTune.Cancel();
    tuning = false;
    AutoTuneHelper(false);
  }
}

void AutoTuneHelper(boolean start)
{

  if(start)
  {
    ATuneModeRemember = myPID.GetMode();
    myPID.SetMode(MANUAL);
  }
  else
  {
    modeIndex = ATuneModeRemember;
    myPID.SetMode(modeIndex);
  } 
}

void StartProfile()
{
  if(!runningProfile)
  {
    //initialize profle
    curProfStep=0;
    runningProfile = true;
    calcNextProf();
  }
}
void StopProfile()
{
  if(runningProfile)
  {
    curProfStep=nProfSteps;
    calcNextProf(); //runningProfile will be set to false in here
  } 
}

void ProfileRunTime()
{
  if(tuning || !runningProfile) return;

  boolean gotonext = false;

  //what are we doing?
  if(curType==1) //ramp
  {
    //determine the value of the setpoint
    if(now>helperTime)
    {
      setpoint = curVal;
      gotonext=true;
    }
    else
    {
      setpoint = (curVal-helperVal)*(1-(double)(helperTime-now)/(double)(curTime))+helperVal; 
    }
  }
  else if (curType==2) //wait
  {
    double err = input-setpoint;
    if(helperflag) //we're just looking for a cross
    {

      if(err==0 || (err>0 && helperVal<0) || (err<0 && helperVal>0)) gotonext=true;
      else helperVal = err;
    }
    else //value needs to be within the band for the perscribed time
    {
      if (abs(err)>curVal) helperTime=now; //reset the clock
      else if( (now-helperTime)>=curTime) gotonext=true; //we held for long enough
    }

  }
  else if(curType==3) //step
  {

    if((now-helperTime)>curTime)gotonext=true;
  }
  else if(curType==127) //buzz
  {
    if(now<helperTime)digitalWrite(buzzerPin,HIGH);
    else 
    {
       digitalWrite(buzzerPin,LOW);
       gotonext=true;
    }
  }
  else
  { //unrecognized type, kill the profile
    curProfStep=nProfSteps;
    gotonext=true;
  }

  if(gotonext)
  {
    curProfStep++;
    calcNextProf();
  }
}

void calcNextProf()
{
  if(curProfStep>=nProfSteps) 
  {
    curType=0;
    helperTime =0;
  }
  else
  { 
    curType = proftypes[curProfStep];
    curVal = profvals[curProfStep];
    curTime = proftimes[curProfStep];

  }
  if(curType==1) //ramp
  {
    helperTime = curTime + now; //at what time the ramp will end
    helperVal = setpoint;
  }   
  else if(curType==2) //wait
  {
    helperflag = (curVal==0);
    if(helperflag) helperVal= input-setpoint;
    else helperTime=now; 
  }
  else if(curType==3) //step
  {
    setpoint = curVal;
    helperTime = now;
  }
  else if(curType==127) //buzzer
  {
    helperTime = now + curTime;    
  }
  else
  {
    curType=0;
  }

  if(curType==0) //end
  { //we're done 
    runningProfile=false;
    curProfStep=0;
    Serial.println("P_DN");
    digitalWrite(buzzerPin,LOW);
  } 
  else
  {
    Serial.print("P_STP ");
    Serial.print(int(curProfStep));
    Serial.print(" ");
    Serial.print(int(curType));
    Serial.print(" ");
    Serial.print((curVal));
    Serial.print(" ");
    Serial.println((curTime));
  }

}

/********************************************
 * Serial Communication functions / helpers
 ********************************************/

boolean ackDash = false, ackTune = false;
union {                // This Data structure lets
  byte asBytes[32];    // us take the byte array
  double asFloat[8];    // sent from processing and
}                      // easily convert it to a
foo;                   // double array

// getting double values from processing into the arduino
// was no small task.  the way this program does it is
// as follows:
//  * a double takes up 4 bytes.  in processing, convert
//    the array of floats we want to send, into an array
//    of bytes.
//  * send the bytes to the arduino
//  * use a data structure known as a union to convert
//    the array of bytes back into an array of floats
void SerialReceive()
{

  // read the bytes sent from Processing
  byte index=0;
  byte identifier=0;
  byte b1=255,b2=255;
  boolean boolhelp=false;

  while(Serial.available())
  {
    byte val = Serial.read();
    if(index==0){ 
      identifier = val;
      Serial.println(int(val));
    }
    else 
    {
      switch(identifier)
      {
      case 0: //information request 
        if(index==1) b1=val; //which info type
        else if(index==2)boolhelp = (val==1); //on or off
        break;
      case 1: //dasboard
      case 2: //tunings
      case 3: //autotune
        if(index==1) b1 = val;
        else if(index<14)foo.asBytes[index-2] = val; 
        break;
      case 4: //EEPROM reset
        if(index==1) b1 = val; 
        break;
      case 5: /*//input configuration
        if (index==1)InputSerialReceiveStart();
        InputSerialReceiveDuring(val, index);
        break;
      case 6: //output configuration
        if (index==1)OutputSerialReceiveStart();
        OutputSerialReceiveDuring(val, index);
        break;*/
      case 7:  //receiving profile
        if(index==1) b1=val;
        else if(b1>=nProfSteps) profname[index-2] = char(val); 
        else if(index==2) proftypes[b1] = val;
        else foo.asBytes[index-3] = val;

        break;
      case 8: //profile command
        if(index==1) b2=val;
        break;
      default:
        break;
      }
    }
    index++;
  }

  //we've received the information, time to act
  switch(identifier)
  {
  case 0: //information request
    switch(b1)
    {
    case 0:
      sendInfo = true; 
      sendInputConfig=true;
      sendOutputConfig=true;
      break;
    case 1: 
      sendDash = boolhelp;
      break;
    case 2: 
      sendTune = boolhelp;
      break;
    case 3: 
      sendInputConfig = boolhelp;
      break;
    case 4: 
      sendOutputConfig = boolhelp;
      break;
    default: 
      break;
    }
    break;
  case 1: //dashboard
    if(index==14  && b1<2)
    {
      setpoint=double(foo.asFloat[0]);
      //Input=double(foo.asFloat[1]);       // * the user has the ability to send the 
      //   value of "Input"  in most cases (as 
      //   in this one) this is not needed.
      if(b1==0)                       // * only change the output if we are in 
      {                                     //   manual mode.  otherwise we'll get an
        output=double(foo.asFloat[2]);      //   output blip, then the controller will 
      }                                     //   overwrite.

      if(b1==0) myPID.SetMode(MANUAL);// * set the controller mode
      else myPID.SetMode(AUTOMATIC);             //
      saveEEPROMSettings();
      ackDash=true;
    }
    break;
  case 2: //Tune
    if(index==14 && (b1<=1))
    {
      // * read in and set the controller tunings
      kp = double(foo.asFloat[0]);           //
      ki = double(foo.asFloat[1]);           //
      kd = double(foo.asFloat[2]);           //
      ctrlDirection = b1;
      myPID.SetTunings(kp, ki, kd);            //    
      if(b1==0) myPID.SetControllerDirection(DIRECT);// * set the controller Direction
      else myPID.SetControllerDirection(REVERSE);          //
      saveEEPROMSettings();
      ackTune = true;
    }
    break;
  case 3: //ATune
    if(index==14 && (b1<=1))
    {

      aTuneStep = foo.asFloat[0];
      aTuneNoise = foo.asFloat[1];    
      aTuneLookBack = (unsigned int)foo.asFloat[2];
      if((!tuning && b1==1)||(tuning && b1==0))
      { //toggle autotune state
        changeAutoTune();
      }
      saveEEPROMSettings();
      ackTune = true;   
    }
    break;
  case 4: //EEPROM reset
    if(index==2 && b1<2) clearEEPROM();
    break;
  case 5: /*//input configuration
    InputSerialReceiveAfter(eepromInputOffset);
    sendInputConfig=true;
    break;
  case 6: //ouput configuration
    OutputSerialReceiveAfter(eepromOutputOffset);
    sendOutputConfig=true;
    break;*/
  case 7: //receiving profile

    if((index==11 || (b1>=nProfSteps && index==9) ))
    {
      if(!receivingProfile && b1!=0)
      { //there was a timeout issue.  reset this transfer
        receivingProfile=false;
        Serial.println("ProfError");
//        EEPROMRestoreProfile();
      }
      else if(receivingProfile || b1==0)
      {
        if(runningProfile)
        { //stop the current profile execution
          StopProfile();
        }

        if(b1==0)
        {
          receivingProfile = true;
          profReceiveStart = millis();
        }

        if(b1>=nProfSteps)
        { //getting the name is the last step
          receivingProfile=false; //last profile step
          Serial.print("ProfDone ");
          Serial.println(profname);
          saveEEPROMProfile(0);
          Serial.println("Archived");
        }
        else
        {
          profvals[b1] = foo.asFloat[0];
          proftimes[b1] = (unsigned long)(foo.asFloat[1] * 1000);
          Serial.print("ProfAck ");
          Serial.print(b1);           
          Serial.print(" ");
          Serial.print(proftypes[b1]);           
          Serial.print(" ");
          Serial.print(profvals[b1]);           
          Serial.print(" ");
          Serial.println(proftimes[b1]);           
        }
      }
    }
    break;
  case 8:
    if(index==2 && b2<2)
    {
      if(b2==1) StartProfile();
      else StopProfile();

    }
    break;
  default: 
    break;
  }
}

// unlike our tiny microprocessor, the processing ap
// has no problem converting strings into floats, so
// we can just send strings.  much easier than getting
// floats from processing to here no?
void SerialSend()
{
  if(sendInfo)
  {//just send out the stock identifier
    Serial.print("\nosPID v1.50");
    Serial.print(' ');
    Serial.print(theInputCard.cardIdentifier());
    Serial.print(' ');
    Serial.print(theOutputCard.cardIdentifier());
    Serial.println("");
    sendInfo = false; //only need to send this info once per request
  }
  if(sendDash)
  {
    Serial.print("DASH ");
    Serial.print(setpoint); 
    Serial.print(" ");
    if(isnan(input)) Serial.print("Error");
    else Serial.print(input); 
    Serial.print(" ");
    Serial.print(output); 
    Serial.print(" ");
    Serial.print(myPID.GetMode());
    Serial.print(" ");
    Serial.println(ackDash?1:0);
    if(ackDash)ackDash=false;
  }
  if(sendTune)
  {
    Serial.print("TUNE ");
    Serial.print(myPID.GetKp()); 
    Serial.print(" ");
    Serial.print(myPID.GetKi()); 
    Serial.print(" ");
    Serial.print(myPID.GetKd()); 
    Serial.print(" ");
    Serial.print(myPID.GetDirection()); 
    Serial.print(" ");
    Serial.print(tuning?1:0);
    Serial.print(" ");
    Serial.print(aTuneStep); 
    Serial.print(" ");
    Serial.print(aTuneNoise); 
    Serial.print(" ");
    Serial.print(aTuneLookBack); 
    Serial.print(" ");
    Serial.println(ackTune?1:0);
    if(ackTune)ackTune=false;
  }/*
  if(sendInputConfig)
  {
    Serial.print("IPT ");
    InputSerialSend();
    sendInputConfig=false;
  }
  if(sendOutputConfig)
  {
    Serial.print("OPT ");
    OutputSerialSend();
    sendOutputConfig=false;
  }*/
  if(runningProfile)
  {
    Serial.print("PROF ");
    Serial.print(int(curProfStep));
    Serial.print(" ");
    Serial.print(int(curType));
    Serial.print(" ");
switch(curType)
{
  case 1: //ramp
    Serial.println((helperTime-now)); //time remaining

  break;
  case 2: //wait
    Serial.print(abs(input-setpoint));
    Serial.print(" ");
    Serial.println(curVal==0? -1 : double(now-helperTime));
  break;  
  case 3: //step
    Serial.println(curTime-(now-helperTime));
  break;
  default: 
  break;

}

  }

}









