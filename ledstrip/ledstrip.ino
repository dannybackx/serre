/*
 * Copyright (c) 2020 Danny Backx
 *
 * Small program to drive a couple of RGB LED strips for the winter period.
 * Currently contains basic code to talk to wifi, mqtt, and allow OTA.
 *
 * First piece of code is just FastLED's XYMatrix example, to be extended.
 */
#include <FastLED.h>
#undef DEBUG

void DrawOneFrame( byte startHue8, int8_t yHueDelta8, int8_t xHueDelta8);

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <TimeLib.h>
#include <stdarg.h>

extern "C" {
#include <sntp.h>
#include <time.h>
}

// Prepare for OTA software installation
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
static int OTAprev;

#include "secrets.h"
#include "personal.h"
#include "buildinfo.h"

struct mywifi {
  const char *ssid, *pass;
} mywifi[] = {
  { MY_SSID_1, MY_WIFI_PASSWORD_1 },
  { MY_SSID_2, MY_WIFI_PASSWORD_2 },
  { MY_SSID_3, MY_WIFI_PASSWORD_3 },
  { MY_SSID_4, MY_WIFI_PASSWORD_4 },
  { MY_SSID_5, MY_WIFI_PASSWORD_5 },
  { NULL, NULL}
};

// MQTT
WiFiClient	espClient;
PubSubClient	client(espClient);
time_t mySntpInit();
void reconnect(void);
void callback(char * topic, byte *payload, unsigned int length);
bool IsDST(int day, int month, int dow);
void Debug(const char *format, ...);
void RelayDebug(const char *format, ...);

#define	VERBOSE_OTA	0x01
#define	VERBOSE_VALVE	0x02
#define	VERBOSE_BMP	0x04
#define	VERBOSE_4	0x08

const char* mqtt_server = MQTT_HOST;
const int mqtt_port = MQTT_PORT;

const char *mqtt_topic = SWITCH_TOPIC;
const char *reply_topic = SWITCH_TOPIC "/reply";
const char *relay_topic = SWITCH_TOPIC "/relay";

int mqtt_initial = 1;
int _isdst = 0;

// Forward
bool IsDST(int day, int month, int dow);
bool IsDST2(int day, int month, int dow, int hr);
char *GetSchedule();
void SetSchedule(const char *desc);
void PinOff();

void RelayDebug(const char *format, ...);
void Debug(const char *format, ...);

// All kinds of variables
int		verbose = 0;
String		ips, gws;
int		state = 0;
int		manual = 0;


#define LED_PIN  3

#define COLOR_ORDER GRB
#define CHIPSET     WS2811

#define BRIGHTNESS 64

// Helper functions for an two-dimensional XY matrix of pixels.
// Simple 2-D demo code is included as well.
//
//     XY(x,y) takes x and y coordinates and returns an LED index number,
//             for use like this:  leds[ XY(x,y) ] == CRGB::Red;
//             No error checking is performed on the ranges of x and y.
//
//     XYsafe(x,y) takes x and y coordinates and returns an LED index number,
//             for use like this:  leds[ XY(x,y) ] == CRGB::Red;
//             Error checking IS performed on the ranges of x and y, and an
//             index of "-1" is returned.  Special instructions below
//             explain how to use this without having to do your own error
//             checking every time you use this function.  
//             This is a slightly more advanced technique, and 
//             it REQUIRES SPECIAL ADDITIONAL setup, described below.


// Params for width and height
const uint8_t kMatrixWidth = 16;
const uint8_t kMatrixHeight = 16;

// Param for different pixel layouts
const bool    kMatrixSerpentineLayout = true;
const bool    kMatrixVertical = false;
// Set 'kMatrixSerpentineLayout' to false if your pixels are 
// laid out all running the same way, like this:
//
//     0 >  1 >  2 >  3 >  4
//                         |
//     .----<----<----<----'
//     |
//     5 >  6 >  7 >  8 >  9
//                         |
//     .----<----<----<----'
//     |
//    10 > 11 > 12 > 13 > 14
//                         |
//     .----<----<----<----'
//     |
//    15 > 16 > 17 > 18 > 19
//
// Set 'kMatrixSerpentineLayout' to true if your pixels are 
// laid out back-and-forth, like this:
//
//     0 >  1 >  2 >  3 >  4
//                         |
//                         |
//     9 <  8 <  7 <  6 <  5
//     |
//     |
//    10 > 11 > 12 > 13 > 14
//                        |
//                        |
//    19 < 18 < 17 < 16 < 15
//
// Bonus vocabulary word: anything that goes one way 
// in one row, and then backwards in the next row, and so on
// is call "boustrophedon", meaning "as the ox plows."


// This function will return the right 'led index number' for 
// a given set of X and Y coordinates on your matrix.  
// IT DOES NOT CHECK THE COORDINATE BOUNDARIES.  
// That's up to you.  Don't pass it bogus values.
//
// Use the "XY" function like this:
//
//    for( uint8_t x = 0; x < kMatrixWidth; x++) {
//      for( uint8_t y = 0; y < kMatrixHeight; y++) {
//      
//        // Here's the x, y to 'led index' in action: 
//        leds[ XY( x, y) ] = CHSV( random8(), 255, 255);
//      
//      }
//    }
//
//
uint16_t XY( uint8_t x, uint8_t y)
{
  uint16_t i;
  
  if( kMatrixSerpentineLayout == false) {
    if (kMatrixVertical == false) {
      i = (y * kMatrixWidth) + x;
    } else {
      i = kMatrixHeight * (kMatrixWidth - (x+1))+y;
    }
  }

  if( kMatrixSerpentineLayout == true) {
    if (kMatrixVertical == false) {
      if( y & 0x01) {
        // Odd rows run backwards
        uint8_t reverseX = (kMatrixWidth - 1) - x;
        i = (y * kMatrixWidth) + reverseX;
      } else {
        // Even rows run forwards
        i = (y * kMatrixWidth) + x;
      }
    } else { // vertical positioning
      if ( x & 0x01) {
        i = kMatrixHeight * (kMatrixWidth - (x+1))+y;
      } else {
        i = kMatrixHeight * (kMatrixWidth - x) - (y+1);
      }
    }
  }
  
  return i;
}


// Once you've gotten the basics working (AND NOT UNTIL THEN!)
// here's a helpful technique that can be tricky to set up, but 
// then helps you avoid the needs for sprinkling array-bound-checking
// throughout your code.
//
// It requires a careful attention to get it set up correctly, but
// can potentially make your code smaller and faster.
//
// Suppose you have an 8 x 5 matrix of 40 LEDs.  Normally, you'd
// delcare your leds array like this:
//    CRGB leds[40];
// But instead of that, declare an LED buffer with one extra pixel in
// it, "leds_plus_safety_pixel".  Then declare "leds" as a pointer to
// that array, but starting with the 2nd element (id=1) of that array: 
//    CRGB leds_with_safety_pixel[41];
//    CRGB* const leds( leds_plus_safety_pixel + 1);
// Then you use the "leds" array as you normally would.
// Now "leds[0..N]" are aliases for "leds_plus_safety_pixel[1..(N+1)]",
// AND leds[-1] is now a legitimate and safe alias for leds_plus_safety_pixel[0].
// leds_plus_safety_pixel[0] aka leds[-1] is now your "safety pixel".
//
// Now instead of using the XY function above, use the one below, "XYsafe".
//
// If the X and Y values are 'in bounds', this function will return an index
// into the visible led array, same as "XY" does.
// HOWEVER -- and this is the trick -- if the X or Y values
// are out of bounds, this function will return an index of -1.
// And since leds[-1] is actually just an alias for leds_plus_safety_pixel[0],
// it's a totally safe and legal place to access.  And since the 'safety pixel'
// falls 'outside' the visible part of the LED array, anything you write 
// there is hidden from view automatically.
// Thus, this line of code is totally safe, regardless of the actual size of
// your matrix:
//    leds[ XYsafe( random8(), random8() ) ] = CHSV( random8(), 255, 255);
//
// The only catch here is that while this makes it safe to read from and
// write to 'any pixel', there's really only ONE 'safety pixel'.  No matter
// what out-of-bounds coordinates you write to, you'll really be writing to
// that one safety pixel.  And if you try to READ from the safety pixel,
// you'll read whatever was written there last, reglardless of what coordinates
// were supplied.

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
CRGB leds_plus_safety_pixel[ NUM_LEDS + 1];
CRGB* const leds( leds_plus_safety_pixel + 1);

uint16_t XYsafe( uint8_t x, uint8_t y)
{
  if( x >= kMatrixWidth) return -1;
  if( y >= kMatrixHeight) return -1;
  return XY(x,y);
}


// Demo that USES "XY" follows code below

void loop()
{
  ArduinoOTA.handle();
  // MQTT
  if (!client.connected()) {
    reconnect();
  }
  if (!client.loop())
    reconnect();

    uint32_t ms = millis();
    int32_t yHueDelta32 = ((int32_t)cos16( ms * (27/1) ) * (350 / kMatrixWidth));
    int32_t xHueDelta32 = ((int32_t)cos16( ms * (39/1) ) * (310 / kMatrixHeight));
    DrawOneFrame( ms / 65536, yHueDelta32 / 32768, xHueDelta32 / 32768);
    if( ms < 5000 ) {
      FastLED.setBrightness( scale8( BRIGHTNESS, (ms * 256) / 5000));
    } else {
      FastLED.setBrightness(BRIGHTNESS);
    }
    FastLED.show();
}

void DrawOneFrame( byte startHue8, int8_t yHueDelta8, int8_t xHueDelta8)
{
  byte lineStartHue = startHue8;
  for( byte y = 0; y < kMatrixHeight; y++) {
    lineStartHue += yHueDelta8;
    byte pixelHue = lineStartHue;      
    for( byte x = 0; x < kMatrixWidth; x++) {
      pixelHue += xHueDelta8;
      leds[ XY(x, y)]  = CHSV( pixelHue, 255, 255);
    }
  }
}


void setup() {
  Serial.begin(115200);
  Serial.print("XYMatrix starting");

  // Try to connect to WiFi
  WiFi.mode(WIFI_STA);

  int wcr = WL_IDLE_STATUS;
  for (int ix = 0; wcr != WL_CONNECTED && mywifi[ix].ssid != NULL; ix++) {
    int wifi_tries = 3;
    while (wifi_tries-- >= 0) {
      Serial.printf("\nTrying (%d %d) %s %s .. ", ix, wifi_tries, mywifi[ix].ssid, mywifi[ix].pass);
      WiFi.begin(mywifi[ix].ssid, mywifi[ix].pass);
      wcr = WiFi.waitForConnectResult();
      if (wcr == WL_CONNECTED)
        break;
    }
  }

  // Reboot if we didn't manage to connect to WiFi
  if (wcr != WL_CONNECTED) {
    delay(2000);
    ESP.restart();
  }

  IPAddress ip = WiFi.localIP();
  ips = ip.toString();
  IPAddress gw = WiFi.gatewayIP();
  gws = gw.toString();
  Serial.printf("Connected : ip %s, gw %s\n", ips.c_str(), gws.c_str());

  // Set up real time clock
  // Note : DST processing comes later
  (void)sntp_set_timezone(MY_TIMEZONE);
  sntp_init();
  sntp_setservername(0, (char *)"ntp.telenet.be");
  sntp_setservername(1, (char *)"ntp.belnet.be");

  ArduinoOTA.onStart([]() {
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
#ifdef DEBUG
      Serial.printf("OTA start\n");
#endif
      client.publish(reply_topic, "OTA start");
    }
  });

  ArduinoOTA.onEnd([]() {
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
#ifdef DEBUG
      Serial.printf("OTA complete\n");
#endif
      client.publish(reply_topic, "OTA complete");
    }
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int curr;
    curr = (progress / (total / 50));
    OTAprev = curr;
  });

  ArduinoOTA.onError([](ota_error_t error) {
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
#ifdef DEBUG
      Serial.printf("OTA error\n");
#endif
      client.publish(reply_topic, "OTA Error");
    }
  });

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(OTA_ID);
  WiFi.hostname(OTA_ID);

  ArduinoOTA.begin();

  time_t t = mySntpInit();

  // MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Note don't use MQTT here yet, we're probably not connected yet
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness( BRIGHTNESS );
}

time_t mySntpInit() {
  time_t t;

  // Wait for a correct time, and report it

  t = sntp_get_current_timestamp();
  while (t < 100000) {
    delay(1000);
    t = sntp_get_current_timestamp();
  }

  // DST handling
  if (IsDST(day(t), month(t), dayOfWeek(t))) {
    _isdst = 1;

    // Set TZ again
    sntp_stop();
    (void)sntp_set_timezone(MY_TIMEZONE + 1);

    Debug("mySntpInit(day %d, month %d, dow %d) : DST = %d, timezone %d", day(t), month(t), dayOfWeek(t), _isdst, MY_TIMEZONE + 1);

    // Re-initialize/fetch
    sntp_init();
    t = sntp_get_current_timestamp();
    while (t < 0x1000) {
      delay(1000);
      t = sntp_get_current_timestamp();
    }

    // Debug("Checkup : day %d month %d time %02d:%02d:%02d, timezone %d", day(t), month(t), hour(t), minute(t), second(t), sntp_get_timezone());
  } else {
    t = sntp_get_current_timestamp();
    Debug("mySntpInit(day %d, month %d, dow %d) : DST = %d", day(t), month(t), dayOfWeek(t), _isdst);
  }

  return t;
}

void reconnect(void) {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Attempt to connect
    if (client.connect(MQTT_CLIENT)) {
      // Get the time
      time_t t = mySntpInit();

      // Once connected, publish an announcement...
      if (mqtt_initial) {
	Debug("boot %04d.%02d.%02d %02d:%02d:%02d, timezone is set to %d",
	  year(t), month(t), day(t), hour(t), minute(t), second(t), sntp_get_timezone());

	mqtt_initial = 0;
      } else {
        Debug("reconnect %04d.%02d.%02d %02d:%02d:%02d",
	  year(t), month(t), day(t), hour(t), minute(t), second(t));
      }

      // ... and (re)subscribe
      client.subscribe(SWITCH_TOPIC);
      client.subscribe(SWITCH_TOPIC "/program");
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char * topic, byte *payload, unsigned int length) {
}

bool IsDST(int day, int month, int dow)
{
  dow--;	// Convert this to POSIX convention (Day Of Week = 0-6, Sunday = 0)

  // January, february, and december are out.
  if (month < 3 || month > 10)
    return false;

  // April to October are in
  if (month > 3 && month < 10)
    return true;

  int previousSunday = day - dow;

  // In march, we are DST if our previous sunday was on or after the 25th.
  if (month == 3)
    return previousSunday >= 25;

  // In november we must be before the first sunday to be dst.
  // That means the previous sunday must be before the 1st.
  return previousSunday <= 0;
}

// Send a line of debug info to MQTT
void RelayDebug(const char *format, ...)
{
  char buffer[128];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buffer, 128, format, ap);
  va_end(ap);
  client.publish(relay_topic, buffer);
#ifdef DEBUG
  Serial.printf("%s\n", buffer);
#endif
}

// Send a line of debug info to MQTT
void Debug(const char *format, ...)
{
  char buffer[128];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buffer, 128, format, ap);
  va_end(ap);
  client.publish(reply_topic, buffer);
#ifdef DEBUG
  Serial.printf("%s\n", buffer);
#endif
}
