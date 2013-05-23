#ifndef PID_AutoTune_v0
#define PID_AutoTune_v0
#define LIBRARY_VERSION	0.0.0

// this version has been modified to use fixed-point in places
#include "ospDecimalValue.h"

class PID_ATune
{


  public:
  //commonly used functions **************************************************************************
    PID_ATune(ospDecimalValue<1> *i, ospDecimalValue<1> *o);                       	// * Constructor.  links the Autotune to a given PID
    int Runtime();						   			   	// * Similar to the PID Compue function, returns non 0 when done
	void Cancel();									   	// * Stops the AutoTune	
	
	void SetOutputStep(ospDecimalValue<1> step);						   	// * how far above and below the starting value will the output step?	
	ospDecimalValue<1> GetOutputStep();							   	// 
	
	void SetControlType(int); 						   	// * Determies if the tuning parameters returned will be PI (D=0)
	int GetControlType();							   	//   or PID.  (0=PI, 1=PID)			
	
	void SetLookbackSec(int);							// * how far back are we looking to identify peaks
	int GetLookbackSec();								//
	
	void SetNoiseBand(ospDecimalValue<1>);							// * the autotune will ignore signal chatter smaller than this value
	ospDecimalValue<1> GetNoiseBand();								//   this should be acurately set
	
	double GetKp();										// * once autotune is complete, these functions contain the
	double GetKi();										//   computed tuning parameters.  
	double GetKd();										//
	
  private:
    void FinishUp();
	bool isMax, isMin;
	ospDecimalValue<1> *input, *output;
	ospDecimalValue<1> setpoint;
	ospDecimalValue<1> noiseBand;
	int controlType;
	bool running;
	unsigned long peak1, peak2, lastTime;
	int sampleTime;
	int nLookBack;
	int peakType;
	ospDecimalValue<1> lastInputs[100];
    ospDecimalValue<1> peaks[10];
	int peakCount;
	bool justchanged;
	bool justevaled;
	int initCount;
	ospDecimalValue<1> absMax, absMin;
	ospDecimalValue<1> oStep;
	ospDecimalValue<1> outputStart;
	double Ku, Pu;
	
};
#endif



