/* This sketch is for an Arduino wired to
** a) RFM69 radio transceiver
** b) temperature sensor or combination temperature/humidity sensor
**
** This sketch is accompanied in its git repository by a design for a 1.7" by 2.5" PCB.
** That PCB makes the connections more convenient, and the packaging solid, but its
** also workable to simply use the sparkfun breakout packaging of the RFM69 and either
** the TMP102 or the Si7021 and haywire the three together with a 2 cell battery.
**
** This sketch has compile time #define's for the various i2c sensors. While multiple
** sensors can be compiled in, the #define's in this sketch arrange for only one of the
** sensor readings to be telemetered.
*/

#include <RadioConfiguration.h>
#include <SPI.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/power.h>

// SparkFun's part numbers are:
// 915MHz: https://www.sparkfun.com/products/12775 (or to SMD mount: https://www.sparkfun.com/products/13909)
// 434MHz: https://www.sparkfun.com/products/12823 (SMD part: https://www.sparkfun.com/products/13910)

// Parts of the code in this sketch are taken from these sparkfun pages,
// as are all the wiring instructions:
// https://learn.sparkfun.com/tutorials/rfm69hcw-hookup-guide

// Uses the RFM69 library by Felix Rusu, LowPowerLab.com
// Original library: https://github.com/LowPowerLab/RFM69

// code only supports reporting any one of TMP102 HIH6130 TMP175 SI7021
// except: SI7021 can also be paired with TMP sensor
//#define USE_TMP102
//#define USE_HIH6130
#define USE_TMP175
//#define USE_SI7021

// The TMP102 has temperature only, -40C to 100C
// The HIH6130 has temperature and relative humidity, -20C to 85C

// Include the RFM69 and SPI libraries:
#define USE_RFM69
#define USE_SERIAL
//if a permanent power source is connected, comment out the next line...
#define TELEMETER_BATTERY_V // ...because the power supply voltage wont ever change.

#define TIMER_INIT_STYLE_REV05 5    // REV05 of PCB and earlier. R and C across D3 and D4
#define TIMER_INIT_STYLE_REV06 6  // REV06 of PCB ONLY. with SN74AHC1 Schmitt trigger. 
#define TIMER_INIT_STYLE_REV07 7    // REV07 of PCB. D3 and D4 participate in charge pump

#define TIMER_INIT_STYLE TIMER_INIT_STYLE_REV07 // One of the above

// Using TIMER2 to sleep costs about 200uA of sleep-time current, but saves the 1uF/10Mohm external parts
//#define SLEEP_WITH_TIMER2

#if defined(USE_RFM69)
#include <RFM69.h>
#endif

#include <Wire.h>

#if defined(USE_HIH6130)
#include "HIH6130Helper.h"
#endif
#if defined(USE_TMP175) || defined(USE_TMP102)
#include "Tmp175.h"
#endif
#if defined(USE_SI7021)
#include "Si7021.h"
#endif

#define VERSION_STRING "REV 15"

namespace {
    const int BATTERY_PIN = A0; // digitize (fraction of) battery voltage
    const int TIMER_RC_GROUND_PIN = 4;
    const int TIMER_RC_PIN = 3; // sleep uProc using RC circuit on this pin

    const unsigned long FirstListenAfterTransmitMsec = 20000;// at system reset, listen Serial/RF for this long
    const unsigned long NormalListenAfterTransmit = 300;// after TX, go to RX for this long

    const float MAX_VALID_TEMPERATURE_C = 80.f;

#if defined(USE_TMP102) // The TMP102 is documented to be backwards compatible with TMP75 i2c commands.
    TMP175 tmp102(0x49, 30); /* PCB layout puts tmp102 at 0x49.*/
    //TMP175 tmp102(0x48, 30); /* TMP102 breakout board gives 0x48*/
#endif
#if defined(USE_HIH6130)
    HomeAutomationTools::HIH6130 sensor0;
#endif
#if defined(USE_TMP175)
    TMP175 tmp175(0x37); /* PCB layout puts tmp175 at 0x37*/
#endif
#if defined(USE_SI7021)
    SI7021 si7021;
#endif

#if defined(USE_RFM69)
// AES encryption (or not):
    const bool ENCRYPT = true; // Set to "true" to use encryption
    // Use ACKnowledge when sending messages (or not):
    const bool USEACK = true; // Request ACKs or not
    const int RFM69_RESET_PIN = 9;
    const int RFM69_CHIP_SELECT_PIN = 10;
    const int RFM69_INT_PIN = 2;
 
    // Create a library object for our RFM69HCW module:
    RFM69 radio(RFM69_CHIP_SELECT_PIN, RFM69_INT_PIN, true);
#endif

#if defined(TELEMETER_BATTERY_V)
    void ResetAnalogReference();
#endif

    RadioConfiguration radioConfiguration;
    bool enableRadio = false;
    unsigned long TimeOfWakeup;
    const unsigned MAX_SLEEP_LOOP_COUNT = 5000; // a couple times per day is minimum check-in interval
    unsigned SleepLoopTimerCount = 30; // approx 10 seconds per Count
    uint8_t GatewayNodeId = 1;
    uint8_t D4PwmCount = 100; // range is 0 to 255
    enum EepromAddress_t {SLEEP_TIMER_COUNT_OFFSET = RadioConfiguration::TOTAL_EEPROM_USED,
            GATEWAY_NODEID_OFFSET = SLEEP_TIMER_COUNT_OFFSET + sizeof(SleepLoopTimerCount),
            D4PWMCOUNT = GATEWAY_NODEID_OFFSET + sizeof(GatewayNodeId),
            THERMOMETER_EEPROM_USED = D4PWMCOUNT + sizeof(D4PwmCount)};
}

void setup()
{
#if defined(USE_SERIAL)
    // Open a serial port so we can send keystrokes to the module:

    Serial.begin(9600);

#if defined(USE_SI7021)
    Serial.println("PacketThermometer " VERSION_STRING " SI7021");
#endif
#if defined(USE_TMP102)
    Serial.println("PacketThermometer " VERSION_STRING " TMP102");
#endif
#if defined(USE_HIH6130)
    Serial.println("PacketThermometer " VERSION_STRING " HIH6130");
#endif
#if defined(USE_TMP175)
    Serial.println("PacketThermometer " VERSION_STRING " TMP175");
#endif
#if TIMER_INIT_STYLE <= TIMER_INIT_STYLE_REV05
    Serial.println(F("TIMER INIT R AND C"));
#elif TIMER_INIT_STYLE == TIMER_INIT_STYLE_REV06
    Serial.println(F("TIMER INIT IS PCB REV06 ONLY"));
#elif TIMER_INIT_STYLE >= TIMER_INIT_STYLE_REV07
   Serial.println(F("TIMER INIT IS CHARGE PUMP"));
#endif

    Serial.print("Node ");
    Serial.print(radioConfiguration.NodeId(), DEC);
    Serial.print(" on network ");
    Serial.print(radioConfiguration.NetworkId(), DEC);
    Serial.print(" band ");
    Serial.print(radioConfiguration.FrequencyBandId(), DEC);
    Serial.print(" key ");
    radioConfiguration.printEncryptionKey(Serial);
    Serial.println(" ready");
#endif

    Wire.begin();

#if defined(USE_TMP102)
    tmp102.startReadTemperature();
#endif
#if defined(USE_TMP175)
    tmp175.startReadTemperature();
#endif
#if defined(USE_SI7021)
#endif

#if defined(USE_RFM69)
    pinMode(RFM69_CHIP_SELECT_PIN, OUTPUT);
    digitalWrite(RFM69_CHIP_SELECT_PIN, HIGH);
    SPI.begin();

    // Initialize the RFM69HCW:
    auto nodeId = radioConfiguration.NodeId();
    auto networkId = radioConfiguration.NetworkId();
    auto fbId = radioConfiguration.FrequencyBandId();
    auto ok = nodeId != 0xff &&
        networkId != 0xff &&
        fbId != 0xff &&
        radio.initialize(fbId, nodeId, networkId);
#if defined(USE_SERIAL)
    Serial.println(ok ? "Radio init OK" : "Radio init failed");
#endif
    if (ok)
    {
        enableRadio = true;
        uint32_t freq;
        bool haveFreq = radioConfiguration.FrequencyKHz(freq);
        radio.setHighPower(); // Always use this for RFM69HCW
        // Turn on encryption if desired:
        if (ENCRYPT)
            radio.encrypt(radioConfiguration.EncryptionKey());
        if (haveFreq)
        {
            auto setF = 1000 * freq;
            radio.setFrequency(setF);
        }
#if defined(USE_SERIAL)
        if (haveFreq)
            {Serial.print("Radio Config Freq="); Serial.println(freq);}
        Serial.print("Freq= "); Serial.print(radio.getFrequency() / 1000); Serial.println(" KHz");
#endif   
        EEPROM.get(GATEWAY_NODEID_OFFSET, GatewayNodeId);
        if (GatewayNodeId == 0xFFu)
            GatewayNodeId = 1;
        EEPROM.get(D4PWMCOUNT, D4PwmCount);
#if defined(USE_SERIAL)
        Serial.print("Gateway Node ID:");
        Serial.println(static_cast<unsigned>(GatewayNodeId));
#endif
    }

#endif

#if defined(TELEMETER_BATTERY_V)
    ResetAnalogReference();
#endif

    digitalWrite(TIMER_RC_GROUND_PIN, LOW);
    pinMode(TIMER_RC_GROUND_PIN, OUTPUT);

    TimeOfWakeup = millis(); // start loop timer now

    unsigned eepromLoopCount(0);
    EEPROM.get(SLEEP_TIMER_COUNT_OFFSET, eepromLoopCount);
    if (eepromLoopCount && eepromLoopCount <= MAX_SLEEP_LOOP_COUNT)
        SleepLoopTimerCount = eepromLoopCount;

#if defined(USE_SERIAL)
    Serial.print("SleepLoopTimerCount = ");
    Serial.println(SleepLoopTimerCount);
#if TIMER_INIT_STYLE >= TIMER_INIT_STYLE_REV07
    Serial.print("D4 PWM Count:");
    Serial.println(static_cast<unsigned>(D4PwmCount));
#endif
#endif

    Wire.end();
}

/* Power management:
 * For ListenAfterTransmitMsec we stay awake and listen on the radio and Serial.
 * Then we power down all: sensor, radio and CPU and CPU
 * sleep using SleepTilNextSample.
 */

namespace {
    unsigned SleepTilNextSample();

    unsigned long ListenAfterTransmitMsec = FirstListenAfterTransmitMsec;
    unsigned int sampleCount;

    bool processCommand(const char* pCmd)
    {
        static const char SET_LOOPCOUNT[] = "SetDelayLoopCount";
        static const char SET_GATEWAY[] = "SetGatewayNodeId";
        static const char SET_D4PWMCOUNT[] = "SetD4PwmCount";
        if (strncmp(pCmd, SET_LOOPCOUNT, sizeof(SET_LOOPCOUNT) - 1) == 0)
        {
            pCmd = RadioConfiguration::SkipWhiteSpace(
                pCmd + sizeof(SET_LOOPCOUNT) - 1);
            if (pCmd)
            {
                unsigned v = RadioConfiguration::toDecimalUnsigned(pCmd);
                // don't allow zero, nor more than MAX_SLEEP_LOOP_COUNT
                if (v && v < MAX_SLEEP_LOOP_COUNT)
                {
                    SleepLoopTimerCount = v;
                    EEPROM.put(SLEEP_TIMER_COUNT_OFFSET, SleepLoopTimerCount);
                    return true;
                }
            }
        }
        else if (strncmp(pCmd, SET_GATEWAY, sizeof(SET_GATEWAY) - 1) == 0)
        {
            pCmd = RadioConfiguration::SkipWhiteSpace(
                pCmd + sizeof(SET_GATEWAY) - 1);
            if (pCmd)
            {
                GatewayNodeId = static_cast<uint8_t>(RadioConfiguration::toDecimalUnsigned(pCmd));
                EEPROM.put(GATEWAY_NODEID_OFFSET, GatewayNodeId);
                return true;
            }
        }
        else if (strncmp(pCmd, SET_D4PWMCOUNT, sizeof(SET_D4PWMCOUNT) - 1) == 0)
        {
            pCmd = RadioConfiguration::SkipWhiteSpace(
                pCmd + sizeof(SET_D4PWMCOUNT) - 1);
            if (pCmd)
            {
                D4PwmCount = static_cast<uint8_t>(RadioConfiguration::toDecimalUnsigned(pCmd));
                EEPROM.put(D4PWMCOUNT, D4PwmCount);
                return true;
            }
        }        
        return false;
    }
}

void loop()
{
    unsigned long now = millis();

#if defined(USE_SERIAL)
    // Set up a "buffer" for characters that we'll send:
    static char sendbuffer[62];
    static int sendlength = 0;
    // In this section, we'll gather serial characters and
    // send them to the other node if we (1) get a carriage return,
    // or (2) the buffer is full (61 characters).

    // If there is any serial input, add it to the buffer:

    if (Serial.available() > 0)
    {
        TimeOfWakeup = now; // extend timer while we hear something
        char input = Serial.read();

        if (input != '\r') // not a carriage return
        {
            sendbuffer[sendlength] = input;
            sendlength++;
        }

        // If the input is a carriage return, or the buffer is full:
        bool eol = (input == '\r') || (input == '\n');

        if (eol || (sendlength == sizeof(sendbuffer) - 1)) // CR or buffer full
        {
            sendbuffer[sendlength] = 0;
            if (processCommand(sendbuffer))
            {
                Serial.print(sendbuffer);
                Serial.println(" command accepted for thermometer");
            }
            else if (radioConfiguration.ApplyCommand(sendbuffer))
            {
                Serial.print(sendbuffer);
                Serial.println(" command accepted for radio");
            }
            Serial.println("ready");
            sendlength = 0; // reset the packet
        }
    }
#endif

#if defined(USE_RFM69) 
    // RECEIVING
    // In this section, we'll check with the RFM69HCW to see
    // if it has received any packets:

    if (enableRadio && radio.receiveDone()) // Got one!
    {
        // Print out the information:
        TimeOfWakeup = now; // extend sleep timer
#if defined(USE_SERIAL)
        Serial.print("received from node ");
        Serial.print(radio.SENDERID, DEC);
        Serial.print(", message [");

        // The actual message is contained in the DATA array,
        // and is DATALEN bytes in size:

        for (byte i = 0; i < radio.DATALEN; i++)
            Serial.print((char)radio.DATA[i]);

        // RSSI is the "Receive Signal Strength Indicator",
        // smaller numbers mean higher power.

        Serial.print("], RSSI ");
        Serial.println(radio.RSSI);
#endif
        // RFM69 ensures trailing zero byte, unless buffer is full...so
        radio.DATA[sizeof(radio.DATA) - 1] = 0; // ...if buffer is full, ignore last byte
        if (processCommand((const char*)&radio.DATA[0]))
        {
#if defined(USE_SERIAL)
            Serial.println("Received command accepted");
#endif
        }
        if (radio.ACKRequested())
        {
            radio.sendACK();
#if defined(USE_SERIAL)
            Serial.println("ACK sent");
#endif
        }
    }
#endif

    static bool SampledSinceSleep = false;
    if (!SampledSinceSleep)
    {
        static char buf[64];
        SampledSinceSleep = true;
        int batt(0);
#if defined(TELEMETER_BATTERY_V)
        // 10K to VCC and (wired on board) 2.7K to ground
        pinMode(BATTERY_PIN, INPUT_PULLUP); // sample the battery
        batt = analogRead(BATTERY_PIN);
        pinMode(BATTERY_PIN, INPUT); // turn off battery drain
#endif

        Wire.begin();

#if (defined(USE_TMP102) || defined(USE_TMP175)) && !defined(USE_SI7021)
        {
#if defined(USE_TMP102)
            auto temperature256 = tmp102.finishReadTempCx256();
#elif defined(USE_TMP175)
            auto temperature256 = tmp175.finishReadTempCx256();
#endif
            char sign = '+';
            if (temperature256 < 0) {
                temperature256 = -temperature256;
                sign = '-';
            }
            else if (temperature256 == 0)
                sign = ' ';

            int whole = temperature256 >> 8;
            if (whole >= static_cast<int>(MAX_VALID_TEMPERATURE_C))
                sign = sign == '-' ? '<' : '>';
            int frac = temperature256 & 0xFF;
            frac *= 100;
            frac >>= 7; // range of 0 through 199
            frac += 5; // round up (away from zero)
            if (frac >= 200)
            {
                whole += 1; // carry
                frac = 0;
            }
            frac >>= 1;
            sprintf(buf, "C:%u, B:%d, T:%c%d.%02d", sampleCount++,
                batt,
                sign, whole, frac);
#if defined(USE_SERIAL)
            Serial.println(buf);
#endif
#if defined(USE_RFM69) 
            if (enableRadio)
                radio.sendWithRetry(GatewayNodeId, buf, strlen(buf));
#endif
        }
#endif
#if defined(USE_HIH6130) || defined(USE_SI7021)
        {
            float humidity(0), temperature(0);
#if defined(USE_HIH6130)
            sensor0.begin();
            // read temperature data
            unsigned char stat = sensor0.GetReadings(humidity, temperature);
            sensor0.end();
            if (stat != 0)
            {
                SampledSinceSleep = false;
                Wire.end();
                return;
            }
#elif defined(USE_SI7021)
#if defined(USE_TMP102)
            // The temperature readout from the Si7021 only has about 1 degree F resolution
            // If a TMP102 or TMP175 is also installed and compiled, use its temperature
            temperature = tmp102.finishReadTempCx256() / 256.f;
#elif defined(USE_TMP175)
            temperature = tmp175.finishReadTempCx256() / 256.f;
#else
            si7021.startReadTemperature();
            temperature = si7021.readTemperature();
#endif
            si7021.startReadHumidity();
            humidity = si7021.readHumidity();
#endif

            char sign = '+';
            if (temperature < 0.f) {
                temperature = -temperature;
                if (temperature < MAX_VALID_TEMPERATURE_C)
                    sign = '-';
                else
                    sign = '<';
            }
            else if (temperature == 0.f)
                sign = ' ';
            else if (temperature >= MAX_VALID_TEMPERATURE_C)
                sign = '>';

            int whole = (int)temperature;
            int wholeRh = (int)humidity;

            sprintf(buf, "C:%u, B:%d, T:%c%d.%02d R:%d.%02d", sampleCount++,
                batt,
                sign, whole,
                (int)(100.f * (temperature - whole)),
                wholeRh,
                (int)(100.f * (humidity - wholeRh)));
#if defined(USE_SERIAL)
            Serial.println(buf);
#endif
#if defined(USE_RFM69) 
            if (enableRadio)
                radio.sendWithRetry(GatewayNodeId, buf, strlen(buf));
#endif
        }
#endif
        Wire.end();
    }

    if (now - TimeOfWakeup > ListenAfterTransmitMsec)
    {
        SleepTilNextSample();
        SampledSinceSleep = false;
        TimeOfWakeup = millis();
        ListenAfterTransmitMsec = NormalListenAfterTransmit;
#if defined(USE_TMP102)
        tmp102.startReadTemperature();
#endif
#if defined(USE_TMP175)
        tmp175.startReadTemperature();
#endif
#if defined(USE_SI7021)
#endif
    }
}

#if !defined(SLEEP_WITH_TIMER2)
void sleepPinInterrupt()	// requires 1uF parallel 10M between two pins
{
    detachInterrupt(digitalPinToInterrupt(TIMER_RC_PIN));
}
#else
ISR(TIMER2_OVF_vect) {} // do nothing but wake up
#endif

namespace {
    unsigned SleepTilNextSample()
    {
#if defined(USE_SERIAL)
        Serial.print("sleep for ");
        Serial.println(SleepLoopTimerCount);
        Serial.flush();
        Serial.end();// wait for finish and turn off pins before sleep
        pinMode(0, INPUT); // Arduino libraries have a symbolic definition for Serial pins?
        pinMode(1, INPUT);
#endif

#if defined(USE_RFM69) 
        if (enableRadio)
            radio.sleep();
#endif

#if defined(TELEMETER_BATTERY_V)
        ADCSRA = 0; // Turn off ADC
#endif

        // sleep mode power supply current measurements indicate this appears to be redundant
        power_all_disable(); // turn off everything

        unsigned count = 0;

#if !defined(SLEEP_WITH_TIMER2)
        // this requires 1uF and 10M in parallel to trigger INT1 on pin 3
        while (count < SleepLoopTimerCount)
        {
            power_timer0_enable(); // delay() requires this
            // determined empirically using BatteryExtenderTest
            static const int TIMER_INIT_CHARGE_MSEC = 20; //AA cell internal resistance affects this
#if (TIMER_INIT_STYLE != TIMER_INIT_STYLE_REV06) 
            pinMode(TIMER_RC_PIN, OUTPUT);
            digitalWrite(TIMER_RC_PIN, HIGH);
            for (uint8_t i = 0; ; i++)
            {
                digitalWrite(TIMER_RC_GROUND_PIN, LOW);
                delay(TIMER_INIT_CHARGE_MSEC);
#if TIMER_INIT_STYLE >= TIMER_INIT_STYLE_REV07 // only the charge pump needs to be cycled
                if (i == D4PwmCount)
                    break;
                digitalWrite(TIMER_RC_GROUND_PIN, HIGH);
                delay(TIMER_INIT_CHARGE_MSEC);
#else
                break;
#endif
            }
#else
            pinMode(TIMER_RC_GROUND_PIN, OUTPUT);
            digitalWrite(TIMER_RC_GROUND_PIN, LOW);
            delay(TIMER_INIT_CHARGE_MSEC); // Charge the C
            pinMode(TIMER_RC_GROUND_PIN, INPUT);
#endif
            pinMode(TIMER_RC_PIN, INPUT);
            cli();
            power_timer0_disable(); // timer0 powered down again
            attachInterrupt(digitalPinToInterrupt(TIMER_RC_PIN), sleepPinInterrupt, LOW);
            set_sleep_mode(SLEEP_MODE_PWR_DOWN);
            sleep_enable();
            sleep_bod_disable();
            sei();
            sleep_cpu(); // Power supply measured: About 250uA rising to about 560 uA on REV05 and lower
            sleep_disable();
            sei();
            count += 1;
        }

#else
        // this section requires no external hardware, AND has a predictable
        // sleep time. BUT takes an extra 100 to 200 uA of sleep current
        power_timer2_enable(); // need this one timer
        clock_prescale_set(clock_div_256); // slow CPU clock down by 256
        while (count < SleepLoopTimerCount)
        {
            cli();
            // This code inspired by:
            // http://donalmorrissey.blogspot.com/2011/11/sleeping-arduino-part-4-wake-up-via.html
            // ...except that we use TIMER2 instead of TIMER1 so we can go to SLEEP_MODE_PWR_SAVE
            // instead of SLEEP_MODE_IDLE
            //
            // We use clock_prescale_set here (not in the link above) to make up for the fact
            // that TIMER2 is only 8 bits wide (TIMER1 is 16)
            //

            /* Normal timer operation.*/
            TCCR2A = 0x00;

            /* Clear the timer counter register.
             * You can pre-load this register with a value in order to
             * reduce the timeout period, say if you wanted to wake up
             * ever 4.0 seconds exactly.
             */
            TCNT2 = 0x0000;

            /* Configure the prescaler for 1:1024, giving us a
             * timeout of 1024 * 256 / 8000000 = 32.768 msec
             */
            TCCR2B = 0x07; // 1024 prescale

            /* Enable the timer overlow interrupt. */
            TIMSK2 = 0x01;
            TIFR2 = 1; // Clear any current overflow flag

            set_sleep_mode(SLEEP_MODE_PWR_SAVE);
            sleep_enable();
            sleep_bod_disable();
            sei();
            sleep_cpu();	// 280 uA -- steady without RFM69
            sleep_disable();
            sei();
            count += 1;
        }
        clock_prescale_set(clock_div_1);
#endif

        power_all_enable();

#if defined(TELEMETER_BATTERY_V)
        ResetAnalogReference();
#endif

#if defined(USE_RFM69) 
        if (enableRadio)
            radio.setMode(RF69_MODE_STANDBY);
#endif

#if defined(USE_SERIAL)
        Serial.begin(9600);
        Serial.print(count, DEC);
        Serial.println(" wakeup");
#endif

        return count;
    }

#if defined(TELEMETER_BATTERY_V)
    void ResetAnalogReference()
    {
        analogReference(INTERNAL);
        pinMode(BATTERY_PIN, INPUT);
        analogRead(BATTERY_PIN);
        delay(10); // let ADC settle
    }
#endif
}
