#ifndef OSPPIDLOOP_H
#define OSPPIDLOOP_H

#include "ospDecimalValue.h"

/*
 * This class implements a fixed-point PID loop.
 */

struct ospPIDLoop {
public:
  // the loop iterates at 10 Hz
  enum { LOOP_CYCLE_MILLISECONDS = 100 };

  ospDecimalValue<3> PGain, IGain, DGain;

  ospDecimalValue<2> integrator;
  ospDecimalValue<1> previousInput;
  bool invertAction;

  // call this function every LOOP_CYCLE_MILLISECONDS to update the loop
  ospDecimalValue<1> updateController(ospDecimalValue<1> setpoint, ospDecimalValue<1> input);
};

inline ospDecimalValue<1> ospPIDLoop::updateController(ospDecimalValue<1> setpoint, ospDecimalValue<1> input)
{
  ospDecimalValue<1> delta = (makeDecimal<0>(1000 / LOOP_CYCLE_MILLISECONDS) * (input - previousInput)).rescale<1>();
  previousInput = input;
  ospDecimalValue<1> derivative = (delta * DGain).rescale<1>();

  ospDecimalValue<1> error = setpoint - input;
  ospDecimalValue<1> proportional = (error * PGain).rescale<1>();

  integrator += (IGain * error).rescale<2>();

  // anti-windup: clamp the integrator by the output limits of [0, 100.0]
  if (integrator < makeDecimal<2>(0))
    integrator = makeDecimal<2>(0);
  else if (integrator > makeDecimal<2>(10000))
    integrator = makeDecimal<2>(10000);

  ospDecimalValue<1> command = proportional + integrator.rescale<1>() - derivative;

  if (invertAction)
    command = -command;

  if (command < makeDecimal<1>(0))
    command = makeDecimal<1>(0);
  else if (command > makeDecimal<1>(1000))
    command = makeDecimal<1>(1000);

  return command;
}

#endif

