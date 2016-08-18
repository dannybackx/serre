// See http://www.bc-robotics.com/tutorials/controlling-a-solenoid-valve-with-arduino/

#define ENABLE_OTA
#define ENABLE_SNTP
#define	ENABLE_I2C

#include <ESP8266WiFi.h>
#ifdef ENABLE_I2C
#include <Wire.h>       // Required to compile UPnP
#endif
#include "PubSubClient.h"
#include "SFE_BMP180.h"

extern "C" {
#ifdef ENABLE_SNTP
#include <sntp.h>
#endif
#include <time.h>
}

// Prepare for OTA software installation
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
static int OTAprev;

#include "mywifi.h"
const char* ssid     = MY_SSID;
const char* password = MY_WIFI_PASSWORD;

// MQTT
WiFiClient	espClient;
PubSubClient	client(espClient);

const char* mqtt_server = "192.168.1.176";
const int mqtt_port = 1883;

char *mqtt_topic_serre = "Serre";
char *mqtt_topic_bmp180 = "Serre/BMP180";
char *mqtt_topic_valve = "Serre/Valve";
int mqtt_initial = 1;

// Pin definitions
int	solenoidPin = 0;
//
// Test results from testwemosd1/testwemosd1 :
// Note this is with an old model Wemos D1 (http://www.wemos.cc/Products/d1.html).
//	value		pin
//	16		D2
//	15		D10 / SS
//	14		D13 / SCK / D5
//	13		D11 / MOSI / D7
//	12		D6 / MISO / D12
//	11		N/A
//	10
//	9
//	8		crashes ?
//	7
//	6
//	5		D15 / SCL / D3
//	4		D14 / SDA / D4
//	3		D0 / RX
//	2		TX1 / D9
//	1		TX breaks serial connection
//	0		D8
//

#define	VERBOSE_OTA		0x01
#define	VERBOSE_VALVE		0x02
#define	VERBOSE_BMP		0x04
#define	VERBOSE_CALLBACK	0x08
#define	VERBOSE_5		0x10
#define	VERBOSE_6		0x20
#define	VERBOSE_7		0x40
#define	VERBOSE_8		0x80

// All kinds of variables
int		pin14;
int		count = 0;
SFE_BMP180	*bmp = 0;
int		verbose = 0;
// char		temperature[BMP180_STATE_LENGTH], pressure[BMP180_STATE_LENGTH];
double		newPressure, newTemperature, oldPressure, oldTemperature;
int		percentage;		// How much of a difference before notify
int		valve = 0;		// Closed = 0

#ifdef ENABLE_SNTP
struct tm *now;
//char	*the_time;
char	buffer[64];
#endif

// Forward declarations
void callback(char * topic, byte *payload, unsigned int length);
void reconnect(void);
void ValveReset(), ValveOpen();
void BMPInitialize(), BMPQuery();

// Arduino setup function
void setup() {
  Serial.begin(9600);
#ifdef ENABLE_I2C
  Wire.begin();		// Default SDA / SCL are xx / xx
#endif
//  Serial.setDebugOutput(true);
  
  delay(3000);    // Allow you to plug in the console
  
  Serial.println("\n\nMQTT Greenhouse Watering and Sensor Module\n");
  Serial.printf("Boot version %d, flash chip size %d, SDK version %s\n",
    ESP.getBootVersion(), ESP.getFlashChipSize(), ESP.getSdkVersion());

  Serial.printf("Free sketch space %d\n", ESP.getFreeSketchSpace());
  Serial.print("Starting WiFi ");
  WiFi.mode(WIFI_STA);

  // Try to connect to WiFi
  int wifi_tries = 3;
  int wcr;
  while (wifi_tries-- >= 0) {
    WiFi.begin(ssid, password);
    Serial.print(". ");
    wcr = WiFi.waitForConnectResult();
    if (wcr == WL_CONNECTED)
      break;
    Serial.printf(" %d ", wifi_tries + 1);
  }

  // Reboot if we didn't manage to connect to WiFi
  if (wcr != WL_CONNECTED) {
    Serial.printf("WiFi Failed\n");
    delay(2000);
    ESP.restart();
  }

  IPAddress ip = WiFi.localIP();
  String ips = ip.toString();
  IPAddress gw = WiFi.gatewayIP();
  String gws = gw.toString();
  Serial.print("MAC "); Serial.print(WiFi.macAddress());
  Serial.printf(", SSID {%s}, IP %s, GW %s\n", WiFi.SSID().c_str(), ips.c_str(), gws.c_str());

#ifdef ENABLE_SNTP
  Serial.println("Initialize SNTP ...");
  // Set up real time clock
  sntp_init();
  sntp_setservername(0, "ntp.scarlet.be");
  sntp_setservername(1, "ntp.belnet.be");
  // This is a bad idea : fixed time zone, no DST processing, .. but it works for now.
  // (void)sntp_set_timezone(+2);
  // (void)sntp_set_timezone(0);		// GMT, leaving this out default to somewhere in Asia
#endif

  // DEBUG
  Serial.print("Timezone ");
  Serial.println(getenv("TZ"));
  tzset();

#ifdef ENABLE_OTA
  Serial.printf("Starting OTA listener ...\n");
  ArduinoOTA.onStart([]() {
    ValveReset();
    Serial.print("OTA Start : ");

    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(mqtt_topic_serre, "OTA start");
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Complete");
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(mqtt_topic_serre, "OTA complete");
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
      client.publish(mqtt_topic_serre, "OTA Error");
    }
  });
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("OTA-Serre");
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.begin();
#endif

#ifdef ENABLE_SNTP
  time_t t;
  // Wait for a correct time, and report it
  Serial.printf("Time ");
  t = sntp_get_current_timestamp();
  while (t < 0x1000) {
    Serial.printf(".");
    delay(1000);
    t = sntp_get_current_timestamp();
  }
  now = localtime(&t);
  strftime(buffer, sizeof(buffer), " %F %T", now);
  Serial.println(buffer);
#endif

  // MQTT
  Serial.print("Starting MQTT ");
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  Serial.println("done");

#ifdef ENABLE_I2C
  // BMP180
  Serial.print("Initializing BMP180 ... ");
  BMPInitialize();
  if (bmp)
    Serial.println("ok");
  else
    Serial.println("failed");
#endif

  pinMode(solenoidPin, OUTPUT);

  Serial.println("Ready");
}

void loop() {
#ifdef ENABLE_OTA
    ArduinoOTA.handle();
#endif

  // MQTT
  if (!client.connected()) {
    reconnect();
  }
  if (!client.loop())
    reconnect();

  delay(100);

  count++;
  if (count > 10) {
    ValveReset();

    if (verbose & VERBOSE_VALVE) {
      if (count == 11) client.publish(mqtt_topic_valve, "0");
    }
  } else {
    ValveOpen();
    if (verbose & VERBOSE_VALVE) {
      if (count == 1) client.publish(mqtt_topic_valve, "1");
    }
  }
  if (count == 20)
    count = 0;
}

void ValveOpen() {
  valve = 1;
  digitalWrite(solenoidPin, 1);
}

void ValveReset() {
  valve = 0;
  digitalWrite(solenoidPin, 0);
}

void callback(char *topic, byte *payload, unsigned int length) {
  char *pl = (char *)payload;
  char reply[80];

  if (verbose & VERBOSE_CALLBACK) {
    Serial.print("Message arrived, topic [");
    Serial.print(topic);
    Serial.print("], payload {");
    for (int i=0; i<length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.printf(",%d}\n", length);
  }

  /*
  Serial.printf("Comparing strcmp(%s, %s) -> %d\n",
    topic, mqtt_topic_serre, strcmp(topic, mqtt_topic_serre));
  Serial.printf("Comparing strncmp(%s, %s, %d) -> %d\n",
    pl, mqtt_topic_bmp180, length, strncmp(pl, mqtt_topic_bmp180, length));
  /* */

  if (strcmp(topic, mqtt_topic_serre) == 0) {
    if (strncmp(pl, mqtt_topic_bmp180, length) == 0) {
      // This is a request to read temperature / humidity data
      Serial.println("Topic bmp");

      if (bmp) {
        BMPQuery();

        int a, b, c;
        // Temperature
        a = (int) newTemperature;
        double td = newTemperature - a;
        b = 100 * td;
        c = newPressure;

        // Format the result
        sprintf(reply, "bmp (%2d.%02d, %d)", a, b, c);
  
      } else {
        sprintf(reply, "No sensor detected");
      }

      if (verbose & VERBOSE_BMP)
        Serial.println(reply);
      client.publish(mqtt_topic_bmp180, reply);
    } else if (strcmp(pl, mqtt_topic_valve) == 0) {
      Serial.println("Topic valve");
      client.publish(mqtt_topic_valve, valve ? "Open" : "Closed");
    }

    // End topic == mqtt_topic_serre

  } else if (strcmp(topic, mqtt_topic_bmp180) == 0) {
    // Ignore, this is a message we've sent out ourselves
  } else if (strcmp(topic, mqtt_topic_valve) == 0) {
    // Ignore, this is a message we've sent out ourselves
  } else {
    Serial.println("Unknown topic");
  }
}

void reconnect(void) {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      if (mqtt_initial) {
#ifdef ENABLE_SNTP
	strftime(buffer, sizeof(buffer), "boot %F %T", now);
        client.publish(mqtt_topic_serre, buffer);
#else
        client.publish(mqtt_topic_serre, "boot");
#endif
	mqtt_initial = 0;
      } else {
#ifdef ENABLE_SNTP
	strftime(buffer, sizeof(buffer), "reconnect %F %T", now);
        client.publish(mqtt_topic_serre, buffer);
#else
        client.publish(mqtt_topic_serre, "reconnect");
#endif
      }

      // ... and (re)subscribe
      client.subscribe(mqtt_topic_serre);
      client.subscribe(mqtt_topic_valve);
      client.subscribe(mqtt_topic_bmp180);
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
}

void BMPInitialize() {
#ifdef ENABLE_I2C
  bmp = new SFE_BMP180();
  char ok = bmp->begin();	// 0 return value is a problem
  if (ok == 0) {
    delete bmp;
    bmp = 0;
  }
#endif
}

void BMPQuery() {
  if (bmp == 0)
    return;

#ifdef ENABLE_I2C
  // This is a multi-part query to the I2C device, see the SFE_BMP180 source files.
  char d1 = bmp->startTemperature();
  delay(d1);
  char d2 = bmp->getTemperature(newTemperature);
  if (d2 == 0) { // Error communicating with device
#ifdef DEBUG
    DEBUG.printf("BMP180 : communication error (temperature)\n");
#endif
    return;
  }

  char d3 = bmp->startPressure(0);
  delay(d3);
  char d4 = bmp->getPressure(newPressure, newTemperature);
  if (d4 == 0) { // Error communicating with device
#ifdef DEBUG
    DEBUG.printf("BMP180 : communication error (pressure)\n");
#endif
    return;
  }

  if (isnan(newTemperature) || isinf(newTemperature)) {
    newTemperature = oldTemperature;
    // nan = true;
  }
  if (isnan(newPressure) || isinf(newPressure)) {
    newPressure = oldPressure;
    // nan = true;
  }
  if (nan) {
#ifdef DEBUG
    DEBUG.println("BMP180 nan");
#endif
    return;
  }
#endif
}
