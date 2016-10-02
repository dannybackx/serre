#include <avr/pgmspace.h>

const char timedate_fmt[] PROGMEM = "%02d:%02d:%02d %02d/%02d/%04d";
const char sensor_up_string[] PROGMEM = "Sensor UP";
const char sensor_down_string[] PROGMEM = "Sensor DOWN";
const char button_up_string[] PROGMEM = "Button UP";
const char button_down_string[] PROGMEM = "Button Down";
const char bmp_fmt[] PROGMEM = "bmp (%2d.%02d, %d)";
const char no_sensor_string[] PROGMEM = "No sensor detected";
const char rcmd_time_set[] PROGMEM = "/arduino/digital/time/set/";
const char setting_rtc[] PROGMEM = "Setting RTC ";
const char initializing_bmp180[] PROGMEM = "Initializing BMP180 ... ";
const char starting_wifi[] PROGMEM = "Starting WiFi ...";
const char startup_text1[] PROGMEM = "Arduino Uno Wifi Chicken Hatch";
const char startup_text2[] PROGMEM = "(c) 2016 by Danny Backx";


char		progmem_bfr[32];

const char *gpm(const char *p)
{
  strcpy_P(progmem_bfr, p);
  return &progmem_bfr[0];
}

