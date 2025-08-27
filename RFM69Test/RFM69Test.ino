#include <RFM69.h>

    const int RFM69_RESET_PIN = 9;
    const int RFM69_CHIP_SELECT_PIN = 10;
    const int RFM69_INT_PIN = 2;
 
    // Create a library object for our RFM69HCW module:
    RFM69 radio(RFM69_CHIP_SELECT_PIN, RFM69_INT_PIN, true);

    static bool radioOK = false;

void setup()
{
    pinMode(RFM69_CHIP_SELECT_PIN, OUTPUT);
    digitalWrite(RFM69_CHIP_SELECT_PIN, HIGH);
    Serial.begin(9600);
    Serial.println("RFM69 Test");
    SPI.begin();

    radioOK = radio.initialize(RF69_915MHZ, 127, 1);
    Serial.print("radio.initialize: "); 
    Serial.println(static_cast<int>(radioOK));

    if (radioOK)
    {
        auto f = radio.getFrequency();
        Serial.print("default frequency: ");
        Serial.println(f);

        f = 920000000;
        radio.setFrequency(f);
        Serial.println("freq after set to 920000KHz");
        Serial.println(radio.getFrequency());

        //radio.readAllRegs();
    }

}

void loop()
{
    auto now = millis();
    static auto testStartTime = now;
    static enum class State_t {IDLE, SCOPE_LOOP} state = State_t::IDLE;

    if (radioOK)
    {
        static bool printOnce;
        if (Serial.available() > 0)
        {
            char input = Serial.read();
            switch (input)
            {
                case 'T':
                case 't':
                    if (state == State_t::IDLE)
                    {
                        state = State_t::SCOPE_LOOP;
                        Serial.println("testing");
                    }
                    break;
                case 'C':
                case 'c':
                    if (state == State_t::SCOPE_LOOP)
                    {
                        state = State_t::IDLE;
                        Serial.println("Test stopped");
                    }
                    break;
                case 'p':
                case 'P':
                    if (state == State_t::SCOPE_LOOP)
                        printOnce = true;
                    break;
                default:
                    if (!isspace(input))
                        Serial.println('?');
                    break;
            }
        }

        switch (state)
        {
            case State_t::SCOPE_LOOP:
                static const int LOOP_INTERVAL_MSEC = 20;
                if (now - testStartTime >= LOOP_INTERVAL_MSEC)
                {
                    testStartTime = now;
                    radio.setFrequency(920000000);
                    auto f = radio.getFrequency();
                    if (printOnce)
                    {
                        Serial.print("Set: 920000000 Read:");
                        Serial.println(f);
                        printOnce = false;
                    }
                }
                break;
            case State_t::IDLE:
            default:
                break;
        }
    }
}