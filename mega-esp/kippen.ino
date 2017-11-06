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
#include <Wire.h>

#include <ELClient.h>
#include <ELClientCmd.h>
#include <ELClientMqtt.h>
#include <ELClientRest.h>

#include "SFE_BMP180.h"
#include <TimeLib.h>
#include "Hatch.h"
#include "Light.h"
#include "ThingSpeak.h"
#include <DS1307RTC.h>
#include "Ifttt.h"
#include <Dyndns.h>
#include "Sunset.h"
#include "global.h"

#include "buildinfo.h"

time_t		boot_time = 0;
time_t		nowts;

SFE_BMP180	*bmp = 0;
double		newPressure, newTemperature, oldPressure, oldTemperature;
char		buffer[buffer_size];

Hatch		*hatch = 0;
Light		*light = 0;
int		newhatch, oldhatch;
enum lightState	newlight, oldlight, sun;
int		sensor_up, sensor_down, button_up, button_down;

int		test = 1;		// Default is NOT production

ThingSpeak	*ts = 0;
Sunset		*sunset = 0;

ELClient	esp(&Serial, &Serial);
ELClientMqtt	mqtt(&esp);
ELClientCmd	cmd(&esp);

char		*mqtt_topic = 0;

void NoIP(int);

/*
 * Helper function because the ELClientCmd doesn't have a static function for this.
 */
time_t mygettime() {
  return cmd.GetTime();
}

/*
 * Application startup on Arduino :
 */
void setup() {
  delay(2000);

  // Needs to be in sync with esp-link's baud rate
  Serial.begin(115200);

  Serial.println("Yow !");

  // Display startup text
  Serial.print("\nArduino Mega Wifi Chicken Hatch, build ");
  Serial.print(_BuildInfo.date);
  Serial.print(" ");
  Serial.println(_BuildInfo.time);
  Serial.println("(c) 2016-2017 by Danny Backx\n");

  // Start WiFi
  Serial.println("\nStarting WiFi ...");

  esp.wifiCb.attach(WifiStatusCallback);
  while (! esp.Sync()) {
    Serial.println("ELClient sync failed");
  }

  // Query MQTT name, we'll use this to steer debug/production behaviour
  // Note this is in the ESP-link config so you can configure per device.

  Serial.print("MQTT client id : ");
  char *mqtt_clientid = cmd.mqttGetClientId();
  Serial.print(mqtt_clientid);
  mqtt_topic = mqtt_clientid;

  // Debug behaviour is defined in the table esplist, e.g. one is called "testesp".
  for (int i=0; esplist[i].name; i++)
    if (strcmp(esplist[i].name, mqtt_clientid) == 0) {
      test = esplist[i].test;
      break;
    }

  if (test)
    Serial.println(" -> test");
  else
    Serial.println(" -> production");

  // Register with NoIP.com
  // This passes along whether to register with a debug hostname
  NoIP(test);

  // BMP180 temperature and air pressure sensor
  Serial.print("Initializing BMP180 ... ");
  BMPInitialize();
  Serial.println(bmp ? "ok" : "failed");

  // Hatch
  sprintf(buffer, "Set up hatch ... motor control on pins (dir) %d, %d, (speed) %d\n",
    motor_dir1_pin, motor_dir2_pin, motor_speed_pin);
  Serial.print(buffer);

  hatch = new Hatch(schedule_string);
  hatch->setMotor(motor_dir1_pin, motor_dir2_pin, motor_speed_pin);
  hatch->setMaxTime(6);

  newhatch = oldhatch = 0;

  // Time
  // setSyncProvider(RTC.get);	// Set the function to get time from RTC
  setSyncProvider(mygettime);	// Set the function to get time from the ESP's esp-link
  if (timeStatus() != timeSet)
    Serial.println("Unable to sync with the RTC");
  else {
    Serial.print("Clock ok, ");      
    boot_time = now();
    sprintf(buffer, "%02d:%02d:%02d %02d/%02d/%04d",
      hour(), minute(), second(), day(), month(), year());
    Serial.println(buffer); 

    // Note the time info in the buffer will be re-used below.
  }

  // Sensors and buttons
  ActivatePin(sensor_up_pin, "Sensor UP");
  ActivatePin(sensor_down_pin, "Sensor DOWN");
  ActivatePin(button_up_pin, "Button UP");
  ActivatePin(button_down_pin, "Button DOWN");

  // Sunset query
  sunset = new Sunset();
  sunset->query(sunset_latitude, sunset_longitude);

  ActivatePin(light_sensor_pin, "Light sensor");
  light = new Light();
  light->setSensorPin(light_sensor_pin);

  // Initialize sensor states
  sensor_up = sensor_down = button_up = button_down = -1;
  if (sensor_up_pin >= 0)  {
    sensor_up = ReadPin(sensor_up_pin);
  }
  if (sensor_down_pin >= 0) {
    sensor_down = ReadPin(sensor_down_pin);
  }

  /*
   * Analog Hall Effect sensors (such as SS49E) have an idle value around 500.
   * When approached by a magnet, the value moves away from that. The direction
   * depends on the polarity of the magnetic field.
   */
#define	HALL_ANALOG_SENSOR_ACTIVE(x) \
	(  ((x - hall_sensor_idle) > hall_sensor_trigger)	\
	|| ((hall_sensor_idle - x ) > hall_sensor_trigger) )

  if (HALL_ANALOG_SENSOR_ACTIVE(sensor_down))
    hatch->IsDown();
  if (HALL_ANALOG_SENSOR_ACTIVE(sensor_up))
    hatch->IsUp();

  if (button_up_pin >= 0)
    button_up = ReadPin(button_up_pin);
  if (button_down_pin >= 0)
    button_down = ReadPin(button_down_pin);

  Debug("Button states : up %d down %d", button_up, button_down);

  // Serial.println("Starting ThingSpeak...");
  ts = new ThingSpeak(test);

#ifdef USE_IFTTT
  Ifttt *ifttt = new Ifttt();
  ifttt->sendEvent(gpm(ifttt_key), (char *)ifttt_event, "Boot");
#endif

  // Share our IP address
  uint32_t ip, nm, gw;
  cmd.GetWifiInfo(&ip, &nm, &gw);

  // FIX ME in reverse
  int ip1 = (ip & 0x000000FF);
  int ip2 = (ip & 0x0000FF00) >> 8;
  int ip3 = (ip & 0x00FF0000) >> 16;
  int ip4 = (ip & 0xFF000000) >> 24;

  // Print several things to MQTT, re-use the current time from above.
  char *p = buffer + strlen(buffer);
  sprintf(p, ", IP address %d.%d.%d.%d, %s, build %s %s",
    ip1, ip2, ip3, ip4,
    test ? "test" : "production",
    _BuildInfo.date, _BuildInfo.time);
  Serial.println(buffer);

  // Set up MQTT
  mqtt.connectedCb.attach(mqConnected);
  mqtt.disconnectedCb.attach(mqDisconnected);
  mqtt.dataCb.attach(mqData);
  mqtt.setup();

  // hack
  mqConnected(0);

  // On MQTT, say not only that we're ready but specify IP address also
  mqttSend(buffer);

  // Yeah !
  Serial.print("Ready ... ");

  delay(3000);
  Serial.println(" !");

  StartTrackStatus();
}

/*********************************************************************************
 *                                                                               *
 * Pin management                                                                *
 *                                                                               *
 *********************************************************************************/
void ActivatePin(int pin, const char *name) {
  if (pin >= 0) {
    if (pin > NUM_DIGITAL_PINS - NUM_ANALOG_INPUTS) {	// Analog
      Serial.print(name);
      Serial.print(F(" is on analog pin "));
      Serial.print(pin - (NUM_DIGITAL_PINS - NUM_ANALOG_INPUTS));
      Serial.print(" (");
      Serial.print(pin);
      Serial.println(")");
      pinMode(sensor_up_pin, INPUT);
    } else {						// Digital
      Serial.print(name);
      Serial.print(F(" is on pin "));
      Serial.println(pin);
      pinMode(sensor_up_pin, INPUT_PULLUP);
    }
  } else {
    Serial.print(F("No "));
    Serial.println(name);
  }
}

int ReadPin(int pin) {
  if (pin >= 0) {
    if (pin > NUM_DIGITAL_PINS - NUM_ANALOG_INPUTS) {	// Analog
      return analogRead(pin);
    } else {						// Digital
      return digitalRead(pin);
    }
  }
  return -1;	// error
}

/*********************************************************************************
 *                                                                               *
 * Loop : Arduino keeps running this                                             *
 *                                                                               *
 *********************************************************************************/
void loop() {
  esp.Process();

  nowts = now();
  
  // Query Sunset, feed value into Light
  sun = sunset->loop(nowts);

  // Has the light changed ?
  oldlight = newlight;
  newlight = light->loop(nowts, sun);

  // FIXME need to average values over e.g. a five minute period

  if (oldlight != newlight) {
    if (newlight == LIGHT_MORNING)
      hatch->Up(hour(nowts), minute(nowts), second(nowts), "morning");
    else if (newlight == LIGHT_EVENING)
      hatch->Down(hour(nowts), minute(nowts), second(nowts), "evening");
  }

  // Note the hatch->loop code will also trigger the motor
  if (hatch != NULL) {
    oldhatch = newhatch;
    newhatch = hatch->loop(hour(nowts), minute(nowts), second(nowts));
  }
#if 0
  // Sensors stop motion
  if (sensor_up_pin >= 0) {
    int oldvalue = sensor_up;
    sensor_up = ReadPin(sensor_up_pin);
    if (oldvalue != sensor_up && sensor_up == 1) {
      // Stop moving the hatch
      hatch->Stop(hour(nowts), minute(nowts), second(nowts));
      hatch->IsUp();
    }
  }
  if (sensor_down_pin >= 0) {
    int oldvalue = sensor_up;
    sensor_down = ReadPin(sensor_down_pin);
    if (oldvalue != sensor_down && sensor_down == 1) {
      // Stop moving the hatch
      hatch->Stop(hour(nowts), minute(nowts), second(nowts));
      hatch->IsDown();
    }
  }
#endif
  // Read the button states
  int old_button_up = button_up,
      old_button_down = button_down;

  if (button_up_pin >= 0)
    button_up = ReadPin(button_up_pin);
  if (button_down_pin >= 0)
    button_down = ReadPin(button_down_pin);

  // Action :-)
  // Note buttons are wired between input pin and LOW voltage,
  // pins are programmed to have a pull up resistor.
  // So active low.
#if 0
  // Buttons start motion
  if (button_up_pin >= 0) {
    if (old_button_up != button_up && button_up == 1) {
      // Move the hatch up
      // hatch->Up();
      hatch->Up(hour(nowts), minute(nowts), second(nowts));
    }
  }
  if (button_down_pin >= 0) {
    if (old_button_down != button_down && button_down == 1) {
      // Move the hatch down
      // hatch->Down();
      hatch->Down(hour(nowts), minute(nowts), second(nowts));
    }
  }
#endif
  ts->loop(nowts);

  delay(loop_delay);
}
 
void BMPInitialize() {
  bmp = new SFE_BMP180();
  char ok = bmp->begin();	// 0 return value is a problem
  if (ok == 0) {
    delete bmp;
    bmp = 0;
  }
}

/*
 * Returns 0 on success, negative values on error
 * -1 not initialized
 * -2xx nan
 * -3 communication error
 */
int BMPQuery() {
  bool nan = false;

  if (bmp == 0)
    return -1;

  // This is a multi-part query to the I2C device, see the SFE_BMP180 source files.
  char d1 = bmp->startTemperature();
  delay(d1);

  char d2 = bmp->getTemperature(newTemperature);
  if (d2 == 0) { // Error communicating with device
#ifdef DEBUG
    DEBUG.printf("BMP180 : communication error (temperature)\n");
#endif
    return -201;
  }

  char d3 = bmp->startPressure(0);
  delay(d3);
  char d4 = bmp->getPressure(newPressure, newTemperature);
  if (d4 == 0) { // Error communicating with device
#ifdef DEBUG
    DEBUG.printf(F("BMP180 : communication error (pressure)\n"));
#endif
    return -202;
  }

  if (isnan(newTemperature) || isinf(newTemperature)) {
    newTemperature = oldTemperature;
    nan = true;
  }
  if (isnan(newPressure) || isinf(newPressure)) {
    newPressure = oldPressure;
    nan = true;
  }
  if (nan) {
#ifdef DEBUG
    DEBUG.println(F("BMP180 nan"));
#endif
    return -203;
  }
  return 0;
}

void mqttSend(const char *msg) {
  mqtt.publish(mqtt_topic, (char *)msg);
}

/*
 * Register yourself with a debug hostname depending on parameter
 *
 * You can use this to have the test and production sites on different networks
 */
void NoIP(int test) {
  Serial.print("Registering with no-ip.com ... ");
  Dyndns *d = new Dyndns();
  if (test) 
    d->setHostname(test_noip_hostname);
  else
    d->setHostname(noip_hostname);
  d->setAuth(noip_auth);	// base64 of user:password
  d->update();
  Serial.println("done");
}

// Send a line of debug info to MQTT
void Debug(const char *format, ...)
{
  char buffer[128];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buffer, 128, format, ap);
  va_end(ap);
  mqtt.publish(mqtt_topic, buffer);
}

