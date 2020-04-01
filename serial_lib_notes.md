# Notes on Serial/Software Serial & pulseIn() variations
This isn't about *baudat* per se.

**SpenceKonde ATTinyCore:**
* latest pulseIn() accurate
* breaks Digispark_SoftSerial-INT0 at 115.2

**SK SoftwareSerial:**
* (mostly) works at 115.2 (modulo long input strings)
* * but flakier than DS-SS-INT0, iirc
* sets [RT]X in constructor ([issue](https://github.com/arduino/Arduino/issues/3041))
* flush() clears input (lame)
* end() does not flush() (harmless because sync tx)
* end() breaks delay() until next begin(), iirc

**SK (TinySoftware)Serial:**
* haven't tried
* RX/TX pins 1/0 (vs 0/1 like lots other Arduini)
* sets [RT]X in begin() (yay)
* flush() empty (yay)
* end() does not flush() (fine)
* ?? end() breaks delay()?

**Digispark:**
* pulseIn() ~5% fast (reports 525/526µSec for 500) but close enough for *baudat*

**Digispark SoftwareSerial:**
* doesn't exist; SoftSerial included instead

**Digispark SoftSerial:**
* no 115.2
* sets [RT]X in constructor  ([issue](https://github.com/arduino/Arduino/issues/3041))
* flush() clears input (lame)
* end() does not flush() (harmless because sync tx)
* ?? end() breaks delay()?

**Digispark (JRios) SoftSerial-INT0:**
* works at 115.2 (modulo long input strings)
* sets [RT]X in constructor  ([issue](https://github.com/arduino/Arduino/issues/3041))
* flush() clears input (lame)
* end() does not flush() (harmless because sync tx)
* end() breaks delay() until next begin()

**Arduino (1.8.12) SoftwareSerial:**
* sets [RT]X in constructor  ([issue](https://github.com/arduino/Arduino/issues/3041))
* flush() does nothing (intentional because sync tx) 
* end() does not flush() (intentional because sync tx)
* ?? end() breaks delay()?

**Arduino (1.8.12) (hardware)Serial:**
* sets [RT]X in begin - ([fixed](https://github.com/arduino/Arduino/commit/8c8b78ce4975103ed82067fe98f2101fcce48d68))
* flush() waits for tx (because buffered tx)
* end() calls flush() (because buffered tx)
* end() does not break delay(), iirc
