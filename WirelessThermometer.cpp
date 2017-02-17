#include "WirelessThermometer.h"
#include <RadioConfiguration.h>
#include <SPI.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/power.h>
// RFM69HCW Example Sketch
// Send serial input characters from one RFM69 node to another
// Based on RFM69 library sample code by Felix Rusu
// http://LowPowerLab.com/contact
// Modified for RFM69HCW by Mike Grusin, 4/16

// This sketch will show you the basics of using an
// RFM69HCW radio module. SparkFun's part numbers are:
// 915MHz: https://www.sparkfun.com/products/12775
// 434MHz: https://www.sparkfun.com/products/12823

// See the hook-up guide for wiring instructions:
// https://learn.sparkfun.com/tutorials/rfm69hcw-hookup-guide

// Uses the RFM69 library by Felix Rusu, LowPowerLab.com
// Original library: https://www.github.com/lowpowerlab/rfm69
// SparkFun repository: https://github.com/sparkfun/RFM69HCW_Breakout

// Include the RFM69 and SPI libraries:
#define USE_TMP102
//#define SLEEP_TMP102_ONLY
#define USE_RFM69
//#define SLEEP_RFM69_ONLY
#define USE_SERIAL
#define TELEMETER_BATTERY_V

#if defined(USE_RFM69)
#include <RFM69.h>
#include <RFM69registers.h>
#endif
#if defined(USE_TMP102)
#include <Wire.h>
#include <TMP102Helper.h> // Used to send and receive specific information from our sensor
#endif

const int BATTERY_PIN = A0; // digitize (fraction of) battery voltage
const int TIMER_RC_GROUND_PIN = 4;
const int TIMER_RC_PIN = 3;

const unsigned long FirstListenAfterTransmitMsec = 20000;// at system reset, listen for this long
const unsigned long NormalListenAfterTransmit = 300;// only this long to send us a message after we send
const unsigned int Loop10sRCtimerCount = 30; // approx 10 seconds per Count

#if defined(USE_TMP102)
// Connections to TMP102
// VCC = 3.3V
// GND = GND
// SDA = A4
// SCL = A5
const int ALERT_PIN = A3;

HomeAutomationTools::TMP102 sensor0(0x48); // Initialize sensor at I2C address 0x48
// Sensor address can be changed with an external jumper to:
// ADD0 - Address
//  VCC - 0x49
//  SDA - 0x4A
//  SCL - 0x4B
#endif

#define TONODEID      1   // Destination node ID

#if defined(USE_RFM69)
// RFM69 frequency, uncomment the frequency of your module:

//#define FREQUENCY   RF69_433MHZ
#define FREQUENCY     RF69_915MHZ

// AES encryption (or not):
static const bool ENCRYPT = true; // Set to "true" to use encryption
// Use ACKnowledge when sending messages (or not):
static const bool USEACK = true; // Request ACKs or not
static const int RFM69_RESET_PIN = A1;
static RadioConfiguration radioConfiguration;

class SleepRFM69 : public RFM69
{
public:
	void startAsleep()
	{
	  digitalWrite(_slaveSelectPin, HIGH);
	  pinMode(_slaveSelectPin, OUTPUT);
	  SPI.begin();
	  SPIoff();
	}
	void SPIoff()
	{
		sleep();
        SPI.end();
	    pinMode(PIN_SPI_MISO, INPUT);
	    pinMode(PIN_SPI_MOSI, INPUT);
	    pinMode(PIN_SPI_SCK, INPUT);
	    pinMode(PIN_SPI_SS, INPUT);
	    pinMode(_slaveSelectPin, INPUT);
	}
	void SPIon()
	{
	  digitalWrite(_slaveSelectPin, HIGH);
	  pinMode(_slaveSelectPin, OUTPUT);
	  SPI.begin();
	}
};
// Create a library object for our RFM69HCW module:
SleepRFM69 radio;
#endif

static unsigned long TimeOfWakeup;

void setup()
{
#if defined(USE_SERIAL)
    // Open a serial port so we can send keystrokes to the module:

    Serial.begin(9600);
    Serial.print("Node ");
    Serial.print(radioConfiguration.NodeId(), DEC);
    Serial.print(" on network ");
    Serial.print(radioConfiguration.NetworkId(), DEC);
    Serial.println(" ready");
#endif

#if defined(USE_TMP102)
    pinMode(ALERT_PIN, INPUT);  // Declare alertPin as an input
    sensor0.begin();  // Join I2C bus
    // Initialize sensor0 settings
    sensor0.setOneShotMode(); // set low power mode

    // These settings are saved in the sensor, even if it loses power

    // set the number of consecutive faults before triggering alarm.
    // 0-3: 0:1 fault, 1:2 faults, 2:4 faults, 3:6 faults.
    sensor0.setFault(0);  // Trigger alarm immediately

    // set the polarity of the Alarm. (0:Active LOW, 1:Active HIGH).
    sensor0.setAlertPolarity(0); // Active LOW

    // set the sensor in Comparator Mode (0) or Interrupt Mode (1).
    sensor0.setAlertMode(0); // Comparator Mode.

    // set the Conversion Rate (how quickly the sensor gets a new reading)
    //0-3: 0:0.25Hz, 1:1Hz, 2:4Hz, 3:8Hz
    sensor0.setConversionRate(2);

    //set Extended Mode.
    //0:12-bit Temperature(-55C to +128C) 1:13-bit Temperature(-55C to +150C)
    sensor0.setExtendedMode(0);

    //set T_HIGH, the upper limit to trigger the alert on
    sensor0.setHighTempC(127); // set T_HIGH in C

    //set T_LOW, the lower limit to shut turn off the alert
    sensor0.setLowTempC(127); // set T_LOW in C

    sensor0.end();
#endif

#if defined(USE_RFM69)
    //digitalWrite(RFM69_RESET_PIN, LOW);
    //pinMode(RFM69_RESET_PIN, OUTPUT);

#if !defined(SLEEP_RFM69_ONLY)
    // Initialize the RFM69HCW:
    radio.initialize(radioConfiguration.FrequencyBandId(),
        radioConfiguration.NodeId(), radioConfiguration.NetworkId());

    radio.setHighPower(); // Always use this for RFM69HCW
    // Turn on encryption if desired:

    if (ENCRYPT)
        radio.encrypt(radioConfiguration.EncryptionKey());
#else
    radio.startAsleep();
#endif

#endif

#if defined(TELEMETER_BATTERY_V)
    analogReference(INTERNAL);
    pinMode(BATTERY_PIN, INPUT);
    analogRead(BATTERY_PIN);
    delay(10); // let ADC settle
#endif

    digitalWrite(TIMER_RC_GROUND_PIN, LOW);
    pinMode(TIMER_RC_GROUND_PIN, OUTPUT);

    TimeOfWakeup = millis();
}

unsigned long ListenAfterTransmitMsec = FirstListenAfterTransmitMsec;
unsigned int sampleCount;

/* Power management:
 * Every SleepBetweenSamplesMsec we wake up and send a sample.
 * For ListenAfterTransmitMsec we stay awake and listen on the radio.
 * Then we power down all: temperature sensor, radio and CPU
 * We use the Atmega "Power Save" mode with Timer/Counter2 running to wake us
 * up after SleepBetweenSamplesMsec
 *
 */

static unsigned SleepTilNextSample();

void loop()
{
    unsigned long now = millis();
    // Set up a "buffer" for characters that we'll send:
    static char sendbuffer[62];
    static int sendlength = 0;

    // SENDING
#if defined(USE_SERIAL)
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

        if ((input == '\r') || (sendlength == 61)) // CR or buffer full
        {
            sendbuffer[sendlength] = 0;
            if (radioConfiguration.ApplyCommand(sendbuffer))
            {
                Serial.print(sendbuffer);
                Serial.println(" command accepted");
            }
            else
            {            // Send the packet!
                Serial.print("sending to node ");
                Serial.print(TONODEID, DEC);
                Serial.print(", message [");
                for (byte i = 0; i < sendlength; i++)
                    Serial.print(sendbuffer[i]);
                Serial.println("]");

#if defined(USE_RFM69) && !defined(SLEEP_RFM69_ONLY)
                // There are two ways to send packets. If you want
                // acknowledgements, use sendWithRetry():

                if (USEACK)
                {
                    if (radio.sendWithRetry(TONODEID, sendbuffer, sendlength))
                        Serial.println("ACK received!");
                    else
                        Serial.println("no ACK received");
                }
                // If you don't need acknowledgements, just use send():
                else // don't use ACK
                {
                    radio.send(TONODEID, sendbuffer, sendlength);
                }
#endif
            }
            sendlength = 0; // reset the packet
        }
    }
#endif

#if defined(USE_RFM69) && !defined(SLEEP_RFM69_ONLY)
    // RECEIVING

    // In this section, we'll check with the RFM69HCW to see
    // if it has received any packets:

    if (radio.receiveDone()) // Got one!
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
        // Send an ACK if requested.
        // (You don't need this code if you're not using ACKs.)

        if (radio.ACKRequested())
        {
            radio.sendACK();
            Serial.println("ACK sent");
        }
    }
#endif

    static bool SampledSinceSleep = false;
    if (!SampledSinceSleep)
    {
        SampledSinceSleep = true;
#if defined(USE_TMP102) && !defined(SLEEP_TMP102_ONLY)

        float temperature(0);

        // Turn sensor on to start temperature measurement.
        // Current consumtion typically ~10uA.
        sensor0.begin();

        // read temperature data
        temperature = sensor0.readTempCfromShutdown();

#if 0	// not using the alertPin
        boolean alertPinState, alertRegisterState;
        // Check for Alert
        alertPinState = digitalRead(ALERT_PIN); // read the Alert from pin
        alertRegisterState = sensor0.alert();   // read the Alert from register
#endif
        sensor0.end();

#if defined(USE_SERIAL)
        // Print temperature and alarm state
        Serial.print("Temperature: ");
        Serial.println(temperature);

#if 0
        Serial.print("\tAlert Pin: ");
        Serial.print(alertPinState);

        Serial.print("\tAlert Register: ");
        Serial.println(alertRegisterState);
#endif
#endif

        int batt(0);
#if defined(TELEMETER_BATTERY_V)
        pinMode(BATTERY_PIN, INPUT_PULLUP); // sample the battery
        batt = analogRead(BATTERY_PIN);
        pinMode(BATTERY_PIN, INPUT); // turn off battery drain
#endif
        char sign = '+';
        static char buf[64];
        if (temperature < 0.f){
            temperature = -temperature;
            sign = '-';
        }
        else if (temperature == 0.f)
            sign = ' ';

        int whole = (int)temperature;

        sprintf(buf, "Count: %d, Battery: %d, temp: %c%d.%02d", sampleCount++,
            batt,
            sign, whole,
            (int)(100.f * (temperature - whole)));
#if defined(USE_SERIAL)
        Serial.println(buf);
#endif
#endif
#if defined(USE_RFM69) && !defined(SLEEP_RFM69_ONLY)
        radio.sendWithRetry(TONODEID, buf, strlen(buf));
#endif
    }

    if (now - TimeOfWakeup > ListenAfterTransmitMsec)
    {
        SleepTilNextSample();
        SampledSinceSleep = false;
        TimeOfWakeup = millis();
        ListenAfterTransmitMsec = NormalListenAfterTransmit;
    }
}


#if !defined(SLEEP_WITH_TIMER2)
void sleepPinInterrupt()	// requires 1uF and 10M between two pins
{
    detachInterrupt(digitalPinToInterrupt(TIMER_RC_PIN));
}
#else
ISR(TIMER2_OVF_vect) {} // do nothing but wake up
#endif

unsigned SleepTilNextSample()
{
#if defined(USE_SERIAL)
    Serial.println("sleep");
#endif

#if defined(USE_RFM69) && !defined(SLEEP_RFM69_ONLY)
     radio.SPIoff();
 #endif

#if defined(USE_SERIAL)
    Serial.end();// wait for finish and turn off pins before sleep
    pinMode(0, INPUT);
    pinMode(1, INPUT);
#endif

    unsigned count = 0;
    power_all_disable(); // turn off everything

#if !defined(SLEEP_WITH_TIMER2) // this requires 1uF and 10M in parallel between pins 3 & 4
    while (count < Loop10sRCtimerCount)
    {
        power_timer0_enable(); // delay() requires this
        pinMode(TIMER_RC_PIN, OUTPUT);
        digitalWrite(TIMER_RC_PIN, HIGH);
        delay(10); // Charge the 1uF
        cli();
        power_timer0_disable(); // now back off
        attachInterrupt(digitalPinToInterrupt(TIMER_RC_PIN), sleepPinInterrupt, LOW);
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        pinMode(TIMER_RC_PIN, INPUT);
        sleep_enable();
        sleep_bod_disable();
        sei();
        sleep_cpu(); // about 500uA, average. About 400uA and rises as cap discharges
        sleep_disable();
        sei();
        count += 1;
    }
#else
    power_timer2_enable(); // need this one timer
    clock_prescale_set(clock_div_256); // slow CPU clock down by 256
    while (count < Loop10sRCtimerCount)
    {
        cli();
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
        sleep_cpu();	// 600 uA -- steady
        sleep_disable();
        sei();
        count += 1;
    }
    clock_prescale_set(clock_div_1);
#endif
    power_all_enable();

#if defined(USE_SERIAL)
    Serial.begin(9600);
    Serial.print(count, DEC);
    Serial.println(" wakeup");
#endif

#if defined(USE_RFM69) && !defined(SLEEP_RFM69_ONLY)
    radio.SPIon();
#endif

    return count;
}

