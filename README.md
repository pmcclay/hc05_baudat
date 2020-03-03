# hc05-baudat

An OTA config tool for HC-05 or similar Bluetooth SPP modules

Use *hc05-baudat* to set serial BAUD & issue AT commands -- with a hat tip to Émile Baudot

  * UI over Bluetooth connection
  * immediately indicate serial/UART baud/bit-rate/bps
  * autodetect serial bit rate
  * compatible with [Digispark](http://digistump.com/products/1) ATtiny85 board & [clones](https://www.aliexpress.com/wholesale?SearchText=digispark)
  * suitable for dedicated device
  * e.g. TODO hw project link

Written for the Arduino "IDE".

## Connect

[TODO many]() [examples]()

Reset cap is handy to start sketch on BT connection.

(It might be that someone thinks HC-05 modules tollerate 5V just fine and routinely connects them directly to 5V devices, but I wouldn't know anything about such nonsense. That someone probably punts to resistive dividers in "keeper" builds anyhow.)


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



<!--stackedit_data:
eyJoaXN0b3J5IjpbLTgxNDk0MzM3NCw5ODk2MTEzODAsLTExMj
gxMzgwMzFdfQ==
-->