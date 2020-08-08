/*
 * Copyright (c) 2017, 2020 Danny Backx
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

#ifndef SIMPLE_L298
#define SIMPLE_L298

#include <esp_log.h>
#include <esp_system.h>
#include <esp_types.h>

#include "esp_attr.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"

#define FORWARD 1
#define BACKWARD 2
#define BRAKE 3
#define RELEASE 4

class SimpleL298
{
 public:
  SimpleL298(int dir_pin1, int dir_pin2, int speed_pin);
  void setMotor(uint8_t dirPin1, uint8_t dirPin2, uint8_t speedPin);
  void run(uint8_t);
  void setSpeed(int);

  void motorForward();
  void motorBackward();
  void motorStop();

 private:
  int			dirPin1, dirPin2, speedPin;
  int			speed;
  mcpwm_config_t	pwm_config;

  const char *l298_tag = "L298";
};
#endif
