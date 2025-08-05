This Arduino sketch implements a wireless thermometer.

The hardware configuration is the combination of the RFM69 wireless module
(<a href='https://learn.sparkfun.com/tutorials/rfm69hcw-hookup-guide'>https://learn.sparkfun.com/tutorials/rfm69hcw-hookup-guide</a>)
and any of several temperature sensors:
<br/>

The semiconductors with positions on the PCB are:
<ul>

<li>TMP102. Its SMD board layout puts it at i2c address 0x49.
<li>There are headers  on the board for various break out temperature sensors.
The pin hookups are the same for any sensor: GND, VCC (3.3V), SDA, SCL are the only
pins used. The pin positions on the various Sparkfun breakout boards differ, which
is why there are several hole patterns on the PCB for headers</li><a href='https://learn.sparkfun.com/tutorials/tmp102-digital-temperature-sensor-hookup-guide'>https://learn.sparkfun.com/tutorials/tmp102-digital-temperature-sensor-hookup-guide.</a>
The sparkfun breakout board wires the i2c address pin at 0x48.
<li>HIH6131, measures humidity as well as temperature. The PCB layout puts it at i2c 0x27.
<a href='https://www.sparkfun.com/products/11295'>HIH6130</a>
has
humidity in addition to temperature, but is limited to -20C to 85C.</li>
<li>Si7021, measures humidity and temperature. It is fixed at i2c 
address 0x40. It has a rather coarse temperature readout--about 1 degree Farenheit resolution
when set to its default 14 bit temperature digitization.
<li>TMP175. Temperature only. PCB leaves all address pins floating, therefore i2c address 0x37
<li>DMP2305U (p-channel MOSFET), DMG3406L (n-channel MOSFET), and SD0805S020S1R0 schotkey diodes. (REV07 of
the PCB and later.) All of
these are part of a <a href='BatteryExtenderCircuit.pdf'>"Battery Extender" circuit</a>. 
</ul>

Battery Extender

With the prior versions of the PCB (REV05 and earlier), the sleep time was determined by a 
single R and C that holds the INT pin (D3 on the Arduino) close to
Vcc/2 for an extended period, which causes the Atmega328 to draw up to about 500uA beyond the sleep
current of 200uA for this circuit. The Battery extender is implemented using a p-channel MOSFET, Q12, with a low
gate threshold (less than -1V is required). Q12 turns on when the RC circuit (at C11 and R11) discharges one
gate threshold below the 3.3V Vcc. When Q12 turns on, it uses Q13 to truncate that normal RC decay, which trigger
port D3's interrupt. The other pair of MOSFETs implements a charge pump so that the RC circuit discharges from
twice Vcc (about 6V) which gives a much longer time delay than is feasible without it.

PCB assembly

While the PCB has multiple positions for placing sensors, the sketch only reports one of them.
The board position for the TMP102 accommodates that chip's 0.5mm lead spacing. That small spacing
is challenging to hand assemble. I succeeded on three boards, but in the first two attempts, I had
to hand rework after the SMD oven bake resulted in one or more leads not connected. 
The third attempt used the technique in this
<a href='https://www.youtube.com/watch?v=xPFujTJbUkI'>video</a> and resulted in all 6 leads nicely soldered.

Power options
<ul>
<li>The circuit is simple and can be haywired without a PCB. Its the builder's choice
<li>The PCB has positions for two AA cells. And the PCB has two hole configurations for
cell holders. Either one two-cell keystone 2462 holder, or two one-cell keystone 2460 holders.
<li>Or, a PJ-202A 5.5mm x 2.1mm power jack may be installed, which routes up to 12VDC
to the regulator on the Pro Mini. The PCB has holes to accommodate the jack on either
its top or bottom.
</ul>

Of the sleep options available at compile time in this sketch, the best
battery life is obtained with a 10M ohm resistor in parallel with a 1uF or larger
capacitor at the position labeled "10uF" under the Pro Mini. 
SMD components of size 1206 are easy enough to solder on. 
The component values are not critical. A pair of AAA lithium cells
powered one of these for 9 months (and counting) with SetDelayLoopCount 
configured such that updates occur about every 11 minutes. A different unit
configured for 5 minute updates lasted 6 months. AA cells are rated
to about twice the Amp-Hour life of AAA cells.

A 2.7K resistor is from A0 to ground for the purpose of 
telemetering the battery volatage. 
On the Arduinio Mini Pro, 3.3V version, solder jumper SJ1 is removed which disables
the on-board volatage regulator and LED.
The system is powered with a 2 cell AA (or AAA) lithium battery wired to VCC (not RAW).

The required SetFrequencyBand settings are documented in RFM69.h (91 in USA). The Arduino
can be programmed through either its serial interface or the ISP pins on the PCB. But
the RFM69 configuration can only be accomplished through the Arduino's serial interface. 

Cover

The CAD directory has a 3D model for a one-piece enclosure that covers the arduino, but leaves the
battery pack exposed. And there is also a 3D model for weather tight ORing enclosure that 
mates the PCB holes.


