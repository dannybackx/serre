#include <Adafruit_AHTX0.h>
#include "aht10.h"
#include "measure.h"

Adafruit_AHTX0 aht;
bool sensor = false;
time_t prev_ts = 0;

struct aht_reg aht_reg[100];
int ix = 0;

void aht_register(time_t ts, float temp, float hum) {
  aht_reg[ix].ts = ts;
  aht_reg[ix].hum = hum;
  aht_reg[ix].temp = temp;
  ix = (ix + 1) % 100;
}

void aht10_begin() {
  if (! aht.begin())
    Serial.println("No AHT sensor");
  else
    sensor = true;

  prev_ts = time(0);
}

void aht10_loop(time_t t) {
  time_t now = time(0);

  if (now - prev_ts < 2)
    return;
  prev_ts = now;

  if (sensor) {
    sensors_event_t	humidity, temp;
    aht.getEvent(&humidity, &temp);
    Serial.printf("Temp %3.1f hum %2.0f (ts %s)\n", temp.temperature, humidity.relative_humidity, timestamp(t));

    aht_register(t, temp.temperature, humidity.relative_humidity);
  }
}
