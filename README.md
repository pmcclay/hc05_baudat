# hc05_baudat

An OTA config tool for [HC-05](https://electropeak.com/learn/tutorial-getting-started-with-hc05-bluetooth-module-arduino/) or similar Bluetooth SPP modules

Use _hc05_baudat_ to set serial BAUD & issue AT commands -- with a hat tip to Émile Baudot

  * UI over Bluetooth connection
  * immediately indicate serial baud/bps
  * autodetect baud/bps
  * prompts for commonly configured parameters
    **or**
    arbitrary AT commands
  * compatible with [Digispark](http://digistump.com/products/1) ATtiny85 board & [clones](https://www.aliexpress.com/wholesale?SearchText=digispark)
  * suitable for dedicated device
  * e.g. TODO hw project link

Written for the Arduino "IDE".

This README will often use _baudat_ in place of _hc05_baudat_. The only collision I found in early 2020 was [Baudat GmbH & Co. KG](https://www.baudat.de) who sell Teutonic cable cutters. This should confuse no one.

## Usage

Compile and upload the _baudat_ sketch and connect an HC-05 module to controller board serial pins. Maybe build a tidy dedicated device. More board & connection info below.

Pair a Bluetooth device with the HC-05 and connect to it using a [Bluetooth terminal app](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal).  BLE devices, if any are similar enough for this to work, do not require pairing.

_baudat_ first displays a banner at all supported bit rates. The one that is readable among other noise tells the currently configured bit rate of the SPP module. If that's all you wanted to know then you're done.

```
##noise##noise##noise##

This is 57600 bps. Type something. 'U'is robust.

##noise##noise##noise##
```

Now you know the SPP module's configured bit rate but _baudat_ does not know which banner you could read. To continue, type something so _baudat_ can see how fast the SPP module sends serial data. Whatever you smash on the keyboard will probably work. 'U' is robust because it looks like `0101010101` on the RX pin. Any sequence of more than one "ordinary" (7 bit) characters sent together (e.g. "line mode" vs "character mode") will probably work because there will probably be a single stop bit between characters `0xxxxxxx010xxxxxxx010xxxxxxx01`. _baudat_ will match the bit rate sent by the SPP module and continue:

```
Hello at 57600 bps

baudat HC-05 configuration tool

Set basic connectivity parameters? [y/n]
```

Answer `y`/`Y` to begin guided configuration of commonly configured parameters, or `n`/`N`  to send arbitrary AT commands to the HC-05.

The "commonly configured parameters" include:
* Bluetooth device name
* BT connection state signal polarity (when used as a RESET signal)
* serial speed in bits per second (baud)

_baudat_'s prompts and responses for guided configuration should be fairly self-explanatory:

```
Set basic connectivity parameters? [y/n] Y

Set Bluetooth device name? [y/n] Y

New name: My_New_BT_Widget

Set BT connection status polarity? [y/n] Y

When connected, set STATE pin LOW(0) or HIGH(1)? [0/1] 0

Supported serial baud rates:
a: 115200
b: 57600
...
f: 4800
g: 2400
Select new speed: [a-g] a
```
_baudat_ will repeat the new values to configure, advise when to press & release the command mode button and wait until you're ready:
```
==== New parameters ====
Name: My_New_BT_Widget
Connected signal level: 0
Baud:  115200

Get ready to press HC-05 command mode button...
Press when LED lights; release when LED flashes.
Ready? [any key]
```
While watching the LED -- on the controller board, not the HC-05 -- send anything when ready to press & release the command button as indicated.

While you're holding the command button, _baudat_ will command the HC-05 as specified. You should **not** see any of the following commands:

```
AT+NAME=My_New_BT_Widget
AT+POLAR=1,0
AT+UART=115200,0,0
```
_baudat_ will then repeat forever:
`power cycle/reset HC-05`

A new speed will not take effect until the HC-05 is reset. If you have wired the STATE signal to RESET the controller, then reconnecting will restart _baudat_ to detect the new rate.

To send arbitrary `AT` commands, answer `n`/`N`  to the first prompt. _baudat_ will then loop:
* prompt for a command, with `AT` prefix assumed
* prompt for command mode button press/release
* display result
```
Set basic connectivity parameters? [y/n] N

Enter command: AT+version

Get ready to press HC-05 command mode button...
Press when LED lights; release when LED flashes.
Ready? [any key]

Go...

Result:
+VERSION:hc01.comV2.1
OK

Enter command: AT 
```

## Boards & compiling

_hc05_baudat_  is written for to compile and run on [UNO](https://store.arduino.cc/usa/arduino-uno-rev3)-like ATmega328p boards and [Digispark](http://digistump.com/products/1)-like ATtiny85 boards. It may work on others.

For **Digispark** boards, _baudat_ requires Jose Rios' [SoftSerial-INT0](https://github.com/J-Rios/Digispark_SoftSerial-INT0/archive/master.zip) library which intrinsically requires serial RX on pin 2. _baudat_ assumes the LED is on pin 1 and uses pin 0 for serial TX. Some early Digispark boards have the LED on pin 0, in which case edit the sketch to swap the definitions of `LED_BUILTIN` and `serialRxPin`.

Digispark (i.e. ATtiny85 with micronucleus bootloader) clock calibration is good for temperatures close (how close?) to when the board was last powered up while plugged into a live USB device (I think). Some early and/or Digistump-branded Digispark boards use a bootloader that calibrates the clock _only_ when connected to a real USB host. A bootloader [upgrade](https://github.com/micronucleus/micronucleus/tree/master/upgrade) will solve that (not-so-old versions configure OSCCAL_SAVE_CALIB [here](https://github.com/micronucleus/micronucleus/blob/master/firmware/configuration/t85_default/bootloaderconfig.h).)


For **UNO**-like or other boards  _baudat_ uses the hardware serial port, or whatever `Serial` connects to, and explicitly assumes RX on pin 0. 

## Wiring

There are many descriptions of wiring HC-05 or similar modules to various Arduino-ish boards for various purposes. [This diagram](http://www.buildlog.net/blog/wp-content/uploads/2017/10/hc-05_prog.png) from [This page](http://www.buildlog.net/blog/2017/10/using-the-hc-05-bluetooth-module/) looks appropriate for using _hc05_baudat_ with UNO-like boards. The connection to RESET is helpful but not essential.

The same connection scheme applies for Digispark-like boards with adjustment for the differently shaped board. +5V, GND & RESET should be obvious. Change RX (on the Digispark, connected to TX from the HC-05) to pin 2 and TX (from the Digispark, connected to RX on the HC-05) to pin 0.
[TODO example]

(It might be that someone thinks HC-05 signal pins tolerate 5V just fine and routinely connects them directly to 5V devices, but I wouldn't know anything about such nonsense. That someone probably punts to resistive dividers in "keeper" builds anyhow.)




## mega tiny
This sketch should compile for "typical" UNO-like ATmega boards, and for Digispark-like ATtiny85 boards.

Cleaning up a 1.0 version in early 2020 was awkward because the [Digistump ATtiny core](https://github.com/digistump/DigistumpArduino) for Arduino was never perfect and has gone unmaintained for a few years while Spence Konde is actively working on [a superior ATtiny core](https://github.com/SpenceKonde/ATTinyCore) that's _almost_ able to handle this without coding much differently than would be for a mega-only version.

### mega
When compiling for not-Digispark boards this uses hardware serial and assumes RX on pin 0. The code almost uses Serial.readBytes{,Until}() with Serial.setTimeout() but...
### tiny
The code almost uses XXX's packaging of SpenceKonde's tiny core but... pulseIn()'s timing loop is borken in that package. SK has recently (Feb 2020) worked to fix that but XXX's zip XXXversion is less recent and an attempt to cherry pick the pulseIn() fix into XXX didn't work for me. So I'm using the [Digispark](http://digispark.fixme) board package for ATtiny support. That's an unstable state because the Digispark core hasn't been touched in XX years while SK has improved his ATtiny core which XX is packaging with a USB loader. SK's core rocks LTO for packing more code into tiny chips and a more complete Stream class. This sketch as it might be written for mega boards mostly works with the SK/CA package except for inaccuracy of pulseIn(). I could beat on that more, but I already had answers for making it work with the Digispark board package. So at least for now:
The code assumes the Digistump Digispark board package and SoftSerial_INT0 library LINK with RX/TX on pins 2/0. It includes a tinyReadBytesUntil() function to stand in for the missing Stream operations. Conditionally extending the SoftSerial class to add the missing ops for Digispark works but makes the code too big. The Digispark package includes a SoftSerial library but SoftSerial_INT0 is hundreds of bytes smaller. And faster. 


===



 Bit rate detection evolved from example by retrolefty:
  https://forum.arduino.cc/index.php?topic=98911.15 posted 30 March 2012

//#ifdef ARDUINO_AVR_ATTINYX5 -- maybe when pulseIn() works better TODO to readme
current pulseIn accurate, but SS0 115.2 breaks but SoftwareSerial (not SoftSerial) works 115.2
vs. Digispark ~5% fast (525/526uSec for 500) but works including SS0 115.2

TODO reset delay/repeat

TODO harmonize readme & comments

TODO SS0 has set[RT]X in constructor.
someone sed moved to begin in dist lib? yes
SK SS?

TODO hw serial end includes flush
SS_0? no. but write is synchronous (looks so)
(SS_0 flush is dump recv buffer)
SK?

TODO use of flush? with SS_0?

TODO does print/println block? looks like all writes returned before print/ln returns so depends on write

TODO SS_0 notes
. pin setup in constructor not begin
. flush is recv clear not send wait (which would be nop?)

T

<!--stackedit_data:
eyJoaXN0b3J5IjpbMTgwNDQyMTY0MSwxNzIwNTY0ODY5LC0yMD
YwMjI0MDI3LDEzMjYxMTI5OTEsLTE2NzA5OTI0NjEsMTQ5NTEw
MjUyLDE3NzI0MjUwMzhdfQ==
-->