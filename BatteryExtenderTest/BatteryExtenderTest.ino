/* This sketch is for an Arduino wired to (only) the Battery Extender circuit,
** in order to test that circuit part way through the assembly process. 
** Why then? because the Arduino Mini Pro is soldered directly on top of the Battery Extender
** circuit on the PCB such that the Battery Extender components are inaccessible once the Mini Pro 
** is soldered in
*/



namespace {
    const int TIMER_RC_GROUND_PIN = 4;
    const int TIMER_RC_PIN = A3; // sleep uProc using RC circuit on this pin
}

void setup()
{
    Serial.begin(9600);
    Serial.println("Battery Extender Test");

    analogReference(INTERNAL);

    digitalWrite(TIMER_RC_GROUND_PIN, LOW);
    pinMode(TIMER_RC_GROUND_PIN, OUTPUT);

}

static void prAnalog(uint16_t t, uint16_t v)
{
    Serial.print("t=");
    Serial.print(t);
    Serial.print(" v=");
    Serial.println(v);
}

static bool processCommand(const char *v)
{
    auto beg = millis();
    auto c = *v;
    if (isalpha(c) && toupper(c) == 'G')
    {
        pinMode(TIMER_RC_GROUND_PIN, OUTPUT);
        pinMode(TIMER_RC_PIN, OUTPUT);
        digitalWrite(TIMER_RC_PIN, HIGH);
        digitalWrite(TIMER_RC_GROUND_PIN, HIGH);
        delay(1);
        digitalWrite(TIMER_RC_GROUND_PIN, LOW);
        pinMode(TIMER_RC_PIN, INPUT);

        auto prev = millis();
        for(;;)
        {
            auto tm = millis();
            if (tm - beg > 1000 * 30)
                break;
            if (tm - prev < 333)
                continue;
            prev = tm;
            uint16_t v = analogRead(TIMER_RC_PIN);
            prAnalog(static_cast<uint16_t>(tm-beg), v);
        }
    }
    return true;
}


void loop()
{
    unsigned long now = millis();

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

        if (input != '\r') // not a carriage return
        {
            sendbuffer[sendlength] = input;
            sendlength++;
        }

        // If the input is a carriage return, or the buffer is full:

        if ((input == '\r') || (sendlength == sizeof(sendbuffer) - 1)) // CR or buffer full
        {
            sendbuffer[sendlength] = 0;
            processCommand(sendbuffer);
            Serial.println("ready");
            sendlength = 0; // reset the packet
        }
    }
}
