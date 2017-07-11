/*
 * Copyright (c) 2017 Danny Backx
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

#include "SimpleL298.h"

SimpleL298::SimpleL298(uint8_t dir1, uint8_t dir2, uint8_t sp) {
  dirPin1 = dir1;
  dirPin2 = dir2;
  speedPin = sp;

  // Configure I/O
  pinMode(dir1, OUTPUT);
  pinMode(dir2, OUTPUT);
  pinMode(speed, OUTPUT);

  // Reset motor
  analogWrite(speedPin, 0);
  digitalWrite(dirPin1, LOW);
  digitalWrite(dirPin2, LOW);
}

void SimpleL298::setMotor(uint8_t dir1, uint8_t dir2, uint8_t speed) {
}

void SimpleL298::run(uint8_t state) {
  analogWrite(speedPin, speed);
  
  switch (state) {
  case FORWARD :
    digitalWrite(dirPin1, HIGH); digitalWrite(dirPin2, LOW);
    break;
  case BACKWARD:
    digitalWrite(dirPin1, LOW); digitalWrite(dirPin2, HIGH);
    break;
  case RELEASE:
    digitalWrite(dirPin1, LOW); digitalWrite(dirPin2, LOW);
    break;
  default:
  case BRAKE:
    digitalWrite(dirPin1, LOW); digitalWrite(dirPin2, LOW);
    break;
  }
}

void SimpleL298::setSpeed(uint8_t speed) {
  this->speed = speed;
}

