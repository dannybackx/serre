/*
*/

#include <stdio.h>
#include "personal.h"

char *getenv(char *varname) {
  if (strcmp(varname, "TZ") == 0) {
    return personal_timezone;
  }
  return NULL;
}
