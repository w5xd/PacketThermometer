This Arduino sketch implements a wireless thermometer.

The hardware configuration is the combination of the RFM69 wireless module
and TMP102 temperature sensor:
<br/>https://learn.sparkfun.com/tutorials/rfm69hcw-hookup-guide
<br/>https://learn.sparkfun.com/tutorials/tmp102-digital-temperature-sensor-hookup-guide.

A 2.7K resistor is added from A0 to ground for the purpose of 
telemetering the battery volatage.
On the Arduinio Mini Pro, 3.3V version, solder jumper SJ1 is removed (which disables
the on-board volatage regulator and LED.)
The system is powered with a 2 cell AAA lithium battery wired to VCC (not RAW).

