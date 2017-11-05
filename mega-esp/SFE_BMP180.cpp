/*
 * SFE_BMP180.cpp
 * Bosch BMP180 pressure sensor library for the Arduino microcontroller
 * Mike Grusin, SparkFun Electronics
 * 
 * Uses floating-point equations from the Weather Station Data Logger project
 * http://wmrx00.sourceforge.net/
 * http://wmrx00.sourceforge.net/Arduino/BMP085-Calcs.pdf
 * 
 * Forked from BMP085 library by M.Grusin
 * 
 * version 1.0 2013/09/20 initial version
 * Version 1.1.2 - Updated for Arduino 1.6.4 5/2015
 * BMP280 support, copyright (c) 2017 by Danny Backx
 * 
 * Our example code uses the "beerware" license. You can do anything
 * you like with this code. No really, anything. If you find it useful,
 * buy me a (root) beer someday.
*/

#include "SFE_BMP180.h"
#include <Wire.h>
#include <stdio.h>
#include <math.h>

#include <ELClient.h>
#include <ELClientCmd.h>
#include <ELClientMqtt.h>
#include <ELClientRest.h>

#include <TimeLib.h>
#include "Hatch.h"
#include "Light.h"
#include "ThingSpeak.h"
#include <DS1307RTC.h>
#include "Ifttt.h"
#include <Dyndns.h>
#include "Sunset.h"

#include "buildinfo.h"
#include "global.h"


SFE_BMP180::SFE_BMP180()
// Base library type
{
}


char SFE_BMP180::begin()
// Initialize library for subsequent pressure measurements
{
	double c3,c4,b1;
	
	// Start up the Arduino's "wire" (I2C) library:
	// Wire.begin();

	int cnt = 0;
	_error = 1;

	// Deal with i2c not being active yet ?
	while (cnt++ < 10 && _error != 0) {
	  chip_addr = ((cnt % 2) == 0) ? BMP180_ADDR : BMP280_ADDR;

	  Wire.beginTransmission(chip_addr);
	  _error = Wire.endTransmission();

	  delay(100);
	}

	// Retrieve BMP180 Chip ID
	// The Bosch BMP180 spec sheet says you can read the Chip ID to assess whether
	// communication is working. 0x55 for BMP180, 0x58 for BMP280

	uint8_t chipid = 0;
	int n = 0, count = 5;
	while (n == 0 && chipid == 0 && count-- > 0) {
	  // Serial.print("BMPx80 : n "); Serial.print(n); Serial.print(" chipid "); Serial.println(chipid);
	  n = readUInt8(0xD0, chipid);
	  delay(100);
	}

	Serial.print("BMPx80 : n "); Serial.print(n); Serial.print(" chipid "); Serial.println(chipid);
	// Debug("BMPx80 chip id is %d", chipid);

	if (n != 1)
	  return 0;
	if (chip_addr == BMP180_ADDR && chipid != 0x55)
	  return 0;	// Error
	if (chip_addr == BMP280_ADDR && chipid != 0x58)
	  return 0;	// Error

	// The BMP180 includes factory calibration data stored on the device.
	// Each device has different numbers, these must be retrieved and
	// used in the calculations when taking pressure measurements.

	// Retrieve calibration data from device:
	if (readInt(0xAA,AC1) &&
		readInt(0xAC,AC2) &&
		readInt(0xAE,AC3) &&
		readUInt(0xB0,AC4) &&
		readUInt(0xB2,AC5) &&
		readUInt(0xB4,AC6) &&
		readInt(0xB6,VB1) &&
		readInt(0xB8,VB2) &&
		readInt(0xBA,MB) &&
		readInt(0xBC,MC) &&
		readInt(0xBE,MD))
	{

		// All reads completed successfully!

		// Example from Bosch datasheet
		// AC1 = 408; AC2 = -72; AC3 = -14383; AC4 = 32741; AC5 = 32757; AC6 = 23153;
		// B1 = 6190; B2 = 4; MB = -32768; MC = -8711; MD = 2868;

		// Example from http://wmrx00.sourceforge.net/Arduino/BMP180-Calcs.pdf
		// AC1 = 7911; AC2 = -934; AC3 = -14306; AC4 = 31567; AC5 = 25671; AC6 = 18974;
		// VB1 = 5498; VB2 = 46; MB = -32768; MC = -11075; MD = 2432;

		// Compute floating-point polynominals:

		c3 = 160.0 * pow(2,-15) * AC3;
		c4 = pow(10,-3) * pow(2,-15) * AC4;
		b1 = pow(160,2) * pow(2,-30) * VB1;
		c5 = (pow(2,-15) / 160) * AC5;
		c6 = AC6;
		mc = (pow(2,11) / pow(160,2)) * MC;
		md = MD / 160.0;
		x0 = AC1;
		x1 = 160.0 * pow(2,-13) * AC2;
		x2 = pow(160,2) * pow(2,-25) * VB2;
		y0 = c4 * pow(2,15);
		y1 = c4 * c3;
		y2 = c4 * b1;
		p0 = (3791.0 - 8.0) / 1600.0;
		p1 = 1.0 - 7357.0 * pow(2,-20);
		p2 = 3038.0 * 100.0 * pow(2,-36);

		// Success!
		return 1;
	}
	else
	{
		// Error reading calibration data; bad component or connection?
		return 0;
	}
}


char SFE_BMP180::readInt(char address, int16_t &value)
// Read a signed integer (two bytes) from device
// address: register to start reading (plus subsequent register)
// value: external variable to store data (function modifies value)
{
	unsigned char data[2];

	data[0] = address;
	if (readBytes(data,2))
	{
		value = (int16_t)((data[0]<<8)|data[1]);
		//if (*value & 0x8000) *value |= 0xFFFF0000; // sign extend if negative
		return(1);
	}
	value = 0;
	return(0);
}


char SFE_BMP180::readUInt(char address, uint16_t &value)
// Read an unsigned integer (two bytes) from device
// address: register to start reading (plus subsequent register)
// value: external variable to store data (function modifies value)
{
	unsigned char data[2];

	data[0] = address;
	if (readBytes(data,2))
	{
		value = (((uint16_t)data[0]<<8)|(uint16_t)data[1]);
		return(1);
	}
	value = 0;
	return(0);
}


char SFE_BMP180::readUInt8(char address, uint8_t &value) {
#if 1
  Wire.beginTransmission(chip_addr);
  Wire.write(address);
  Wire.endTransmission();
  Wire.requestFrom(chip_addr, (byte)1);
  value = Wire.read();
  return 1;
#else
	unsigned char data[2];

	data[0] = address;
	if (readBytes(data, 1)) {
		value = data[0];
		return 1;
	}
	value = 0;
	return 0;
#endif
}

char SFE_BMP180::readBytes(unsigned char *values, char length)
// Read an array of bytes from device
// values: external array to hold data. Put starting register in values[0].
// length: number of bytes to read
{
	char x;

	Wire.beginTransmission(chip_addr);
	Wire.write(values[0]);
	_error = Wire.endTransmission();
	if (_error == 0)
	{
		Wire.requestFrom((uint8_t)chip_addr, (uint8_t)length);
		while(Wire.available() != length) ; // wait until bytes are ready
		for(x=0;x<length;x++)
		{
			values[x] = Wire.read();
		}
		return(1);
	}
	return(0);
}


char SFE_BMP180::writeBytes(unsigned char *values, char length)
// Write an array of bytes to device
// values: external array of data to write. Put starting register in values[0].
// length: number of bytes to write
{
	char x;
	
	Wire.beginTransmission(chip_addr);
	Wire.write(values,length);
	_error = Wire.endTransmission();
	if (_error == 0)
		return(1);
	else
		return(0);
}


char SFE_BMP180::startTemperature(void)
// Begin a temperature reading.
// Will return delay in ms to wait, or 0 if I2C error
{
	unsigned char data[2], result;
	
	data[0] = BMP180_REG_CONTROL;
	data[1] = BMP180_COMMAND_TEMPERATURE;
	result = writeBytes(data, 2);
	if (result) // good write?
		return(5); // return the delay in ms (rounded up) to wait before retrieving data
	else
		return(0); // or return 0 if there was a problem communicating with the BMP
}


char SFE_BMP180::getTemperature(double &T)
// Retrieve a previously-started temperature reading.
// Requires begin() to be called once prior to retrieve calibration parameters.
// Requires startTemperature() to have been called prior and sufficient time elapsed.
// T: external variable to hold result.
// Returns 1 if successful, 0 if I2C error.
{
	unsigned char data[2];
	char result;
	double tu, a;
	
	data[0] = BMP180_REG_RESULT;

	result = readBytes(data, 2);
	if (result) // good read, calculate temperature
	{
		tu = (data[0] * 256.0) + data[1];

		//example from Bosch datasheet
		//tu = 27898;

		//example from http://wmrx00.sourceforge.net/Arduino/BMP085-Calcs.pdf
		//tu = 0x69EC;
		
		a = c5 * (tu - c6);
		T = a + (mc / (a + md));
	}
	return(result);
}


char SFE_BMP180::startPressure(char oversampling)
// Begin a pressure reading.
// Oversampling: 0 to 3, higher numbers are slower, higher-res outputs.
// Will return delay in ms to wait, or 0 if I2C error.
{
	unsigned char data[2], result, delay;
	
	data[0] = BMP180_REG_CONTROL;

	switch (oversampling)
	{
		case 0:
			data[1] = BMP180_COMMAND_PRESSURE0;
			delay = 5;
		break;
		case 1:
			data[1] = BMP180_COMMAND_PRESSURE1;
			delay = 8;
		break;
		case 2:
			data[1] = BMP180_COMMAND_PRESSURE2;
			delay = 14;
		break;
		case 3:
			data[1] = BMP180_COMMAND_PRESSURE3;
			delay = 26;
		break;
		default:
			data[1] = BMP180_COMMAND_PRESSURE0;
			delay = 5;
		break;
	}
	result = writeBytes(data, 2);
	if (result) // good write?
		return(delay); // return the delay in ms (rounded up) to wait before retrieving data
	else
		return(0); // or return 0 if there was a problem communicating with the BMP
}


char SFE_BMP180::getPressure(double &P, double &T)
// Retrieve a previously started pressure reading, calculate abolute pressure in mbars.
// Requires begin() to be called once prior to retrieve calibration parameters.
// Requires startPressure() to have been called prior and sufficient time elapsed.
// Requires recent temperature reading to accurately calculate pressure.

// P: external variable to hold pressure.
// T: previously-calculated temperature.
// Returns 1 for success, 0 for I2C error.

// Note that calculated pressure value is absolute mbars, to compensate for altitude call sealevel().
{
	unsigned char data[3];
	char result;
	double pu,s,x,y,z;

	data[0] = BMP180_REG_RESULT;

	result = readBytes(data, 3);
	if (result) // good read, calculate pressure
	{
		pu = (data[0] * 256.0) + data[1] + (data[2]/256.0);

		//example from Bosch datasheet
		//pu = 23843;

		//example from http://wmrx00.sourceforge.net/Arduino/BMP085-Calcs.pdf, pu = 0x982FC0;	
		//pu = (0x98 * 256.0) + 0x2F + (0xC0/256.0);
		
		s = T - 25.0;
		x = (x2 * pow(s,2)) + (x1 * s) + x0;
		y = (y2 * pow(s,2)) + (y1 * s) + y0;
		z = (pu - x) / y;
		P = (p2 * pow(z,2)) + (p1 * z) + p0;
	}
	return(result);
}


double SFE_BMP180::sealevel(double P, double A)
// Given a pressure P (mb) taken at a specific altitude (meters),
// return the equivalent pressure (mb) at sea level.
// This produces pressure readings that can be used for weather measurements.
{
	return(P/pow(1-(A/44330.0),5.255));
}


double SFE_BMP180::altitude(double P, double P0)
// Given a pressure measurement P (mb) and the pressure at a baseline P0 (mb),
// return altitude (meters) above baseline.
{
	return(44330.0*(1-pow(P/P0,1/5.255)));
}


char SFE_BMP180::getError(void)
	// If any library command fails, you can retrieve an extended
	// error code using this command. Errors are from the wire library: 
	// 0 = Success
	// 1 = Data too long to fit in transmit buffer
	// 2 = Received NACK on transmit of address
	// 3 = Received NACK on transmit of data
	// 4 = Other error
{
	return(_error);
}

