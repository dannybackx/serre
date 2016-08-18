/*
*/

#include <stdio.h>

char *getenv(char *varname) {
  if (strcmp(varname, "TZ") == 0) {
    return "CET+1:00:00CETDST+2:00:00,M3.5.0,M10.5.0";
  }
  return NULL;
}
