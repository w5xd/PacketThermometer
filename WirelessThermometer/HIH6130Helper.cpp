/******************************************************************************
based on:

SparkFunTMP102.cpp
SparkFunTMP102 Library Source File
Alex Wende @ SparkFun Electronics
Original Creation Date: April 29, 2016
https://github.com/sparkfun/Digital_Temperature_Sensor_Breakout_-_TMP102

******************************************************************************/
#include <Arduino.h>
#include "HIH6130Helper.h"
#include <Wire.h>

namespace HomeAutomationTools {

    HIH6130::HIH6130(byte address)
    	: _address(address)
    {
    }

    void HIH6130::begin(void)
    {
    }

    unsigned char HIH6130::GetReadings(float &humidity, float &tempC)
    {
		humidity = -1;
		tempC = 125;

    	Wire.beginTransmission(_address); // start digitization
    	Wire.endTransmission();
    	delay(70); // spec is 60 msec measurement delay
    	Wire.requestFrom(_address, 4);
    	byte b[4];
		for (uint8_t i = 0; i < 4; i++)
		{
			if (Wire.available())
				b[i] = Wire.read();
			else
				return 0xFF;
		}
    	unsigned char ret = (b[0] >> 6) & 0x3;
    	uint16_t h = b[0] & 0x3F;
    	h <<= 8;
    	h |= b[1] & 0xFF;
    	humidity = static_cast<float>(h) * 100.f / static_cast<float>(0x3ffe);
    	uint16_t t = b[2];
    	t <<= 8;
    	t |= b[3] & 0xFF0;
    	t >>= 2;
    	tempC = -40 + static_cast<float>(t) * 165.f / static_cast<float>(0x3ffe);
    	return ret;
    }


    void HIH6130::end()
    {
    }



}
