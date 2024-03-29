#pragma once
/******************************************************************************
Based on:



Distributed as-is; no warranty is given.
******************************************************************************/
namespace HomeAutomationTools {
    class HIH6130
    {
    public:
        HIH6130(byte address=0x27);	// Initialize HIH6130 sensor at given address
        void begin(void);  
        void end(void); 
        unsigned char GetReadings(float &humidity, float &tempC);

    protected:
        const int _address; // Address of Temperature sensor (0x27)
    };

}

