#pragma once
// 2wire interface for SHT41 humidty/temperature sensor
class SHT41 {
public:
    SHT41(uint8_t addr = SLAVE_ADDRESS, unsigned DelayMsec = SETTLE_TIME_MSEC) : SlaveAddress(addr), SettleTimeMsec(DelayMsec), m_rawhumidity(0)
    {}

    void startReadTemperature()
    {
        Wire.beginTransmission(SlaveAddress);    
        Wire.write(static_cast<uint8_t>(READ_HIGH_PRECISION));
        Wire.endTransmission(); 
        MsecWhenStartedTemperature = millis();
    }

    int16_t finishReadTempCx256()
    {
        long sht41Delay = millis() - MsecWhenStartedTemperature;
        sht41Delay = static_cast<long>(SettleTimeMsec) - sht41Delay;
        if (sht41Delay > 0)
            delay(sht41Delay);

        uint8_t buffer[READ_BUFFER_SIZE];
        Wire.requestFrom(SlaveAddress, static_cast<uint8_t>(READ_BUFFER_SIZE));
        for (int i = 0; i < READ_BUFFER_SIZE; i++)
        {
            if (Wire.available())
            {
                unsigned char v = Wire.read();
                buffer[i] = v;
            }
            else
                return 100 * 256; // invalid
        }
        uint16_t tTicks = buffer[1] | (0xFF00 & (static_cast<uint16_t>(buffer[0]) << 8));
        m_rawhumidity = buffer[4] | (0xFF00 & (static_cast<uint16_t>(buffer[3]) << 8));
        return (-45L << 8) + ((175L * tTicks) >> 8);
    }

    int16_t  humidityX256()
    {
        return (-6L << 8) + ((125L * m_rawhumidity) >> 8);;
    }

    void delayForADC()
    {
        delay(SettleTimeMsec); // let temperature ADC settle--depends on R0/R1
    }

    int16_t readTempCx256()
    {
        startReadTemperature();
        delayForADC();
        return finishReadTempCx256();
    }
    
    
private:
    enum {SETTLE_TIME_MSEC = 10, SLAVE_ADDRESS = 0x44, };
    enum {READ_HIGH_PRECISION = 0xfd};
    enum {READ_BUFFER_SIZE = 6};
    uint16_t  m_rawhumidity;
    const uint8_t SlaveAddress;
    unsigned long MsecWhenStartedTemperature;
    const unsigned SettleTimeMsec;
};
