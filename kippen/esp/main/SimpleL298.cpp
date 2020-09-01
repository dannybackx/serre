/*
 * Simple motor control via a L298.
 * The ESP32 has stuff for this so let's use it instead of just PWM.
 *
 * Pins on the Alarm PCB OLED connector :
 *  - VCC				(5V, don't touch)
 *  - GND				need to connect
 *  - CS (IO17)				use for speed
 *  - RESET				(don't touch)
 *  - D/C (IO16)			use for A
 *  - SDI/MOSI (IO23)			use for B
 *  - SCK (IO18)
 *  - LED (IO5)
 *  - SDO/MISO (IO19)
 *  - ...
 *
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

#include "SimpleL298.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_attr.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"

extern SimpleL298 *simple;
static const char *sl298_tag = "L298s";

static void example(void *arg) {
  int count = 2;
  while (count-- > 0) {
    simple->setSpeed(50);
    simple->motorForward();
    vTaskDelay(2000 / portTICK_RATE_MS);
    simple->setSpeed(30);
    simple->motorBackward();
    vTaskDelay(2000 / portTICK_RATE_MS);
    simple->motorStop();
    vTaskDelay(2000 / portTICK_RATE_MS);
  }

  ESP_LOGI(sl298_tag, "Stopping task ...");
  vTaskDelay(100 / portTICK_RATE_MS);
  vTaskDelete(0);
}


SimpleL298::SimpleL298(int dir_pin1, int dir_pin2, int speed_pin) {
  ESP_LOGI(l298_tag, "SimpleL298(dir1 %d, dir2 %d, sp %d)", dir_pin1, dir_pin2, speed_pin);

  dirPin1 = dir_pin1;
  dirPin2 = dir_pin2;
  speedPin = speed_pin;

  // GPIO initialization
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, dir_pin1);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, dir_pin2);

  // initial MCPWM configuration
  pwm_config.frequency = 1000;	// 500Hz
  pwm_config.cmpr_a = 0;
  pwm_config.cmpr_b = 0;
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);	// Configure PWM0A and PWM0B with above settings

  // Motor stop
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);

#if 0
  // Stuff from the example in esp-idf
  xTaskCreate(example, "motor control example", 4096, NULL, 5, NULL);
#endif
}

void SimpleL298::motorForward() {
  float duty_cycle = speed;

  ESP_LOGI(l298_tag, "forward");
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, duty_cycle);
  mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
}

void SimpleL298::motorBackward() {
  float duty_cycle = speed;

  ESP_LOGI(l298_tag, "backward");
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, duty_cycle);
  mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
}

void SimpleL298::motorStop() {
  ESP_LOGI(l298_tag, "stop");
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
}

void SimpleL298::setMotor(uint8_t dir1, uint8_t dir2, uint8_t speed) {
}

void SimpleL298::run(uint8_t state) {
  switch (state) {
  case FORWARD :
    motorForward();
    break;
  case BACKWARD:
    motorBackward();
    break;
  case RELEASE:
    motorStop();
    break;
  default:
  case BRAKE:
    motorStop();
    break;
  }
}

void SimpleL298::setSpeed(int speed) {
  this->speed = speed;
}
