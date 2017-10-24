/*
 * Power switch, controlled by MQTT
 *
 * Copyright (c) 2017 by Danny Backx
 */

#define	PRODUCTION
#define	USE_DST
#define	SNTP_API

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <TimeLib.h>

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

#ifdef PRODUCTION
#define	SWITCH_TOPIC	"/switch"
#define OTA_ID		"OTA-Switch"
#else
#define	SWITCH_TOPIC	"/testswitch"
#define OTA_ID		"OTA-TestSwitch"
#endif

const char *mqtt_topic = SWITCH_TOPIC;
const char *reply_topic = SWITCH_TOPIC "/reply";
const char *relay_topic = SWITCH_TOPIC "/relay";

int mqtt_initial = 1;
int _isdst = 0;

// Forward
bool IsDST(int day, int month, int dow);
char *GetSchedule();
void SetSchedule(const char *desc);
void PinOff();

// All kinds of variables
#ifdef DO_BLINK
int		count = 0;
#endif
int		verbose = 0;
String		ips, gws;
int		state = 0;

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

struct tm *tsnow;

// Forward declarations
void callback(char * topic, byte *payload, unsigned int length);
void reconnect(void);
time_t mySntpInit();

// Arduino setup function
void setup() {
  char buffer[80];

#ifdef USE_SERIAL
  Serial.begin(9600);
  delay(3000);    // Allow you to plug in the console

  Serial.println("\n\nPower switch\n(c) 2017 by Danny Backx\n");
  Serial.printf("Boot version %d, flash chip size %d, SDK version %s\n",
    ESP.getBootVersion(), ESP.getFlashChipSize(), ESP.getSdkVersion());

  Serial.printf("Free sketch space %d, application build %s %s\n",
    ESP.getFreeSketchSpace(), _BuildInfo.date, _BuildInfo.time);

  Serial.print("Starting WiFi ");
#endif

  // Try to connect to WiFi
  WiFi.mode(WIFI_STA);

  int wcr = WL_IDLE_STATUS;
  for (int ix = 0; wcr != WL_CONNECTED && mywifi[ix].ssid != NULL; ix++) {
    int wifi_tries = 3;
    while (wifi_tries-- >= 0) {
      // Serial.printf("\nTrying (%d %d) %s %s .. ", ix, wifi_tries, mywifi[ix].ssid, mywifi[ix].pass);
      WiFi.begin(mywifi[ix].ssid, mywifi[ix].pass);
#ifdef USE_SERIAL
      Serial.print(".");
#endif
      wcr = WiFi.waitForConnectResult();
      if (wcr == WL_CONNECTED)
        break;
    }
  }

  // Reboot if we didn't manage to connect to WiFi
  if (wcr != WL_CONNECTED) {
#ifdef USE_SERIAL
    Serial.printf("WiFi Failed\n");
#endif
    delay(2000);
    ESP.restart();
  }

  IPAddress ip = WiFi.localIP();
  ips = ip.toString();
  IPAddress gw = WiFi.gatewayIP();
  gws = gw.toString();
#ifdef USE_SERIAL
  Serial.print(" MAC "); Serial.print(WiFi.macAddress());
  Serial.printf(", SSID {%s}, IP %s, GW %s\n", WiFi.SSID().c_str(), ips.c_str(), gws.c_str());

  Serial.println("Initialize SNTP ...");
#endif

  // Set up real time clock
#ifdef SNTP_APIS
  // Note : DST processing comes later
  (void)sntp_set_timezone(MY_TIMEZONE);
  sntp_init();
  sntp_setservername(0, (char *)"ntp.scarlet.be");
  sntp_setservername(1, (char *)"ntp.belnet.be");
#else
  // configTime(MY_TIMEZONE * 3600, 3600, (char *)"ntp.scarlet.be", (char *)"ntp.belnet.be");
  configTime(7200, 3600, (char *)"ntp.scarlet.be", (char *)"ntp.belnet.be");
#endif

#ifdef USE_SERIAL
  Serial.printf("Starting OTA listener ...\n");
#endif
  ArduinoOTA.onStart([]() {
#ifdef USE_SERIAL
    Serial.print("OTA Start : ");
#endif

    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(reply_topic, "OTA start");
    }
  });
  ArduinoOTA.onEnd([]() {
#ifdef USE_SERIAL
    Serial.println("\nOTA Complete");
#endif
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(reply_topic, "OTA complete");
    }
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int curr;
    curr = (progress / (total / 50));
#ifdef USE_SERIAL
    if (OTAprev < curr)
      Serial.print('#');
#endif
    OTAprev = curr;
  });
  ArduinoOTA.onError([](ota_error_t error) {
#ifdef USE_SERIAL
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
#endif

    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(reply_topic, "OTA Error");
    }
  });

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(OTA_ID);
  WiFi.hostname(OTA_ID);
  // ArduinoOTA.setPassword((const char *)"123");

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
  SetSchedule("21:00,1,22:35,0,06:45,1,07:35,0");
  // SetSchedule("06:45,1,07:35,0,21:00,1,22:35,0");

#ifdef USE_SERIAL
  Serial.printf("Ready!\n");
#endif
}

void PinOn() {
  state = 1;
  digitalWrite(SSR_PIN, 1);
  digitalWrite(LED_PIN, 0);
}

void PinOff() {
  state = 0;
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
  char buffer[80];

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

#ifdef SNTP_API
    the_time = sntp_get_current_timestamp();
#else
    the_time = time(NULL);
#endif
    newhour = hour(the_time);
    newminute = minute(the_time);

    if (newminute == oldminute && newhour == oldhour) {
      return;
    }

    int tn = newhour * 60 + newminute;
    for (int i=1; i<nitems; i++) {
      int t1 = items[i-1].hour * 60 + items[i-1].minute;
      int t2 = items[i].hour * 60 + items[i].minute;

      // sprintf(buffer, "%02d:%02d : i %d tn %d t1 %d t2 %d state[i] %d state %d",
      //   newhour, newminute, i, tn, t1, t2, items[i].state, state);
      // client.publish(relay_topic, buffer);

      if (t1 <= tn && tn < t2) {
	if (items[i-1].state == 1 && state == 0) {
          // sprintf(buffer, "%02d:%02d : i %d tn %d t1 %d t2 %d state[i] %d state %d",
          //   newhour, newminute, i, tn, t1, t2, items[i].state, state);
          // client.publish(relay_topic, buffer);

	  PinOn();
	  sprintf(buffer, "%02d:%02d : %s", newhour, newminute, "on");
	  client.publish(relay_topic, buffer);
	} else if (items[i-1].state == 0 && state == 1) {
          // sprintf(buffer, "%02d:%02d : i %d tn %d t1 %d t2 %d state[i] %d state %d",
          //   newhour, newminute, i, tn, t1, t2, items[i].state, state);
          // client.publish(relay_topic, buffer);

	  PinOff();
	  sprintf(buffer, "%02d:%02d : %s", newhour, newminute, "off");
	  client.publish(relay_topic, buffer);
	}
      }
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length) {
  char *pl = (char *)payload;
  char reply[80];

#ifdef USE_SERIAL
  Serial.print("Message arrived, topic [");
  Serial.print(topic);
  Serial.print("], payload {");
  for (int i=0; i<length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("}");
#endif

         if (strcmp(topic, SWITCH_TOPIC "/on") == 0) {	// Turn the relay on
#ifdef USE_SERIAL
    Serial.println("SSR on");
#endif
    PinOn();
  } else if (strcmp(topic, SWITCH_TOPIC "/off") == 0) {	// Turn the relay off
#ifdef USE_SERIAL
    Serial.println("SSR off");
#endif
    PinOff();
  } else if (strcmp(topic, SWITCH_TOPIC "/state") == 0) {	// Query on/off state
    sprintf(reply, "Switch %s", GetState() ? "on" : "off");
    client.publish(relay_topic, reply);
  } else if (strcmp(topic, SWITCH_TOPIC "/query") == 0) {	// Query schedule
    client.publish(SWITCH_TOPIC "/schedule", GetSchedule());
  } else if (strcmp(topic, SWITCH_TOPIC "/network") == 0) {	// Query network parameters
    sprintf(reply, "SSID {%s}, IP %s, GW %s", WiFi.SSID().c_str(), ips.c_str(), gws.c_str());
    client.publish(reply_topic, reply);
  } else if (strcmp(topic, SWITCH_TOPIC "/program") == 0) {	// Set the schedule according to the string provided
    SetSchedule((char *)payload);
    client.publish(SWITCH_TOPIC "/schedule", "Ok");
  } else if (strcmp(topic, SWITCH_TOPIC "/time") == 0) {	// Query the device's current time
    struct tm *tmp = localtime(&the_time);
    strftime(reply, sizeof(reply), "%F %T", tmp);
    char *p = reply + strlen(reply);
    int tz = sntp_get_timezone();
    sprintf(p, ", tz %d", tz);
    client.publish(reply_topic, reply);
  } else {
    // Silently ignore, this includes our own replies
  }

}

void reconnect(void) {
  char buffer[80];

  // Loop until we're reconnected
  while (!client.connected()) {
#ifdef USE_SERIAL
    Serial.print("Attempting MQTT connection...");
#endif
    // Attempt to connect
    if (client.connect(MQTT_CLIENT)) {
#ifdef USE_SERIAL
      Serial.println("connected");
#endif
      // Get the time
      time_t t = mySntpInit();

      tsnow = localtime(&t);

      // Once connected, publish an announcement...
      if (mqtt_initial) {
	strftime(buffer, sizeof(buffer), "boot %F %T", tsnow);
	char *p = buffer + strlen(buffer);
	int tz = sntp_get_timezone();
	sprintf(p, ", timezone is set to %d", tz);

        client.publish(reply_topic, buffer);
	mqtt_initial = 0;
      } else {
	strftime(buffer, sizeof(buffer), "reconnect %F %T", tsnow);
        client.publish(reply_topic, buffer);
      }

      // ... and (re)subscribe
      client.subscribe(SWITCH_TOPIC "/#");
    } else {
#ifdef USE_SERIAL
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
#endif
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

extern "C" {
void Debug(char *s) {
#ifdef USE_SERIAL
  Serial.print(s);
#endif
}

void DebugInt(int i) {
#ifdef USE_SERIAL
  Serial.print(i);
#endif
}

char *getenv(const char *name) {
  return 0;
}
}


bool IsDST(int day, int month, int dow)
{
  // January, february, and december are out.
  if (month < 3 || month > 11)
    return false;

  // April to October are in
  if (month > 3 && month < 11)
    return true;

  int previousSunday = day - dow;

  // In march, we are DST if our previous sunday was on or after the 8th.
  if (month == 3)
    return previousSunday >= 8;

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

  // Debug("setSchedule("); Debug(desc); Debug(")\n");

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

  // DebugInt(count); Debug(" -> "); DebugInt(nitems); Debug(" items\n");

  // Convert CSV string into items
  p = desc;
  for (int i=1; i<nitems-1; i++) {
    int hr, min, val, nchars;
    // Debug("sscanf("); Debug(p); Debug(")\n");
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
#ifdef USE_SERIAL
  Serial.printf("Time ");
#endif
#ifdef SNTP_API
  t = sntp_get_current_timestamp();
  while (t < 0x1000) {
#ifdef USE_SERIAL
    Serial.printf(".");
#endif
    delay(1000);
    t = sntp_get_current_timestamp();
  }
#else
  t = time(NULL);
#endif

#ifdef SNTP_API
#ifdef USE_DST
  // DST handling
  if (IsDST(day(t), month(t), dayOfWeek(t))) {
    _isdst = 1;

    // Set TZ again
    sntp_stop();
    (void)sntp_set_timezone(MY_TIMEZONE + 1);

    // Re-initialize/fetch
    sntp_init();
    t = sntp_get_current_timestamp();
    while (t < 0x1000) {
#ifdef USE_SERIAL
      Serial.printf(".");
#endif
      delay(1000);
      t = sntp_get_current_timestamp();
    }
  }
#endif
#else
#warning todo
#endif
  return t;
}
