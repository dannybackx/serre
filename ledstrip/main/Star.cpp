/*
 * Animation for a 5-point star
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
  {  0,  1,  2,  3,	28, 29, 30, 31,		56, 57, 58, 59 },	// triangle 1
  {  4,  5,  6,  7,	32, 33, 34, 35,		36, 37, 38, 39 },	// triangle 2
  {  8,  9, 10, 11,	12, 13, 14, 15,		40, 41, 42, 43 },	// triangle 3
  { 16, 17, 18, 19,	44, 45, 46, 47,		48, 49, 50, 51 },	// triangle 4
  { 20, 21, 22, 23,	24, 25, 26, 27,		52, 53, 54, 55 }	// triangle 5
};

static int i = 0;

void star_loop()
{
  // Default = dark
  for (CRGB& pixel : leds)
    pixel = CRGB::Black;

  // Running dot
  for (int star = 0; star < 6; star++) {
    int x = triangle[star][i];
    leds[x] = CRGB::Red;
  }

  i = (i + 1) % 12;

  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}
