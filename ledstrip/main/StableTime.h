/*
 * Call this module instead of time(0) or gettimeofday(&x, 0) who both freeze occasionally.
 *
 * Copyright (c) 2019, 2020 by Danny Backx
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

#ifndef	_STABLETIME_H_
#define	_STABLETIME_H_

#include <sys/time.h>

class StableTime {
public:
  StableTime();
  StableTime(const char *);
  ~StableTime();
  void loop(time_t);
  void loop(struct timeval *);
  time_t loop();
  time_t Query();
  void Query(struct timeval *);
  char *TimeStamp();
  char *TimeStamp(time_t);

private:
  struct timeval	now;
  const char *st_tag = "StableTime";
  char ts[24];
};

extern StableTime *stableTime;

#endif	/* _STABLETIME_H_ */
