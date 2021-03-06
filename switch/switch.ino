#undef	DEBUG
/*
 * Power switch, controlled by MQTT
 *
 * Copyright (c) 2016, 2017, 2019, 2020, 2021 Danny Backx
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
#include <stdarg.h>

#include <coredecls.h>
#include <TZ.h>

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
#ifdef MY_SSID_5
  { MY_SSID_5, MY_WIFI_PASSWORD_5 },
#endif
  { NULL, NULL}
};

// MQTT
WiFiClient	espClient;
PubSubClient	mqtt(espClient);

const char* mqtt_server = MQTT_HOST;
const int mqtt_port = MQTT_PORT;

const char *mqtt_topic = SWITCH_TOPIC;
const char *reply_topic = SWITCH_TOPIC "/reply";
const char *relay_topic = SWITCH_TOPIC "/relay";

int mqtt_initial = 1;

// Forward
char *GetSchedule();
void SetSchedule(const char *desc);
void PinOff();
void time_is_set(bool from_sntp);

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

struct dates {
  short		month, day;
  const char	*schedule;
} date_table [] = {
  { 2, 10,	"21:00,1,22:35,0,06:58,1,07:25,0" },					// Late winter
  { 3, 1,	"21:00,1,22:35,0,06:48,1,07:05,0" },					
  { 3, 15,	"20:12,1,22:05,0,06:28,1,06:39,0" },					
  { 4, 1,	"21:00,1,22:35,0" },							// Spring
  { 6, 15,	"22:00,1,22:45,0" },							// Summer
  { 11, 1,	"16:03,1,16:36,0,16:58,1,17:42,0,21:00,1,22:35,0,06:58,1,07:25,0" },	// Winter
  { 12, 15,	"17:28,1,18:12,0,21:00,1,22:35,0,06:58,1,07:25,0" },
  { 12, 32,	NULL },									// Year end
  { 0, 0,	0 }
};

#define	VERBOSE_OTA	0x01
#define	VERBOSE_VALVE	0x02
#define	VERBOSE_BMP	0x04
#define	VERBOSE_4	0x08

// Forward declarations
void callback(char * topic, byte *payload, unsigned int length);
void reconnect(void);
void SetDefaultSchedule(time_t);

// Arduino setup function
void setup() {
  Serial.begin(115200);
  Serial.printf("Starting %s", MQTT_CLIENT);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);

#ifdef	USE_IP_FIXED
  // Set fixed IP ?
  {
    IPAddress	me((const uint8_t *)USE_IP_ME);
    IPAddress	gw((const uint8_t *)USE_IP_GW);
    IPAddress	mask((const uint8_t *)USE_IP_MASK);
    IPAddress	dns1((const uint8_t *)USE_IP_DNS1);
    IPAddress	dns2((const uint8_t *)USE_IP_DNS2);

    if (! WiFi.config(me, gw, mask, dns1, dns2)) {
      Serial.printf("Failed to set configured IP address\n");
    }
  }
#endif

  int wcr = WL_IDLE_STATUS;
  for (int ix = 0; wcr != WL_CONNECTED && mywifi[ix].ssid != NULL; ix++) {
    int wifi_tries = 3;
    while (wifi_tries-- >= 0) {
      Serial.printf("\nTrying (%d %d) %s .. ", ix, wifi_tries, mywifi[ix].ssid);
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
  Serial.printf("Connected\n");

  IPAddress ip = WiFi.localIP();
  ips = ip.toString();
  IPAddress gw = WiFi.gatewayIP();
  gws = gw.toString();

  // Set up real time clock
  settimeofday_cb(time_is_set);
  configTime(MY_TIMEZONE, MY_NTP_SERVER);

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int curr;
    curr = (progress / (total / 50));
    OTAprev = curr;
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error\n");
  });

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(OTA_ID);
  WiFi.hostname(OTA_ID);

  ArduinoOTA.begin();

  // MQTT
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(callback);

  // Note don't use MQTT here yet, we're probably not connected yet

  // Pin to the (Solid state) Relay must be output, and 0
  pinMode(SSR_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  PinOff();

  // Schedule will probably be set initially in time_is_set(), not here.
  time_t now = time(0);
  if (now > 1000L)
    SetDefaultSchedule(now);
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
tm	*tmp;

void loop() {
  ArduinoOTA.handle();

  // MQTT
  if (!mqtt.connected()) {
    reconnect();
  }
  if (!mqtt.loop())
    reconnect();

  delay(100);

  counter++;

  // Only do this once per second or so (given the delay 100)
  if (counter % 10 == 0) {
    counter = 0;

    oldminute = newminute;
    oldhour = newhour;

    the_time = time(0);
    tmp = localtime(&the_time);

    newhour = tmp->tm_hour;
    newminute = tmp->tm_min;

    if (newminute == oldminute && newhour == oldhour) {
      return;
    }

    int the_day = tmp->tm_mday;
    int the_month = tmp->tm_mon + 1;
    int the_year = tmp->tm_year + 1900;
    int the_dow = tmp->tm_wday;

    int tn = newhour * 60 + newminute;
    for (int i=1; i<nitems; i++) {
      int t1 = items[i-1].hour * 60 + items[i-1].minute;
      int t2 = items[i].hour * 60 + items[i].minute;

      // On the exact hour, turn off manual mode
      if (t1 == tn && manual == 1) {
        manual = 0;

	if (items[i-1].state == 1 && state == 0) {
	  PinOn();
	  RelayDebug("%04d.%02d.%02d %02d:%02d : %s", the_year, the_month, the_day, newhour, newminute, "on");
	} else if (items[i-1].state == 0 && state == 1) {
	  PinOff();
	  RelayDebug("%04d.%02d.%02d %02d:%02d : %s", the_year, the_month, the_day, newhour, newminute, "off");
	}
      } else if (t1 <= tn && tn < t2 && manual == 0) {
        // Between the hours, only change state if not manual
	if (items[i-1].state == 1 && state == 0) {
	  PinOn();
	  RelayDebug("%04d.%02d.%02d %02d:%02d : %s", the_year, the_month, the_day, newhour, newminute, "on");
	} else if (items[i-1].state == 0 && state == 1) {
	  PinOff();
	  RelayDebug("%04d.%02d.%02d %02d:%02d : %s", the_year, the_month, the_day, newhour, newminute, "off");
	}
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
#ifdef DEBUG
      Serial.printf("%s %s %s\n", SWITCH_TOPIC, "/schedule", GetSchedule());
#endif
      RelayDebug("schedule %s", GetSchedule());
    } else if (strcmp(pl, "network") == 0) {	// Query network parameters
      Debug("SSID {%s}, IP %s, GW %s", WiFi.SSID().c_str(), ips.c_str(), gws.c_str());
    } else if (strcmp(pl, "time") == 0) {	// Query the device's current time
      time_t t = time(0);
      tm *tmp = localtime(&t);
      RelayDebug("%04d.%02d.%02d %02d:%02d:%02d, tz %s",
        tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, MY_TIMEZONE);
    } else if (strcmp(pl, "reboot") == 0) {
      ESP.restart();
    }
    // else silently ignore
  } else if (strcmp(topic, SWITCH_TOPIC "/program") == 0) {
    // Set the schedule according to the string provided
    SetSchedule((char *)pl);
    mqtt.publish(SWITCH_TOPIC "/schedule", "Ok");
  }
  // Else silently ignore
}

void reconnect(void) {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    // Attempt to connect
    if (mqtt.connect(MQTT_CLIENT)) {
      // Get the time
      time_t t = time(0);

      // Once connected, publish an announcement...
      if (mqtt_initial && (t > 1000L)) {
	Debug("boot %04d.%02d.%02d %02d:%02d:%02d, timezone is set to %s",
          tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, MY_TIMEZONE);

	mqtt_initial = 0;
      } else {
        if (t > 1000L)
	  Debug("reconnect %04d.%02d.%02d %02d:%02d:%02d",
            tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
      }

      // ... and (re)subscribe
      mqtt.subscribe(SWITCH_TOPIC);
      mqtt.subscribe(SWITCH_TOPIC "/program");
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/*
 * Set the schedule based on a formatted string.
 *
 * Example format : SetSchedule("21:00,1,22:35,0,06:45,1,07:35,0");
 */
void SetSchedule(const char *desc) {
  const char *p;
  int count;

  RelayDebug("SetSchedule(%s)", desc);

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

// Send a line of debug info to MQTT
void RelayDebug(const char *format, ...)
{
  char buffer[128];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buffer, 128, format, ap);
  va_end(ap);
  mqtt.publish(relay_topic, buffer);
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
  mqtt.publish(reply_topic, buffer);
#ifdef DEBUG
  Serial.printf("%s\n", buffer);
#endif
}

void SetDefaultSchedule(time_t the_time) {
  int i;

  tm *tmp = localtime(&the_time);
  RelayDebug("SetDefaultSchedule %04d.%02d.%02d", tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday);

  for (i=0; date_table[i].schedule; i++) {
    // Exact date match ?
    if (date_table[i].month == tmp->tm_mon + 1 && date_table[i].day == tmp->tm_mday) {
      SetSchedule(date_table[i].schedule);
      return;
    }

    // Compare date ranges
    int x = date_table[i].month * 100 + date_table[i].day;
    int y = date_table[i+1].month * 100 + date_table[i+1].day;
    int z = (tmp->tm_mon + 1) * 100 + tmp->tm_mday;

    if (x <= z && z <= y) {
      // RelayDebug("SetDefaultSchedule -> %s", date_table[i].schedule);
      SetSchedule(date_table[i].schedule);
      return;
    }
  }
}

/*
 * Get notified when NTP time is updated.
 * Reinitialize our schedule when that happens.
 */
void time_is_set(bool from_sntp) {
  time_t now = time(0);
  tm *tmp = localtime(&now);
  reconnect();
  if (mqtt_initial) {
    Debug("boot %04d.%02d.%02d %02d:%02d:%02d, timezone is set to %s",
      tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, MY_TIMEZONE);

    mqtt_initial = 0;
  }
  SetDefaultSchedule(now);
}
