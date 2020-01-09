# Firecast

Firecast is an auxiliary audio adapter for General Motors factory radios,
similar to the GM9-AUX that may be hard-to-find or discontinued.  It works by
pretending to be a slave cassette player.  When powered on, it tells the head
unit that it has begun playing, and the head unit will switch to your audio
device as its input source.

* You do NOT need to disassemble your radio and splice in a 3.5mm jack.
* You do NOT need to have a slave cassette/CD player (unlike the AAI-GM9).
* You DO need to have the 9-pin connector on the back of your radio, or the
  cassette/CD player connector that may be hidden somewhere in your car.

In particular, you need access to the three audio wires that go into the head
unit, the E&C bus (one signal and one ground wire), and a power source.  For
the bill of materials, you will need an arduino and a handful of circuit
components to connect the arduino and your audio device to those wires in your
car.  See the [schematic](schematic.md) for more info.

Firecast was developed on the Monsoon sound system in my Pontiac Firebird.
It should work for other radios, but your mileage may vary.
