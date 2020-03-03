# hc05-baudat

An OTA config tool for HC-05 or similar Bluetooth SPP modules

Use *hc05-baudat* to set serial BAUD & issue AT commands  with a hat tip to Émile Baudot

  * UI over Bluetooth connection
  * immediately indicate serial/UART bit rate/bps
  * autodetect serial bit rate
  * compatible with Digispark (ATtiny85)
  * suitable for dedicated device
  * e.g. TODO hw project link

Written for the Arduino "IDE".

## mega tiny
This sketch compiles for "typical" ATmega boards, like UNO & friends, and for Digispark ATtiny85 boards.
### mega
When compiling for not-Digispark boards this uses hardware serial and assumes RX on pin 0. The code almost uses Serial.readBytes{,Until}() with Serial.setTimeout() but...
### tiny
The code almost uses XXX's packaging of SpenceKonde's tiny core but... pulseIn()'s timing loop is borken in that package. SK has recently (Feb 2020) worked to fix that but XXX's zip XXXversion is less recent and an attempt to cherry pick the pulseIn() fix into XXX didn't work for me. So I'm using the [Digispark](http://digispark.fixme) board package for ATtiny support. That's an unstable state because the Digispark core hasn't been touched in XX years while SK has improved his ATtiny core which XX is packaging with a USB loader. SK's core rocks LTO for packing more code into tiny chips and a more complete Stream class. This sketch as it might be written for mega boards mostly works with the SK/CA package except for inaccuracy of pulseIn(). I could beat on that more, but I already had answers for making it work with the Digispark board package. So at least for now:
The code assumes the Digistump Digispark board package and SoftSerial_INT0 library LINK with RX/TX on pins 2/0. It includes a tinyReadBytesUntil() function to stand in for the missing Stream operations. Conditionally extending the SoftSerial class to add the missing ops for Digispark works but makes the code too big. The Digispark package includes a SoftSerial library but SoftSerial_INT0 is hundreds of bytes smaller. And faster. 


===

detect &amp; set HC-05 bit rate, name &amp; polarity

<!--stackedit_data:
eyJoaXN0b3J5IjpbLTEyNjAwMzM1NjRdfQ==
-->