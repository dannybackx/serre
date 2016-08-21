#include "SFE_BMP180.h"


extern SFE_BMP180	*bmp;
extern void BMPQuery();

extern double		newPressure, newTemperature, oldPressure, oldTemperature;
extern int		valve;
extern time_t		tsboot;
extern int		nreconnects;
extern struct tm	*now;
