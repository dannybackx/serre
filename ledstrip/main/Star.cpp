/*
 * Animations for a 5-point star
 *
 * Copyright (c) 2020 Danny Backx
 *
 * License (GNU Lesser General Public License) :
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 3 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "App.h"
#include <FastLED.h>

#define BRIGHTNESS  200
#define FRAMES_PER_SECOND 20

/*
 * Note : 60 LEDs, in 5 stretches of 12, compose the star.
 *
 * So there are 5 triangles
 */
char triangle[5][12] = {
  {  0,  1,  2,  3,	31, 30, 29, 28,		56, 57, 58, 59 },	// triangle 1
  { 36, 37, 38, 39,	 7,  6,  5,  4,		32, 33, 34, 35 },	// triangle 2
  { 12, 13, 14, 15,	43, 42, 41, 40,		 8,  9, 10, 11 },	// triangle 3
  { 48, 49, 50, 51,	19, 18, 17, 16,		44, 45, 46, 47 },	// triangle 4
  { 24, 25, 26, 27,	55, 54, 53, 52,		20, 21, 22, 23 }	// triangle 5
};

static int i = 0;

void star_running_dot()
{
  // Default = dark
  for (CRGB& pixel : leds)
    pixel = CRGB::Black;

  // Running dot
  for (int star = 0; star < 5; star++) {
    int x = triangle[star][i];
    if (0 <= x && x < MY_NUM_LEDS) {
      leds[x] = CRGB::Red;
    }
  }

  i = (i + 1) % 12;

  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void star_chasing_dot() {
  // Default = dark
  for (CRGB& pixel : leds)
    pixel = CRGB::Black;

  // Running dot
  for (int star = 0; star < 5; star++) {
    int x = triangle[star][i];
    if (0 <= x && x < MY_NUM_LEDS) {
      leds[x] = CRGB::Red;
      if (i > 0) {
	// Chase the red dot with a green one
	int y = triangle[star][i-1];
	if (0 <= y && y < MY_NUM_LEDS)
	  leds[y] = CRGB::Green;
      }
    }
  }

  i = (i + 1) % 12;

  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void star_triangle() {
  i = (i + 1) % 7;

  // char line[80];
  // strcpy(line, "colours                                                                        ");
  // Every triangle has its colour
  for (int tr=0; tr < 5; tr++) {
    CRGB colour;
    char cv = ' ';
    int y = (tr + i) % 7;
         if (y == 0) { colour = CRGB::Red;	cv = 'r'; }
    else if (y == 1) { colour = CRGB::Orange;	cv = 'o'; }
    else if (y == 2) { colour = CRGB::Yellow;	cv = 'y'; }
    else if (y == 3) { colour = CRGB::Green;	cv = 'g'; }
    else if (y == 4) { colour = CRGB::Blue;	cv = 'b'; }
    else if (y == 5) { colour = CRGB::Indigo;	cv = 'i'; }
    else if (y == 6) { colour = CRGB::Violet;	cv = 'v'; }
    for (int x=0; x<12; x++) {
      int i=triangle[tr][x];
      if (0 <= i && i < MY_NUM_LEDS) {
        leds[i] = colour;
	// line[i + 8] = cv;
      }
    }
  }

  // ESP_LOGI("star", "%s", line);

  FastLED.show(); // display this frame
  FastLED.delay(1000);
}

void star_loop(int i) {
  switch (i) {
  case 1:
  default:
    star_running_dot();
    break;
  case 2:
    star_chasing_dot();
    break;
  case 3:
    star_triangle();
    break;
  }
}
