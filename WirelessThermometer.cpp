#include "WirelessThermometer.h"
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

#include <RFM69.h>
#include <SPI.h>
#include <Wire.h>
#include <SparkFunTMP102.h> // Used to send and recieve specific information from our sensor

// Connections
// VCC = 3.3V
// GND = GND
// SDA = A4
// SCL = A5
const int ALERT_PIN = A3;
const int BATTERY_PIN = A0;

TMP102 sensor0(0x48); // Initialize sensor at I2C address 0x48
// Sensor address can be changed with an external jumper to:
// ADD0 - Address
//  VCC - 0x49
//  SDA - 0x4A
//  SCL - 0x4B
// Addresses for this node. CHANGE THESE FOR EACH NODE!

#define NETWORKID     0   // Must be the same for all nodes
#define MYNODEID      2   // My node ID
#define TONODEID      1   // Destination node ID

// RFM69 frequency, uncomment the frequency of your module:

//#define FREQUENCY   RF69_433MHZ
#define FREQUENCY     RF69_915MHZ

// AES encryption (or not):

#define ENCRYPT       true // Set to "true" to use encryption
#define ENCRYPTKEY    "TOPSECRETPASSWRD" // Use the same 16-byte key on all nodes

// Use ACKnowledge when sending messages (or not):

#define USEACK        true // Request ACKs or not

// Packet sent/received indicator LED (optional):

#if 0
#define LED           9 // LED positive pin
#define GND           8 // LED ground pin
#endif

// Create a library object for our RFM69HCW module:

RFM69 radio;


void setup()
{
  // Open a serial port so we can send keystrokes to the module:

  Serial.begin(9600);
  Serial.print("Node ");
  Serial.print(MYNODEID,DEC);
  Serial.println(" ready");

  pinMode(ALERT_PIN,INPUT);  // Declare alertPin as an input
   sensor0.begin();  // Join I2C bus

   // Initialize sensor0 settings
   // These settings are saved in the sensor, even if it loses power

   // set the number of consecutive faults before triggering alarm.
   // 0-3: 0:1 fault, 1:2 faults, 2:4 faults, 3:6 faults.
   sensor0.setFault(0);  // Trigger alarm immediately

   // set the polarity of the Alarm. (0:Active LOW, 1:Active HIGH).
   sensor0.setAlertPolarity(1); // Active HIGH

   // set the sensor in Comparator Mode (0) or Interrupt Mode (1).
   sensor0.setAlertMode(0); // Comparator Mode.

   // set the Conversion Rate (how quickly the sensor gets a new reading)
   //0-3: 0:0.25Hz, 1:1Hz, 2:4Hz, 3:8Hz
   sensor0.setConversionRate(2);

   //set Extended Mode.
   //0:12-bit Temperature(-55C to +128C) 1:13-bit Temperature(-55C to +150C)
   sensor0.setExtendedMode(0);

   //set T_HIGH, the upper limit to trigger the alert on
   sensor0.setHighTempF(85.0);  // set T_HIGH in F
   //sensor0.setHighTempC(29.4); // set T_HIGH in C

   //set T_LOW, the lower limit to shut turn off the alert
   sensor0.setLowTempF(84.0);  // set T_LOW in F
   //sensor0.setLowTempC(26.67); // set T_LOW in C
  // Initialize the RFM69HCW:

  radio.initialize(FREQUENCY, MYNODEID, NETWORKID);
  radio.setHighPower(); // Always use this for RFM69HCW

  // Turn on encryption if desired:

  if (ENCRYPT)
    radio.encrypt(ENCRYPTKEY);
  analogReference(INTERNAL);
  pinMode(BATTERY_PIN, INPUT_PULLUP);
}

void loop()
{
  // Set up a "buffer" for characters that we'll send:

  static char sendbuffer[62];
  static int sendlength = 0;

  // SENDING

  // In this section, we'll gather serial characters and
  // send them to the other node if we (1) get a carriage return,
  // or (2) the buffer is full (61 characters).

  // If there is any serial input, add it to the buffer:

  if (Serial.available() > 0)
  {
    char input = Serial.read();

    if (input != '\r') // not a carriage return
    {
      sendbuffer[sendlength] = input;
      sendlength++;
    }

    // If the input is a carriage return, or the buffer is full:

    if ((input == '\r') || (sendlength == 61)) // CR or buffer full
    {
      // Send the packet!


      Serial.print("sending to node ");
      Serial.print(TONODEID, DEC);
      Serial.print(", message [");
      for (byte i = 0; i < sendlength; i++)
        Serial.print(sendbuffer[i]);
      Serial.println("]");

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

      sendlength = 0; // reset the packet
    }
  }

  // RECEIVING

  // In this section, we'll check with the RFM69HCW to see
  // if it has received any packets:

  if (radio.receiveDone()) // Got one!
  {
    // Print out the information:

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

    // Send an ACK if requested.
    // (You don't need this code if you're not using ACKs.)

    if (radio.ACKRequested())
    {
      radio.sendACK();
      Serial.println("ACK sent");
    }
#if 0
    Blink(LED,10);
#endif
  }

  unsigned long now = millis();
  static unsigned long lastTemp;
  if (now - lastTemp > 10000)
  {
	  lastTemp = now;
  float temperature;
   boolean alertPinState, alertRegisterState;

   // Turn sensor on to start temperature measurement.
   // Current consumtion typically ~10uA.
   sensor0.wakeup();

   // read temperature data
   temperature = sensor0.readTempF();
   //temperature = sensor0.readTempC();

   // Check for Alert
   alertPinState = digitalRead(ALERT_PIN); // read the Alert from pin
   alertRegisterState = sensor0.alert();   // read the Alert from register

   // Place sensor in sleep mode to save power.
   // Current consumtion typically <0.5uA.
   sensor0.sleep();

   // Print temperature and alarm state
   Serial.print("Temperature: ");
   Serial.print(temperature);

   Serial.print("\tAlert Pin: ");
   Serial.print(alertPinState);

   Serial.print("\tAlert Register: ");
   Serial.println(alertRegisterState);

   int batt = analogRead(BATTERY_PIN);
   char sign = '+';
   static char buf[64];
   if (temperature < 0){
	   temperature = -temperature;
 	   sign = '-';
  }
  int whole = (int)temperature;

   sprintf(buf, "Battery: %d, temp: %c%d.%02d", batt,
		   sign, whole,
		   (int)(100.f * (temperature - whole)));
   Serial.println(buf);
   radio.sendWithRetry(TONODEID, buf, strlen(buf));

  }
}

