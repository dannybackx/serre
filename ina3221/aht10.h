#include <Adafruit_AHTX0.h>

extern void aht10_begin();
extern void aht10_loop(time_t);

extern struct aht_reg {
  time_t ts;
  float temp, hum;
} aht_reg[];
