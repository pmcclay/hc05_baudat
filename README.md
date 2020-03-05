# hc05_baudat

An OTA config tool for [HC-05](https://electropeak.com/learn/tutorial-getting-started-with-hc05-bluetooth-module-arduino/) or similar Bluetooth SPP modules

Use *hc05_baudat* to set serial BAUD & issue AT commands -- with a hat tip to Émile Baudot

  * UI over Bluetooth connection
  * immediately indicate serial/UART baud/bit-rate/bps
  * autodetect current serial bit rate
  * prompts for commonly configured parameters
    **or**
    arbitrary AT commands
  * compatible with [Digispark](http://digistump.com/products/1) ATtiny85 board & [clones](https://www.aliexpress.com/wholesale?SearchText=digispark)
  * suitable for dedicated device
  * e.g. TODO hw project link

Written for the Arduino "IDE".

This README will often use _baudat_ in place of _hc05_baudat_. The only collision I found in early 2020 was [Baudat GmbH & Co. KG](https://www.baudat.de) who sell badass cable cutters. This should confuse no one.

## Usage

Upload sketch and connect an HC-05 to serial pins.

Connect to HC-05 with a [Bluetooth terminal app](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal).

*baudat* first displays a banner at all supported bit rates. The one that is readable among other noise tells the currently configured bit rate of the SPP module. If that's all you wanted to know then you're done.
```
##noise##noise##noise##

This is 57600 bps. Type something. 'U'is robust.

##noise##noise##noise##
```
Now you know the SPP module's configured bit rate but *baudat* does not yet know what you know. To continue, type something so *baudat* can see how fast the SPP module sends serial data. Whatever you smash on the keyboard will probably work. 'U' is robust because it looks like `0101010101` on the wire. Any sequence of more than one "ordinary" (7 bit) characters sent together (e.g. "line mode" vs "character mode") will probably work because there will probably be a single stop bit between characters `0xxxxxxx010xxxxxxx010xxxxxxx01`. *baudat* will match the bit rate of the SPP module and continue.

```
Hello at 57600 bps

baudat HC-05 configuration tool

Set name|polar|bps? [y/n]
```

Answer `y`/`Y` to begin guided configuration of commonly configured parameters, or `n`/`N`  to send arbitrary AT commands to the HC-05.

The "common" parameters include:
* Bluetooth device name
* BT connection state signal polarity (when used as a RESET signal)
* serial speed in bits per second (baud)

Prompts and responses for guided configuration should be fairly self-explanatory:

```
Set name|polar|bps? [y/n] Y

Update Bluetooth device name? [y/n] Y

New name: My_New_BT_Widget

Update connection status polarity? [y/n] Y

When connected, set status signal LOW or HIGH? 0/1: 0

Select new serial speed
a: 115200
b: 57600
...
f: 4800
g: 2400
```
_baudat_ will repeat the new values to configure, advise when to press & release the command mode button and wait until you're ready:
```
New parameters
Name: My_New_BT_Widget
Connected state signal: 0
speed 115200

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
_baudat_ will then repeat forever: `power cycle/reset HC-05`
A new speed will not take effect until the HC-05 is reset.
After changing the BT name, you'll have to pair with the new name to reconnect.

To send arbitrary `AT` commands, answer `n`/`N`  to the first prompt. _baudat_ will then loop:
* prompt for a command, with `AT` prefix assumed
* prompt for command mode button press/release
* display result
```
Set name|polar|bps? [y/n] N

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

## Connect

[TODO many]() [examples]()

Reset cap is handy to start sketch on BT connection.

(It might be that someone thinks HC-05 signal pins tolerate 5V just fine and routinely connects them directly to 5V devices, but I wouldn't know anything about such nonsense. That someone probably punts to resistive dividers in "keeper" builds anyhow.)


### "the usual" UNO-like boards
hardware serial
* whatever "Serial" connects to
* but bps detection specifically assumes RX on pin 0



### connecting to Digispark-like ATtiny85 boards

RX (from HC-05 TX) on pin 2. Because INT0.
TX (to HC-05 RX) on pin 0. LED on 0 might be a reason to use 1.
TODO [example]()




## mega tiny
This sketch should compile for "typical" UNO-like ATmega boards, and for Digispark-like ATtiny85 boards.

Cleaning up a 1.0 version in early 2020 was awkward because the [Digistump ATtiny core](https://github.com/digistump/DigistumpArduino) for Arduino was never perfect and has gone unmaintained for a few years while Spence Konde is actively working on [a superior ATtiny core](https://github.com/SpenceKonde/ATTinyCore) that's _almost_ able to handle this without coding much differently than would be for a mega-only version.

### mega
When compiling for not-Digispark boards this uses hardware serial and assumes RX on pin 0. The code almost uses Serial.readBytes{,Until}() with Serial.setTimeout() but...
### tiny
The code almost uses XXX's packaging of SpenceKonde's tiny core but... pulseIn()'s timing loop is borken in that package. SK has recently (Feb 2020) worked to fix that but XXX's zip XXXversion is less recent and an attempt to cherry pick the pulseIn() fix into XXX didn't work for me. So I'm using the [Digispark](http://digispark.fixme) board package for ATtiny support. That's an unstable state because the Digispark core hasn't been touched in XX years while SK has improved his ATtiny core which XX is packaging with a USB loader. SK's core rocks LTO for packing more code into tiny chips and a more complete Stream class. This sketch as it might be written for mega boards mostly works with the SK/CA package except for inaccuracy of pulseIn(). I could beat on that more, but I already had answers for making it work with the Digispark board package. So at least for now:
The code assumes the Digistump Digispark board package and SoftSerial_INT0 library LINK with RX/TX on pins 2/0. It includes a tinyReadBytesUntil() function to stand in for the missing Stream operations. Conditionally extending the SoftSerial class to add the missing ops for Digispark works but makes the code too big. The Digispark package includes a SoftSerial library but SoftSerial_INT0 is hundreds of bytes smaller. And faster. 


===

detect &amp; set HC-05 bit rate, name &amp; polarity

 Bit rate detection evolved from example by retrolefty:
  https://forum.arduino.cc/index.php?topic=98911.15 posted 30 March 2012

//#ifdef ARDUINO_AVR_ATTINYX5 -- maybe when pulseIn() works better TODO to readme


[https://github.com/micronucleus/micronucleus/blob/master/firmware/configuration/t85_default/bootloaderconfig.h](https://github.com/micronucleus/micronucleus/blob/master/firmware/configuration/t85_default/bootloaderconfig.h)
OSCCAL_SAVE_CALIB
<!--stackedit_data:
eyJoaXN0b3J5IjpbLTEzODA5NzIzNzldfQ==
-->