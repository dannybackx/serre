#include "App.h"
#include <FastLED.h>

#define BRIGHTNESS  200
#define FRAMES_PER_SECOND 20

static int i = 0;

void cylon_loop()
{
  for (CRGB& pixel: leds) {
    pixel = CRGB::Black;
  }
  leds[i] = CRGB::Red;
  if (i > 0)
    leds[i-1] = CRGB::Green;
  i = (i + 1) % MY_NUM_LEDS;

  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}
