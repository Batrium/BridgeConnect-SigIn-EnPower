/*
  SigStateMgmt.h
  Copyright (c) 2016 Batrium Technologies.  All right reserved.

  Author:JaronWare
  22-July-2016

  Contributor:
  Eric Huang
  
  The MIT License (MIT)

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef SigStateMgmt_H
#define SigStateMgmt_H

#include "Arduino.h"
#include <Metro.h>

typedef enum
{
	SIG_STATE_OFF = 0,
	SIG_STATE_COMPLETED_CHARGE = 1,
	SIG_STATE_LOW_CURRENT = 2,
	SIG_STATE_HIGH_CURRENT = 3,
	SIG_STATE_DISABLE = 4,
	SIG_STATE_ENABLE = 5,
	SIG_STATE_UNDEFINED =6,	
} EnumSigState;

class SigStateMgmt
{
public:
	SigStateMgmt();
	~SigStateMgmt();

	void setup(uint8_t pinNum);
	void loop();

	uint32_t ReadPulseFreqHz();
	EnumSigState ReadSigState();

private:
	Metro updateTimer = Metro(100);// milliseconds
	elapsedMillis countTime_mS = 0;
	uint32_t pulseFreqHz = 0;
};

//-----------------------------------------------------------------------------

/* constructor */
SigStateMgmt::SigStateMgmt()
{
}

/* destructor */
SigStateMgmt::~SigStateMgmt()
{
}

static volatile uint32_t pulseCount = 0;

// pulse input interrupt service routine
static void PulseInISR()
{
	pulseCount++;
}

void SigStateMgmt::setup(uint8_t pulseInPin)
{
	pinMode(pulseInPin, INPUT);
	attachInterrupt(pulseInPin, PulseInISR, RISING);
	countTime_mS = 0; // reset timer	
}

void SigStateMgmt::loop()
{
	if (updateTimer.check())
	{
		uint32_t pulseCountCopy;

		cli(); // enter critical area
		pulseCountCopy = pulseCount;
		pulseCount = 0; // reset counter
		sei(); // exit critical area

		pulseFreqHz = pulseCountCopy * 1000 / countTime_mS;
		countTime_mS = 0; // reset timer		
	}
}

uint32_t SigStateMgmt::ReadPulseFreqHz()
{
	return pulseFreqHz;
}

EnumSigState SigStateMgmt::ReadSigState()
{
	if (pulseFreqHz == 0) return SIG_STATE_OFF;
	//else if (pulseFreqHz <= 50) return SIG_STATE_ENABLE;
	else if (pulseFreqHz <= 150) return SIG_STATE_COMPLETED_CHARGE; // ~ 125
	else if (pulseFreqHz <= 350) return SIG_STATE_LOW_CURRENT;      // ~ 250 
	else if (pulseFreqHz <= 550) return SIG_STATE_HIGH_CURRENT;     // ~ 500

	return SIG_STATE_UNDEFINED;
}

#endif

