#pragma once

class SI7021
{
public:
    SI7021() : MsecWhenStarted(millis())
    {}


    void startReadHumidity() // by definitions, start T and RH both. But T cannot be read with CRC8
    {
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(MEASURE_RH);
        Wire.endTransmission();
        MsecWhenStarted = millis();
    }

    void startReadTemperature()
    { 
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(MEASURE_T);
        Wire.endTransmission();
        MsecWhenStarted = millis();
    }

    float readHumidity()
    {
        uint16_t raw = read();
        if (raw == static_cast<uint16_t>(-1))
            return -1;
        return  125.f * static_cast<float>(raw) / 65536.f -6.0f; 
    }

    float readTemperature()
    {
        uint16_t raw = read();
        if (raw == static_cast<uint16_t>(-1))
            return -999.f;
        return  (175.72f / 65536.f) * static_cast<float>(raw) - 46.85f;
    }

protected:
    uint16_t read()
    {
        long toWait = MEASURE_MSEC - millis() - MsecWhenStarted;
        if (toWait > 0)
            delay(static_cast<int>(toWait));
        static const int RETRY = 10;
        for (int i = 0; i < RETRY; i++)
        {
            Wire.requestFrom(I2C_ADDR, READ_BYTES);
            if (Wire.available() == 3)
            {
                uint8_t data[2];
                data[0] = Wire.read();
                data[1] = Wire.read();
                unsigned char crc = Wire.read();
                unsigned char calc = crc8(data, sizeof(data));
#if 0
                Serial.print("CRC: 0x");
                Serial.print((int)crc, HEX);
                Serial.print(" 0x");
                Serial.println((int)calc, HEX);
#endif
                if (crc == calc)
                    return (static_cast<uint16_t>(data[0]) << 8) | data[0];
            }
            else
            {
                delay(POLL_MSEC);
                continue;
            }
        }
        return -1;
    }

    uint8_t crc8(uint8_t data[], uint8_t c) 
    {
        static const uint16_t POLYNOMIAL = 0x131;  // P(x)=x^8+x^5+x^4+1 = 100110001
        uint8_t crc = 0;
        for (uint8_t byteCtr = 0; byteCtr < c; ++byteCtr)
        {
            crc ^= (data[byteCtr]);
            for (uint8_t bit = 8; bit > 0; --bit)
            {
                if (crc & 0x80) crc = (crc << 1) ^ POLYNOMIAL;
                else crc = (crc << 1);
            }
        }
        return crc;
    }

    enum Commands { MEASURE_RH = 0xF5, MEASURE_T =  0xF3,  // These are not the Hold Master Mode
        WRITE_USER_REG = 0xE6,
        I2C_ADDR = 0x40, // Part is fixed at 0x40
        MEASURE_MSEC = 12,
        READ_BYTES = 3,
        POLL_MSEC= 2};
    unsigned long MsecWhenStarted;
};
