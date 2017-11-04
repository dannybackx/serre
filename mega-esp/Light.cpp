/*
 * Copyright (c) 2016, 2017 Danny Backx
 *
 * License (MIT license):
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *   THE SOFTWARE.
 */

/*
 * This module tracks a light sensor, and detects daylight / night situation.
 * The loop() function provides feedback to indicate when to move the hatch,
 * and indicates whether it's day or night.
 */
#include <Arduino.h>

#include <ELClient.h>
#include <ELClientCmd.h>
#include <ELClientMqtt.h>

#include "TimeLib.h"
#include "Light.h"
#include "Hatch.h"
#include "SFE_BMP180.h"
#include "ThingSpeak.h"
#include "Sunset.h"
#include "global.h"

Light::Light() {
  sensorPin = -1;
  lowTreshold = light_treshold_low;
  highTreshold = light_treshold_high;
  duration = light_min_duration;

  lastChange = stableTime = 0;
  stableValue = currentValue = LIGHT_NONE;
}

Light::~Light() {
}

enum lightState Light::loop(time_t t, enum lightState sun) {
  if (sensorPin < 0)
    return LIGHT_NONE;

  int sensorValue = analogRead(sensorPin);

  if (stableValue == LIGHT_NONE) {
    /*
     * Startup : get a sensible value to begin with
     *	the parameter is from Sunset::loop so gives time-based initialisation
     */
    stableTime = t;

    // Don't put transient states in here ...
    if (sun == LIGHT_MORNING) stableValue = LIGHT_NIGHT;
    else if (sun == LIGHT_EVENING) stableValue = LIGHT_DAY;
    else stableValue = sun;

  }
  
  // Transient states happen only once, so take precedence
  if (sun == LIGHT_MORNING) {
    stableValue = LIGHT_NIGHT;
    return sun;
  } else if (sun == LIGHT_EVENING) {
    stableValue = LIGHT_DAY;
    return sun;

  // Now take light into account
  } else if (stableValue == LIGHT_NIGHT) {
    if (sensorValue < lowTreshold) {
      // Don't do anything
    } else if (sensorValue > highTreshold) {
      // It's light. Figure out if it's for real.
      if (lastChange < 0) {
        lastChange = t;
      } else if (t - lastChange > duration) {	// Also for lastChange == 0
        // This is a real change !
        stableValue = LIGHT_DAY;
	stableTime = t;

	return LIGHT_MORNING;
      }
    } else {
      // Between low and high tresholds --> don't do anything
    }
  } else if (stableValue == LIGHT_DAY) {
    if (sensorValue > highTreshold) {
      // Don't do anything
    } else if (sensorValue < lowTreshold) {
      // It's dark. Figure out if it's for real.
      if (lastChange < 0) {
        lastChange = t;
      } else if (t - lastChange > duration) {	// Also for lastChange == 0
        // This is a real change !
        stableValue = LIGHT_NIGHT;
	stableTime = t;

	return LIGHT_EVENING;
      }
    } else {
      // Between low and high tresholds --> don't do anything
    }
  }
  // Ignore other (transient) states

  return stableValue;
}

void Light::setSensorPin(int n) {
  sensorPin = n;
}

int Light::getSensorPin() {
  return sensorPin;
}

void Light::setLowTreshold(int n) {
  lowTreshold = n;
}

int Light::getLowTreshold() {
  return lowTreshold;
}

void Light::setHighTreshold(int n) {
  highTreshold = n;
}

int Light::getHighTreshold() {
  return highTreshold;
}

void Light::setDuration(int n) {
  duration = n;
}

int Light::getDuration() {
  return duration;
}

int Light::query() {
  if (sensorPin < 0)
    return -1;

  return analogRead(sensorPin);
}
