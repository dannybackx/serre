/*
 * I borrowed the idea and a couple of lines of code from David Birds work.
 * The rest is (c) Danny Backx 2018.
 *
 * Intention is to use stuff I have around :
 * - use a esp8266 (a Wemos D1 Mini)
 * - put in a small almost sealed box
 * - couple with an old Nokia 5V power supply, and a capacitor
 * - attach a BME280 sensor
 * and provide this functionality :
 * - feed Wunderground
 * - software update possible via OTA
 * - also MQTT support (logging to my server, remote query and reboot).
 */

/*
 * This software, the ideas and concepts is Copyright (c) David Bird 2018.
 * All rights to this software are reserved.
 * 
 * Any redistribution or reproduction of any part or all of the contents in any form
 * is prohibited other than the following:
 * 1. You may print or download to a local hard disk extracts for your personal and
 *    non-commercial use only.
 * 2. You may copy the content to individual third parties for their personal use,
 *    but only if you acknowledge the author David Bird as the source of the material.
 * 3. You may not, except with my express written permission, distribute or commercially
 *    exploit the content.
 * 4. You may not transmit it or store it in any other website or other form of
 *    electronic retrieval system for commercial purposes.
 *
 * The above copyright ('as annotated') notice and this permission notice shall be included
 * in all copies or substantial portions of the Software and where the software use is
 * visible to an end-user.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN
 * WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
 * OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * See more at http://www.dsbird.org.uk
 */
  
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>             // Built-in 
#include "secret.h"
#include <time.h>
#include <PubSubClient.h>
#include <TimeLib.h>

extern "C" {
  #include <sntp.h>
  #include <time.h>
}

// Prepare for OTA software installation
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

struct Wunderground {
  float		sensor_humidity,
		sensor_temperature,
		sensor_tempf,
		sensor_pressure,
		sensor_pressurem;
  time_t	time;
};

void SetupWifi();
void SetupOTA();
Wunderground *ReadSensorInformation();
boolean UploadDataToWU(Wunderground *);
time_t mySntpInit();
bool IsDST(int day, int month, int dow);
bool IsDST2(int day, int month, int dow, int hr);
void reconnect(void);
void callback(char * topic, byte *payload, unsigned int length);
void ReportToMqtt(Wunderground *);

#include <BMP280.h>
BMP280 *bmp280 = 0;

// ElektorLabs
#include <BME280.h>
BME280 *bme280 = 0;

/* MQTT */
const char *mqtt_server	= MQTT_HOST;
const int mqtt_port	= MQTT_PORT;
const char *mqtt_topic	= MQTT_TOPIC;
const char *reply_topic	= MQTT_TOPIC "/reply";
int mqtt_initial	= 1;		// Initial message ?
int _isdst = 0;

WiFiClient	espClient;
PubSubClient	client(espClient);

// Delay between updates, in milliseconds
// WU allows 500 requests per-day maximum, this sets the time to 30-mins
const unsigned long  UpdateInterval     = 30L*60L*1000L;

boolean ok;

// Is this device indoors or outside
const int indoor = 1;

void setup() {
  Serial.begin(115200);
  Serial.print("\nPersonal weather station\n(c) 2018 Danny Backx, based on work by David Bird (c) 2018\n");

  SetupWifi();
  Wire.begin();
  SetupOTA();

  // MQTT initialisation. Beware : needs a bit of time before it's active.
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Set up real time clock
  // Note : DST processing comes later
  (void)sntp_set_timezone(MY_TIMEZONE);
  sntp_init();
  sntp_setservername(0, (char *)"ntp.scarlet.be");
  sntp_setservername(1, (char *)"ntp.belnet.be");

  mySntpInit();

  // Detect sensor presence

  bmp280 = new BMP280();
  ok = bmp280->begin();  
  if (! ok) {
    delete bmp280;
    bmp280 = 0;
  }

  bme280 = new BME280();
  ok = bme280->begin();
  if (! ok) {
    delete bme280;
    bme280 = 0;
  }

  if (!ok) {
    Serial.println("Could not find a valid BME280 / BMP280 sensor");
    return;
  }
}

void loop() {
  ArduinoOTA.handle();

  if (ok) {
    Wunderground *data = ReadSensorInformation();

    time_t the_time = sntp_get_current_timestamp();
    bool newdst = IsDST2(day(the_time), month(the_time), dayOfWeek(the_time), hour(the_time));
    if (_isdst != newdst) {
      mySntpInit();
      _isdst = newdst;
    }

    if (data) {
      data->time = the_time;

      if (!UploadDataToWU(data))
        Serial.println("Error uploading to Weather Underground, trying next time");

      // MQTT
      if (!client.connected())
        reconnect();
      if (!client.loop())
        reconnect();

      ReportToMqtt(data);
    }
  }

  delay(UpdateInterval);
}

boolean UploadDataToWU(Wunderground *wup) {
  WiFiClientSecure client;

  Serial.print("Connecting to   : " + String(WU_UPDATE_HOST));
  // Serial.println();

  // Create SSL connection
  if (!client.connect(WU_UPDATE_HOST, HTTPS_PORT)) {
    Serial.println("Connection failed");
    return false;
  }

  float dewpt = wup->sensor_temperature - (100 - wup->sensor_humidity) / 5.0;

  // Put info in the right (String) variables
  String tempf		= String(wup->sensor_tempf);
  String dewptf		= String(dewpt*9/5+32);
  String humidity	= String(wup->sensor_humidity,2);
  String baromin	= String(wup->sensor_pressurem,5);

  String url =
    "/weatherstation/updateweatherstation.php?ID=" + String(WU_MY_STATION_ID)
    + "&PASSWORD=" + String(WU_MY_STATION_PASS)
    + "&dateutc=" + "now"
    + (indoor ? "&indoortempf=" : "&tempf=") + tempf
    + "&humidity=" + humidity
    + "&dewptf=" + dewptf
    + "&baromin=" + baromin
    + "&action=updateraw&realtime=1&rtfreq=60";

  // Serial.println("Requesting      : "+url);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + WU_UPDATE_HOST + "\r\n" +
               "User-Agent: Danny Backx Garden Sensor\r\n" +
               "Connection: close\r\n\r\n");
  // Serial.print("Request sent    : ");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      // Serial.println("Headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');

  boolean ok = true;
  // if (line == "success") line = "Server confirmed all data received";
  if (line == "INVALIDPASSWORDID|Password or key and/or id are incorrect") {
    line = "Invalid PWS/User data entered in the ID and PASSWORD or GET parameters";
    ok = false;
  }
  if (line == "RapidFire Server") {
    line = "The minimum GET parameters of ID, PASSWORD, action and dateutc were not set correctly";
    ok = false;
  }
  // Serial.println("Server Response : "+line);
  // Serial.println("Status          : Closing connection");
  
  if (ok)
    Serial.println(" -> ok");

  return ok;
}

Wunderground *ReadSensorInformation() {
  double t, h, p;

  if (bmp280) {
    char d3 = bmp280->startMeasurement();
    delay(d3);
    char d4 = bmp280->getTemperatureAndPressure(t, p);
    if (d4 == 0) { // Error communicating with device
      Serial.printf("BMP180 : communication error (pressure)\n");
      return 0;
    }
    h = 100.0;
  } else if (bme280) {
  #warning bme280
  } else {
    return 0;
  }

  // Safeguard against sensor communication failure
  if (t == 0 && p == 0)
    return 0;

  Wunderground *data = (Wunderground *)malloc(sizeof(Wunderground));
  if (! data) {
    Serial.printf("Malloc failed\n");
    return 0;
  }

  // Transform to imperial measurements (yuck, Wunderground requires this)
  data->sensor_humidity		= h;
  data->sensor_temperature	= t;
  data->sensor_tempf		= t * 9/5+32;
  data->sensor_pressure		= p;
  data->sensor_pressurem	= p * 0.0295300586;	// www.unitconverters.net

  int ta, tb, pa, tfa, pia, pib;
  ta = t;
  tb = 10 * (t-ta);
  pa = p;
  tfa = data->sensor_tempf;
  pia = data->sensor_pressurem;
  pib = 1000000 * (data->sensor_pressurem - pia);

  Serial.printf("Sensor read %d.%01d°C, %d hPa (imperial units : %d°F, pressure %d.%06d)\n",
    ta, tb, pa,
    tfa, pia, pib);

  return data;
}

// Support structure for WiFi
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

// Connect to WiFi
void SetupWifi() {
  int ix;
					// Try to connect to WiFi
  WiFi.mode(WIFI_STA);
					// Loop over known networks
  int wcr = WL_IDLE_STATUS;
  for (ix = 0; wcr != WL_CONNECTED && mywifi[ix].ssid != NULL; ix++) {
    int wifi_tries = 5;

    Serial.printf("\nTrying %s ", mywifi[ix].ssid);
    while (wifi_tries-- > 0) {
      Serial.print(".");
      WiFi.begin(mywifi[ix].ssid, mywifi[ix].pass);
      wcr = WiFi.waitForConnectResult();
      if (wcr == WL_CONNECTED)
        break;
      delay(250);
    }
  }
					// This fails if include file not read
  if (mywifi[0].ssid == NULL) {
    Serial.println("Configuration problem : we don't know any networks");
  }
					// Aw crap
  // Reboot if we didn't manage to connect to WiFi
  if (wcr != WL_CONNECTED) {
    Serial.println("Not connected -> reboot");
    delay(2000);
    ESP.restart();
  }
					// Report success
  IPAddress ip = WiFi.localIP();
  String ips = ip.toString();
  IPAddress gw = WiFi.gatewayIP();
  String gws = gw.toString();
  Serial.printf(" -> IP %s gw %s\n", ips.c_str(), gws.c_str());
}

// I2C write function must support repeated start to avoid interruption of transactions.
// User-provided function to write data_size bytes from buffer p_data to I2C device at bus address i2c_address.
void i2cWrite(uint8_t i2c_address, uint8_t *p_data, uint8_t data_size, uint8_t repeated_start) {
  Wire.beginTransmission(i2c_address);
  Wire.write(p_data,data_size);
  Wire.endTransmission(repeated_start!=0?false:true);
}

// User-provided function to read data_size bytes from I2C device at bus address i2c_address to buffer p_data.
void i2cRead(uint8_t i2c_address, uint8_t *p_data, uint8_t data_size) {
  uint8_t i = 0;
  Wire.requestFrom(i2c_address,data_size);
  while (Wire.available() && i<data_size) {
    p_data[i] = Wire.read();
    i += 1;
    }
}

// Empty support function for Elektor's BME280 driver
void spiWrite(uint8_t *p_data, uint8_t data_size) {
}

// Empty support function for Elektor's BME280 driver
void spiRead(uint8_t *p_data, uint8_t data_size) {
}

void SetupOTA() {
  Serial.printf("Set up OTA (id %s) at port %d ... ", OTA_ID, OTA_PORT);
  ArduinoOTA.onStart([]() { Serial.print("OTA "); });

  ArduinoOTA.onEnd([]() { Serial.print("\nOTA complete\n"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { Serial.print("."); });

  ArduinoOTA.onError([](ota_error_t error) { Serial.print("\nOTA Error\n"); });

  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(OTA_ID);
  WiFi.hostname(OTA_ID);

  ArduinoOTA.begin();
  Serial.println(" done");
}

void reconnect(void) {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Attempt to connect
    if (client.connect(MQTT_CLIENT)) {
      if (mqtt_initial) {
      // Once connected, publish an announcement...
	mqtt_initial = 0;
      } else {
      }

      // ... and (re)subscribe
      client.subscribe(MQTT_TOPIC "/#");
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/*
 * Respond to MQTT queries.
 * Only two types are subscribed to :
 *   topic == "/weather"		This is our own message
 *   topic == "/weather/reboot"		This is a request
 *   topic == "/weather/query"		This is a request
 */
void callback(char *topic, byte *payload, unsigned int length) {
  char reply[80];

  char pl[80];
  strncpy(pl, (const char *)payload, length);
  pl[length] = 0;

  if (strcmp(topic, MQTT_TOPIC) == 0) {
    ;	// Silently ignore
  } else if (strcmp(topic, MQTT_TOPIC "/reboot") == 0) {
  } else if (strcmp(topic, MQTT_TOPIC "/query") == 0) {
    // Set the schedule according to the string provided
    // SetSchedule((char *)pl);
    client.publish(MQTT_TOPIC "/reply", "Ok");
  }
  // Else silently ignore
}

void ReportToMqtt(Wunderground *data) {
  char line[80], tbuf[32];

  int ta, tb, pa;
  ta = data->sensor_temperature;
  tb = 10 * (data->sensor_temperature - ta);
  pa = data->sensor_pressure;

  struct tm *tmp = localtime(&data->time);
  strftime(tbuf, sizeof(tbuf), "%F %R", tmp);

  if (indoor)
    sprintf(line, "%s, indoor %d.%01d°C, %d hPa", tbuf, ta, tb, pa);
  else
    sprintf(line, "%s, sensor %d.%01d°C, %d hPa", tbuf, ta, tb, pa);

  if (client.connected())
    client.publish(MQTT_TOPIC, line);
  Serial.printf("MQTT : %s\n", line);
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
