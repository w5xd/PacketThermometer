#include <RFM69.h>

    const int RFM69_RESET_PIN = 9;
    const int RFM69_CHIP_SELECT_PIN = 10;
    const int RFM69_INT_PIN = 2;
 
    // Create a library object for our RFM69HCW module:
    RFM69 radio(RFM69_CHIP_SELECT_PIN, RFM69_INT_PIN, true);

void setup()
{
    Serial.begin(9600);
    Serial.println("RFM69 Test");
    pinMode(RFM69_CHIP_SELECT_PIN, OUTPUT);
    digitalWrite(RFM69_CHIP_SELECT_PIN, HIGH);
    SPI.begin();

    auto ok = radio.initialize(RF69_915MHZ, 127, 1);
    Serial.print("radio.initialize: "); 
    Serial.println(static_cast<int>(ok));

    if (ok)
    {
    auto f = radio.getFrequency();
    Serial.print("default frequency: ");
    Serial.println(f);

    f = 915000000;
    radio.setFrequency(f);
    Serial.print("freq after set to 915000KHz");
    Serial.println(radio.getFrequency());
    }

}

void loop()
{
}