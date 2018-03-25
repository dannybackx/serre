/*
 * Power switch, controlled by MQTT
 *
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
#ifdef MY_SSID_1
  { MY_SSID_1, MY_WIFI_PASSWORD_1 },
#endif
#ifdef MY_SSID_2
  { MY_SSID_2, MY_WIFI_PASSWORD_2 },
#endif
#ifdef MY_SSID_3
  { MY_SSID_3, MY_WIFI_PASSWORD_3 },
#endif
#ifdef MY_SSID_4
  { MY_SSID_4, MY_WIFI_PASSWORD_4 },
#endif
  { NULL, NULL}
};

// MQTT
WiFiClient	espClient;
PubSubClient	client(espClient);

const char* mqtt_server = MQTT_HOST;
const int mqtt_port = MQTT_PORT;

#define	SWITCH_TOPIC	"/switch"
#define OTA_ID		"OTA-Switch"

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

// Schedule
struct item {
  short hour, minute, state;
};

int nitems = 0;
item *items;

#define	VERBOSE_OTA	0x01
#define	VERBOSE_VALVE	0x02
#define	VERBOSE_BMP	0x04
#define	VERBOSE_4	0x08

// Forward declarations
void callback(char * topic, byte *payload, unsigned int length);
void reconnect(void);
time_t mySntpInit();

// Arduino setup function
void setup() {
  // Try to connect to WiFi
  WiFi.mode(WIFI_STA);

  int wcr = WL_IDLE_STATUS;
  for (int ix = 0; wcr != WL_CONNECTED && mywifi[ix].ssid != NULL; ix++) {
    int wifi_tries = 3;
    while (wifi_tries-- >= 0) {
      // Serial.printf("\nTrying (%d %d) %s %s .. ", ix, wifi_tries, mywifi[ix].ssid, mywifi[ix].pass);
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

  // Set up real time clock
  // Note : DST processing comes later
  (void)sntp_set_timezone(MY_TIMEZONE);
  sntp_init();
  sntp_setservername(0, (char *)"ntp.scarlet.be");
  sntp_setservername(1, (char *)"ntp.belnet.be");

  ArduinoOTA.onStart([]() {
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(reply_topic, "OTA start");
    }
  });

  ArduinoOTA.onEnd([]() {
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
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
      client.publish(reply_topic, "OTA Error");
    }
  });

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(OTA_ID);
  WiFi.hostname(OTA_ID);

  ArduinoOTA.begin();

  mySntpInit();

  // MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Note don't use MQTT here yet, we're probably not connected yet

  // Pin to the (Solid state) Relay must be output, and 0
  pinMode(SSR_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  PinOff();

  // Default schedule
  // SetSchedule("16:03,1,16:36,0,16:58,1,17:42,0,21:00,1,22:35,0,06:58,1,07:25,0");	// Winter
  // SetSchedule("21:00,1,22:35,0,06:58,1,07:25,0");	// Late winter
  SetSchedule("22:00,1,22:45,0");	// Summer
}

void PinOn() {
  state = 1;
  manual = 1;
  digitalWrite(SSR_PIN, 1);
  digitalWrite(LED_PIN, 0);
}

void PinOff() {
  state = 0;
  manual = 1;
  digitalWrite(SSR_PIN, 0);
  digitalWrite(LED_PIN, 1);
}

int GetState() {
  return state;
}

int	counter = 0;
int	oldminute, newminute, oldhour, newhour;
time_t	the_time;

void loop() {
  ArduinoOTA.handle();

  // MQTT
  if (!client.connected()) {
    reconnect();
  }
  if (!client.loop())
    reconnect();

  delay(100);

  counter++;

  // Only do this once per second or so (given the delay 100)
  if (counter % 10 == 0) {
    counter = 0;

    oldminute = newminute;
    oldhour = newhour;

    the_time = sntp_get_current_timestamp();

    newhour = hour(the_time);
    newminute = minute(the_time);

    if (newminute == oldminute && newhour == oldhour) {
      return;
    }

    int tn = newhour * 60 + newminute;
    for (int i=1; i<nitems; i++) {
      int t1 = items[i-1].hour * 60 + items[i-1].minute;
      int t2 = items[i].hour * 60 + items[i].minute;

#if 1
      // On the exact hour, turn off manual mode
      if (t1 == tn && manual == 1) {
        manual = 0;

	if (items[i-1].state == 1 && state == 0) {
	  PinOn();
	  RelayDebug("%02d:%02d : %s", newhour, newminute, "on");
	} else if (items[i-1].state == 0 && state == 1) {
	  PinOff();
	  RelayDebug("%02d:%02d : %s", newhour, newminute, "off");
	}
      } else if (t1 <= tn && tn < t2 && manual == 0) {
        // Between the hours, only change state if not manual
	if (items[i-1].state == 1 && state == 0) {
	  PinOn();
	  RelayDebug("%02d:%02d : %s", newhour, newminute, "on");
	} else if (items[i-1].state == 0 && state == 1) {
	  PinOff();
	  RelayDebug("%02d:%02d : %s", newhour, newminute, "off");
	}
      }
#else
      if (t1 <= tn && tn < t2) {
	if (items[i-1].state == 1 && state == 0) {
	  PinOn();
	  RelayDebug("%02d:%02d : %s", newhour, newminute, "on");
	} else if (items[i-1].state == 0 && state == 1) {
	  PinOff();
	  RelayDebug("%02d:%02d : %s", newhour, newminute, "off");
	}
      }
#endif
    }

    // On the beginning of the hour, check DST
    if (newminute == 0) {
      int the_day = day(the_time);
      int the_month = month(the_time);
      int the_dow = dayOfWeek(the_time);

      bool newdst = IsDST2(the_day, the_month, the_dow, newhour);
      if (_isdst != newdst) {
        mySntpInit();
	_isdst = newdst;
      }
    }
  }
}

/*
 * Respond to MQTT queries.
 * Only two types are subscribed to :
 *   topic == "/switch"			payload contains subcommand
 *   topic == "/switch/program"		payload contains new schedule
 */
void callback(char *topic, byte *payload, unsigned int length) {
  char reply[80];

  char pl[80];
  strncpy(pl, (const char *)payload, length);
  pl[length] = 0;

  // RelayDebug("Callback topic {%s} payload {%s}", topic, pl);

  if (strcmp(topic, SWITCH_TOPIC) == 0) {
           if (strcmp(pl, "on") == 0) {	// Turn the relay on
      PinOn();
    } else if (strcmp(pl, "off") == 0) {	// Turn the relay off
      PinOff();
    } else if (strcmp(pl, "state") == 0) {	// Query on/off state
      RelayDebug("Switch %s", GetState() ? "on" : "off");
    } else if (strcmp(pl, "query") == 0) {	// Query schedule
      client.publish(SWITCH_TOPIC "/schedule", GetSchedule());
    } else if (strcmp(pl, "network") == 0) {	// Query network parameters
      Debug("SSID {%s}, IP %s, GW %s", WiFi.SSID().c_str(), ips.c_str(), gws.c_str());
    } else if (strcmp(pl, "time") == 0) {	// Query the device's current time
      time_t t = sntp_get_current_timestamp();
      Debug("%04d-%02d-%02d %02d:%02d:%02d, tz %d",
        year(t), month(t), day(t), hour(t), minute(t), second(t), sntp_get_timezone());
    } else if (strcmp(pl, "reinit") == 0) {
      mySntpInit();
    }
    // else silently ignore
  } else if (strcmp(topic, SWITCH_TOPIC "/program") == 0) {
    // Set the schedule according to the string provided
    SetSchedule((char *)pl);
    client.publish(SWITCH_TOPIC "/schedule", "Ok");
  }
  // Else silently ignore
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
	Debug("boot %04d-%02d-%02d %02d:%02d:%02d, timezone is set to %d",
	  year(t), month(t), day(t), hour(t), minute(t), second(t), sntp_get_timezone());

	mqtt_initial = 0;
      } else {
        Debug("reconnect %04d-%02d-%02d %02d:%02d:%02d",
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

bool IsDST2(int day, int month, int dow, int hr)
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
  if (month == 3) {
    if (previousSunday < 25)
      return false;
    if (dow > 0)
      return true;
    if (hr < 3)
      return false;
    return true;
    // return previousSunday >= 25;
  }

  // In november we must be before the first sunday to be dst.
  // That means the previous sunday must be before the 1st.
  return previousSunday <= 0;
}

/*
 * Set the schedule based on a formatted string.
 *
 * Example format : SetSchedule("21:00,1,22:35,0,06:45,1,07:35,0");
 */
void SetSchedule(const char *desc) {
  const char *p;
  int count;

  // Count commas
  for (p=desc,count=0; *p; p++)
    if (*p == ',')
      count++;

  // Clean up old allocation, make a new one
  if (nitems != 0) {
    free(items);
    items = NULL;
  }

  nitems = (count + 1) / 2;	// Number of timestamps provided is half the commas
  nitems += 2;			// For beginning and end of list

  items = (item *)malloc(sizeof(item) * nitems);

  // Convert CSV string into items
  p = desc;
  for (int i=1; i<nitems-1; i++) {
    int hr, min, val, nchars;
    sscanf(p, "%d:%d,%d%n", &hr, &min, &val, &nchars);

    p += nchars;
    if (*p == ',')
      p++;

    items[i].hour = hr;
    items[i].minute = min;
    items[i].state = val;
  }

  // Now add a first and last entry
  items[0].hour = 0;
  items[0].minute = 0;
  items[0].state = items[nitems-2].state;
  items[nitems-1].hour = 24;
  items[nitems-1].minute = 0;
  items[nitems-1].state = items[nitems-2].state;

  // for (int i=0; i<nitems; i++) Debug("SetSchedule %d (%d:%d) %d", i, items[i].hour, items[i].minute, items[i].state);

  // Sort the damn thing
  for (int i=1; i<nitems; i++) {
    int m=i;
    int tm = items[i].hour * 60 + items[i].minute;
    // Find lowest value
    for (int j=i+1; j<nitems; j++) {
      int tj = items[j].hour * 60 + items[j].minute;
      if (tj < tm) {
        m = j;
	tm = tj;
      }
    }
    // Swap if necessary
    if (i != m) {
      struct item xx = items[i];
      items[i] = items[m];
      items[m] = xx;
    }
  }

  // for (int i=0; i<nitems; i++) Debug("Sorted schedule %d (%d:%d) %d", i, items[i].hour, items[i].minute, items[i].state);

}

char *GetSchedule() {
  int len = 0,
      max = 24; // initial size, good for 2 entries, increases if necessary
  char *r = (char *)malloc(max), s[10];

  r[0] = '\0';
  for (int i=0; i<nitems; i++) {
    if (i != 0)
      r[len++] = ',';
    r[len] = '\0';
    sprintf(s, "%02d:%02d,%d", items[i].hour, items[i].minute, items[i].state);
    int ms = strlen(s);

    // increase allocation size ?
    if (len + ms > max) {
      max += 10;
      // Note doing this the hard way, realloc() gave strange results
      char *r2 = (char *)malloc(max);
      strcpy(r2, r);
      free(r);
      r = r2;
    }
    strcat(r, s);
    len = strlen(r);
  }
  return r;
}

time_t mySntpInit() {
  time_t t;

  // Wait for a correct time, and report it

  t = sntp_get_current_timestamp();
  while (t < 0x1000) {
    delay(1000);
    t = sntp_get_current_timestamp();
  }

  // DST handling
  if (IsDST(day(t), month(t), dayOfWeek(t))) {
    _isdst = 1;

    // Set TZ again
    sntp_stop();
    (void)sntp_set_timezone(MY_TIMEZONE + 1);

    // Debug("mySntpInit(day %d, month %d, dow %d) : DST = %d, timezone %d", day(t), month(t), dayOfWeek(t), _isdst, MY_TIMEZONE + 1);

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
    // Debug("mySntpInit(day %d, month %d, dow %d) : DST = %d", day(t), month(t), dayOfWeek(t), _isdst);
  }

  return t;
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
}
