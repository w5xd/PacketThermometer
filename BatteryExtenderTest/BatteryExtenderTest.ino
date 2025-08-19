/* This sketch is for an Arduino wired to (only) the Battery Extender circuit,
** in order to test that circuit part way through the assembly process. 
** Why then? because the Arduino Mini Pro is soldered directly on top of the Battery Extender
** circuit on the PCB such that the Battery Extender components are inaccessible once the Mini Pro 
** is soldered in
*/

namespace {
    // FWIW: Don't bother running this test program on Teensy. Its input pins do not have the
    // multi-megohm input impedance that the AVR processors have, and is required.
    const int TIMER_RC_GROUND_PIN = 4;
    const int TIMER_RC_PIN = A0; // sleep uProc using RC circuit on this pin
    const int TIMER_RC_INT_PIN = 3; //Pro Mini has no common analog + interrupt pin. must rewire for this
}

void setup()
{
    Serial.begin(9600);
    Serial.println("Battery Extender Test");
    if (TIMER_RC_PIN != TIMER_RC_INT_PIN)
        Serial.println(F(
            "This test sketch is compiled to use a different pin for analogRead versus\r\n"
            "Interrupt. Presumably that is because its running on an Arduino that has\r\n"
            "no pin that can do both. This configuration REQUIRES you move the jumper\r\n"
            "to TIMER_RC_INT_PIN to run the I test\r\n"
            "and to TIMER_RC_PIN to run the G test"
        ));
    Serial.print("gnd="); Serial.println(TIMER_RC_GROUND_PIN);
    Serial.print("analog="); Serial.println(TIMER_RC_PIN);

    digitalWrite(TIMER_RC_GROUND_PIN, LOW);
    pinMode(TIMER_RC_GROUND_PIN, OUTPUT);

}

static uint8_t CYCLES = 0;
static uint8_t PERIOD = 2;
static const float VDD = 3.3f;

static void prAnalog(uint16_t t, uint16_t v)
{
    if (t != -1)
    {
        Serial.print("t=");
        Serial.print(t);
    }
    Serial.print(" V=");
    Serial.print((VDD * v) / 1023u);
    Serial.print(" v=");
    Serial.println(v);
}

static void charge (int TIMER_RC_PIN)
{
        pinMode(TIMER_RC_GROUND_PIN, OUTPUT);
        pinMode(TIMER_RC_PIN, OUTPUT);
        digitalWrite(TIMER_RC_PIN, HIGH);
        for (uint8_t i = 0; ;i++)
        {
            digitalWrite(TIMER_RC_GROUND_PIN, LOW);
            delay(PERIOD);
            if (i == CYCLES)
                break;
            digitalWrite(TIMER_RC_GROUND_PIN, HIGH);
            delay(PERIOD);
          }
        pinMode(TIMER_RC_PIN, INPUT);
}

volatile static bool hello = false;
void sleepPinInterrupt()	// requires 1uF parallel 10M between two pins
{
    hello = true;
}

static bool processCommand(const char *v)
{
    auto beg = millis();
    auto c = *v;
    auto up = c;
    if (isalpha(up))
        up = toupper(c);
    if (up == 'G')
    {
        Serial.println("Cycles Test");
        charge(TIMER_RC_PIN);
        uint16_t prevV = 0xffff;
        auto beg2 = millis();
        Serial.print("charge time="); Serial.println(static_cast<int32_t>(beg2-beg));
        auto prev = beg2-1000;
        typedef unsigned long long tdif_t;
        tdif_t tAbove = static_cast<tdif_t>(-1);
        bool printedDeltaT = false;
        for(;;)
        {
            auto tm = millis();
            if (tm - beg2 > 1000l * 100)
                break;
            uint16_t v = analogRead(TIMER_RC_PIN);
            if (tAbove==static_cast<tdif_t>(-1) && (v < 512+20))
                tAbove = tm;
            if (!printedDeltaT && v < 512-20)
            {
                Serial.print("crossing t=");
                Serial.println(static_cast<uint32_t>(tm - tAbove));
                printedDeltaT = true;
            }
            int16_t vDif = v - prevV;
             if (vDif < 0)
                vDif = -vDif;
            if (vDif < 10)
                continue;
            if (tm - prev < 10)
                continue;
            prev = tm;
            prAnalog(static_cast<uint16_t>(tm-beg2), v);
            prevV = v;
            if (v < 200)
                break;
        }
    }
    else if (isdigit(c))
    {
        CYCLES = 0;
        while (auto c = *v++)
        {
            CYCLES *= 10;
            if (!isdigit(c))
                break;
            CYCLES += (c - '0') ;
        }
        Serial.print("CYCLES="); Serial.println(static_cast<unsigned>(CYCLES));
    }
    else if (up == 'P')
    {
        while (isspace(*++v));
        PERIOD = 0;
        while (auto c = *v++)
        {
            if (!isdigit(c))
                break;
            PERIOD *= 10;
            PERIOD += c - '0';
        }
        Serial.print("PERIOD="); Serial.println(static_cast<unsigned>(PERIOD));
    }
    else if (up == 'I')
    {
        Serial.println("Interrupt test");
        charge(TIMER_RC_INT_PIN);
        auto now = millis();
        hello = false;
        auto pinI = digitalPinToInterrupt(TIMER_RC_INT_PIN);
        Serial.print("pinI="); Serial.println(pinI);
        attachInterrupt(pinI, sleepPinInterrupt, LOW);
        while(!hello);
        Serial.print("int time="); Serial.println(static_cast<int32_t>(millis()-now));
        detachInterrupt(pinI);    
    }
    else if (up == 'A')
    {
        prAnalog(-1, analogRead(TIMER_RC_PIN));
    }
    return true;
}


void loop()
{
    // Set up a "buffer" for characters that we'll send:
    static char sendbuffer[62];
    static int sendlength = 0;
    // In this section, we'll gather serial characters and
    // send them to the other node if we (1) get a carriage return,
    // or (2) the buffer is full (61 characters).

    // If there is any serial input, add it to the buffer:

    if (Serial.available() > 0)
    {
        char input = Serial.read();
        bool eol = (input == '\r') || (input == '\n');

        if (!eol) // not a carriage return
        {
            sendbuffer[sendlength] = input;
            sendlength++;
        }

        // If the input is a carriage return, or the buffer is full:

        if (sendlength != 0 && (eol || (sendlength == sizeof(sendbuffer) - 1))) // CR or buffer full
        {
            sendbuffer[sendlength] = 0;
            processCommand(sendbuffer);
            Serial.println("ready");
            sendlength = 0; // reset the packet
        }
    }
}
