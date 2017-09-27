/*
 * Power switch, controlled by MQTT
 *
 * Copyright (c) 2017 by Danny Backx
 */
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

const char *mqtt_topic = "/switch";
const char *reply_topic = "/switch/reply";
int mqtt_initial = 1;

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
char	buffer[64];

// Forward declarations
void callback(char * topic, byte *payload, unsigned int length);
void reconnect(void);

// Arduino setup function
void setup() {
  Serial.begin(9600);
  delay(3000);    // Allow you to plug in the console

  Serial.println("\n\nPower switch\n(c) 2017 by Danny Backx\n");
  Serial.printf("Boot version %d, flash chip size %d, SDK version %s\n",
    ESP.getBootVersion(), ESP.getFlashChipSize(), ESP.getSdkVersion());

  Serial.printf("Free sketch space %d, application build %s %s\n",
    ESP.getFreeSketchSpace(), _BuildInfo.date, _BuildInfo.time);

  // Try to connect to WiFi
  Serial.print("Starting WiFi ");
  WiFi.mode(WIFI_STA);

  int wcr = WL_IDLE_STATUS;
  for (int ix = 0; wcr != WL_CONNECTED && mywifi[ix].ssid != NULL; ix++) {
    int wifi_tries = 3;
    while (wifi_tries-- >= 0) {
      // Serial.printf("\nTrying (%d %d) %s %s .. ", ix, wifi_tries, mywifi[ix].ssid, mywifi[ix].pass);
      WiFi.begin(mywifi[ix].ssid, mywifi[ix].pass);
      Serial.print(".");
      wcr = WiFi.waitForConnectResult();
      if (wcr == WL_CONNECTED)
        break;
    }
  }

  // Reboot if we didn't manage to connect to WiFi
  if (wcr != WL_CONNECTED) {
    Serial.printf("WiFi Failed\n");
    delay(2000);
    ESP.restart();
  }

  IPAddress ip = WiFi.localIP();
  ips = ip.toString();
  IPAddress gw = WiFi.gatewayIP();
  gws = gw.toString();
  Serial.print(" MAC "); Serial.print(WiFi.macAddress());
  Serial.printf(", SSID {%s}, IP %s, GW %s\n", WiFi.SSID().c_str(), ips.c_str(), gws.c_str());

  Serial.println("Initialize SNTP ...");
  // Set up real time clock
  sntp_init();
  sntp_setservername(0, (char *)"ntp.scarlet.be");
  sntp_setservername(1, (char *)"ntp.belnet.be");

  // This is a bad idea : fixed time zone, .. but it works for now.
  // Note : DST processing comes later
  (void)sntp_set_timezone(MY_TIMEZONE);

  Serial.printf("Starting OTA listener ...\n");
  ArduinoOTA.onStart([]() {
    Serial.print("OTA Start : ");

    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(mqtt_topic, "OTA start");
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Complete");
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(mqtt_topic, "OTA complete");
    }
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int curr;
    curr = (progress / (total / 50));
    if (OTAprev < curr)
      Serial.print('#');
    OTAprev = curr;
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");

    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(mqtt_topic, "OTA Error");
    }
  });

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(OTA_ID);
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.begin();

  time_t t;
  // Wait for a correct time, and report it
  Serial.printf("Time ");
  t = sntp_get_current_timestamp();
  while (t < 0x1000) {
    Serial.printf(".");
    delay(1000);
    t = sntp_get_current_timestamp();
  }

  // DST handling
  if (IsDST(day(t), month(t), dayOfWeek(t))) {
    // Set TZ again
    sntp_stop();
    (void)sntp_set_timezone(MY_TIMEZONE + 1);

    // Re-initialize/fetch
    sntp_init();
    t = sntp_get_current_timestamp();
    while (t < 0x1000) {
      Serial.printf(".");
      delay(1000);
      t = sntp_get_current_timestamp();
    }
  }

  tsnow = localtime(&t);
  strftime(buffer, sizeof(buffer), " %F %T", tsnow);
  Serial.println(buffer);

  // MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Pin to the (Solid state) Relay must be output, and 0
  pinMode(SSR_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  PinOff();

  // Default schedule
  SetSchedule("21:00,1,22:35,0,06:45,1,07:35,0");

  Serial.printf("Ready!\n");
}

void PinOn() {
  state = 1;
  digitalWrite(SSR_PIN, 1);
  digitalWrite(LED_PIN, 1);
}

void PinOff() {
  state = 0;
  digitalWrite(SSR_PIN, 0);
  digitalWrite(LED_PIN, 0);
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

    // Check if this is a timestamp with processing
    for (int i=0; i<nitems; i++) {
      if (items[i].hour == newhour && items[i].minute == newminute) {
	if (items[i].state == 1)
	  PinOn();
	else
	  PinOff();
        return;
      }
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length) {
  char *pl = (char *)payload;
  char reply[80];

  Serial.print("Message arrived, topic [");
  Serial.print(topic);
  Serial.print("], payload {");
  for (int i=0; i<length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("}");

         if (strcmp(topic, "/switch/on") == 0) {	// Turn the relay on
    Serial.println("SSR on");
    PinOn();
  } else if (strcmp(topic, "/switch/off") == 0) {	// Turn the relay off
    Serial.println("SSR off");
    PinOff();
  } else if (strcmp(topic, "/switch/state") == 0) {	// Query on/off state
    sprintf(reply, "Switch %s", GetState() ? "on" : "off");
    client.publish("/switch/relay", reply);
  } else if (strcmp(topic, "/switch/query") == 0) {	// Query schedule
    client.publish("/switch/schedule", GetSchedule());
  } else if (strcmp(topic, "/switch/network") == 0) {	// Query network parameters
    sprintf(reply, "SSID {%s}, IP %s, GW %s", WiFi.SSID().c_str(), ips.c_str(), gws.c_str());
    client.publish(reply_topic, reply);
  } else if (strcmp(topic, "/switch/program") == 0) {	// Set the schedule according to the string provided
    SetSchedule((char *)payload);
    client.publish("/switch/schedule", "Ok");
  } else if (strcmp(topic, "/switch/time") == 0) {	// Query the device's current time
    struct tm *tmp = localtime(&the_time);
    strftime(reply, sizeof(reply), "%F %T", tmp);
    client.publish(reply_topic, reply);
  } else {
    // Silently ignore, this includes our own replies
  }

}

void reconnect(void) {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_CLIENT)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      if (mqtt_initial) {
	strftime(buffer, sizeof(buffer), "boot %F %T", tsnow);
        client.publish(mqtt_topic, buffer);
	mqtt_initial = 0;
      } else {
	strftime(buffer, sizeof(buffer), "reconnect %F %T", tsnow);
        client.publish(mqtt_topic, buffer);
      }

      // ... and (re)subscribe
      client.subscribe("/switch/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

extern "C" {
void Debug(char *s) {
  Serial.print(s);
}

void DebugInt(int i) {
  Serial.print(i);
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

  nitems = (count + 1) / 2;
  items = (item *)malloc(sizeof(item) * nitems);

  // DebugInt(count); Debug(" -> "); DebugInt(nitems); Debug(" items\n");

  // Convert CSV string into items
  p = desc;
  for (int i=0; i<nitems; i++) {
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

  // PrintSchedule();
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
