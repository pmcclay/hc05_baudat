/*
  hc05_baudat -- an OTA config tool for HC-05 or similar Bluetooth SPP modules
  Set baud & issue AT commands, with a hat tip to Émile Baudot
  * UI over Bluetooth connection
  * immediately indicate serial bit rate
  * autodetect serial bit rate
  * guided configuration or arbitrary AT commands
  * compatible with Digispark (ATtiny85)
  * suitable for dedicated device
  * e.g. TODO hw project link

  Setup:
    Not much.

  Loop:
    // coarse loop() ok since no serialEvent()
    Send banner at all rates -- legible banner indicates current rate
    Detect new serial bit rate
    Begin Serial Stream at detected rate
    Ask: configure basic parameters?
      Yes:
        Prompt for new info
        Prompt user to switch HC-05 to command mode
        Configure HC-05
        Reset HC-05 into 38.4kbps command mode
        Prompt user to release HC-05 command mode
        Reset HC-05 to new serial bit rate
      No:
        Loop:
          Prompt for AT command
          Prompt user to switch HC-05 to command mode
          Send command to HC-05
          Read/Store response from HC-05
          Prompt user to release HC-05 command mode
          Display stored HC-05 response

  See README & updates at https://github.com/pmcclay/hc05_baudat/

  Copyright 2019 Paul McClay <mcclay at g mail>
  MIT License & disclaimer https://opensource.org/licenses/MIT
*/


#include <limits.h>


/***** conditional macros *****/


//#define BITTIMER // uncomment to loop reporting detected bit times & rates
#define BITTIMERBPS 57600


#ifdef ARDUINO_AVR_DIGISPARK
// Compiling for Digispark-like ATtiny85 board with Digistump core

// Worst case with small flash, big bootloader and no LTO
// This is the target for compactness

// The bootloaders on early/real Digisparks may calibrate clock only when connected to a real USB host.
// See https://digistump.com/wiki/digispark/tricks "Detect if system clock was calibrated".
// The "aggressive" micronucleus bootloader does not calibrate the clock.

#define LED_BUILTIN 1 // early units may have LED on pin 0

// Use Digispark_SoftSerial-INT0 library
// https://github.com/J-Rios/Digispark_SoftSerial-INT0/archive/master.zip
// SoftSerial in Digistump lib is too big for this case and fails at 115200
#include <SoftSerial_INT0.h>
const uint8_t serialRxPin = 2; // required by Digispark_SoftSerial-INT0
SoftSerial mySerial(serialRxPin, 0); // or TX on pin 1 if LED on 0
#define Serial mySerial
const uint8_t textBufferSize = _SS_MAX_RX_BUFF;
#define NOFLUSH // no need & sw serial uses old style flush that might hurt

#define BITTIMEVAR 3 // accept +/- 12.5% (1/(2^3)) variation of bit time in uSec
                     // Digispark clock may vary. In some cases HC-05 can receive what Ds sends
                     //   but Ds receives only 1 good char from HC-05. Enough to dial down bps.


#elif defined ARDUINO_AVR_ATTINYX5
// Compiling for ATtiny85 board with some variation of tiny core
// (other ATTinyx5 have insufficient memory)

// No promises
// Assuming Digispark-like board/bootloader, this works with
// - Spence Konde's ATTinyCore 1.3.3 (https://github.com/SpenceKonde/ATTinyCore)
// - post-1.3.3 pulseIn fix (https://github.com/SpenceKonde/ATTinyCore/issues/384)
// - {platform,boards}.txt tweaked for micronucleus (reference tbd; kinda like
//     https://hackaday.io/project/162845-attiny84a-tiniest-development-board)

#define LED_BUILTIN 1 // early Digispark-like boards may have LED on pin 0

#include <SoftwareSerial.h>
const uint8_t serialRxPin = 2; // if wired for Digispark core + SoftSerial-INT0
SoftwareSerial mySerial(serialRxPin, 0); // not LED pin
#define Serial mySerial
const uint8_t textBufferSize = _SS_MAX_RX_BUFF;
#define NOFLUSH // no need & sw serial may use old style flush that might hurt

#define BITTIMEVAR 3 // accept +/- 12.5% (1/(2^3)) variation of bit time in uSec
                     // ATtiny85 clock may vary. At 115200 HC-05 receives what tiny sends
                     //   but tiny receives only few good chars from HC-05. Enough to dial down bps.


#else
// not a Digispark or similar ATtiny85 board

// assume hardware serial with receive on pin 0
const uint8_t serialRxPin = 0;
const uint8_t textBufferSize = SERIAL_RX_BUFFER_SIZE;
#define BITTIMEVAR 4 // accept +/- 6.35% (1/(2^4)) variation of bit time
                     // Expect accurate clock. 5 (+/-3%) would likely work.


#endif


/***** constants *****/


const uint8_t maxNameLen = 32; // max HC-05 BT name, per at least one reference

const uint8_t samples = 8; // number of mark/1/high bits sample attempts per rate detect attempt

struct rateParms {
  long bps; // "standard" bit rate
  int uSec; // bit duration TODO compare code size to calculate from bps
};

const struct rateParms rateParms[] {
	
  #if !defined ARDUINO_AVR_DIGISPARK && ! defined ARDUINO_AVR_ATTINYX5
  // don't let ATtiny set a speed it can't reconnect to
  {500000, 2},   // possible with hw serial at 16MHz but not commonly supported by SPP modules
  //{460800, 2}, // commonly supported by SPP modules but not possible with hw serial at 16MHz
  {230400, 4},
  #endif

  {115200, 9},
  {57600, 17},
  {38400, 26},
  //{28800, 35}, // not used by known SPP modules
  {19200, 52},
  //{14400, 69}, // not used by known SPP modules
  {9600, 104},
  {4800, 208},
  {2400, 417}
};

const uint8_t numRates = (sizeof(rateParms) / sizeof(struct rateParms));


/***** global variables (because microcontroller) *****/


long bps = 0;
int bitTime; // microseconds
char textBuffer[textBufferSize+1]; // +1 for a null beyond end of full buffer
uint8_t polarity;


/***** setup() & loop() (because Arduino) *****/


// Sparse setup() & big loop() vs self-reset to restart, like suggested here:
// https://forum.arduino.cc/index.php?topic=381083.msg2635570#msg2635570

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
  }


void loop()
{    
  // blast banner at all rates
  // should display one recognizable bps number amidst noise
  // ...and flash LED a bit to signal sketch startup
  for (uint8_t a = 0; a < numRates; a++)
  {
    digitalWrite (LED_BUILTIN, a%2); // alternate 0,1
    Serial.begin(rateParms[a].bps);
    Serial.print(F("\n\nThis is "));
    Serial.print(rateParms[a].bps, DEC);
    Serial.println(F(" bps. Type something. 'U' is robust.\n")); //'U' = x10101010101x (8 bits no parity)
    #ifndef NOFLUSH
    Serial.flush();
    #endif
    Serial.end();
    delay(2); // extend stop state before continue at slower rate
  }
  digitalWrite (LED_BUILTIN, LOW);


#ifdef BITTIMER // just loop reporting detecting times/rates
do {
  bps=0;
#endif

  
  /*
   * Detect bit rate of incoming serial comms
   * 
   * * Time shortest of a sample of RX high states
   * * Compare time approximately with reciprocals of recognized bit rates
   * 
   * Needs to see a single high bit within a sample, which is not assured  
   * Any 2+ 7bit ASCII chars will include 010 msb-stop-start sequence
   * Stop bit is probably one bit-time; send 0x0000 to time exactly one stop bit
   * 'U'=0b01010101 assures some single bits
   * 
   * This bit rate detection method/code evolved after starting from:
   * https://forum.arduino.cc/index.php?topic=98911.15 by retrolefty
   * 
   */

  do {
    bitTime = rateParms[numRates - 1].uSec * 2; // longer than any accepted bit duration
    while (digitalRead(serialRxPin)); // if line idle, wait for start bit
    for (uint8_t a = 0; a < samples; a++)
    {
      #ifndef ARDUINO_AVR_DIGISPARK
      cli(); // yes, disable interrupts. since ~2015.
      #endif   // but Digispark core uses ye olde style pulseIn()
      long temp = pulseIn(serialRxPin, HIGH, bitTime);  // measure the next mark bit width - up to shortest already recorded
      #ifndef ARDUINO_AVR_DIGISPARK
      sei();
      #endif
      if (temp)
        bitTime = temp; // only decrease - timeout limits detections to no longer than shortest so far
    }
    
    uint8_t a = 0;
    while (a != numRates && bitTime > rateParms[a].uSec + (rateParms[a].uSec >> BITTIMEVAR ? rateParms[a].uSec >> BITTIMEVAR : 1 /*hack: 230400 often 5uSec*/)) a++;
    if (a < numRates && bitTime >= rateParms[a].uSec - (rateParms[a].uSec >> BITTIMEVAR))
      bps = rateParms[a].bps;

    #ifdef BITTIMER
    if (bitTime != rateParms[numRates - 1].uSec * 2) {
      Serial.begin(BITTIMERBPS);
      Serial.print(F("bitTime "));
      Serial.println(bitTime, DEC);
      Serial.print(F("bps "));
      Serial.println(bps, DEC);
      delay(500);
      Serial.end();
    }
    #endif

  } while (!bps); // i.e. until bit rate found within range


#ifdef BITTIMER
} while (true); // BITTIMER never exits this loop
#endif


  Serial.begin(bps);
  Serial.print(F("\n\nHello at "));
  Serial.print(bps, DEC);
  Serial.println(F(" bps"));
  Serial.println(F("\nbaudat HC-05 config/command tool\n"));

  Serial.print(F("Set BT name, \"polar\" & serial bit rate?"));
  if (yorn()) { 
    Serial.print(F("\nSet Bluetooth device name?"));
    if (yorn()) {
      Serial.print(F("\nNew name: "));
      textBuffer[read7BitText(textBuffer, maxNameLen, ULONG_MAX)] = '\0';
      Serial.println(textBuffer);
    } else textBuffer[0] = '\0'; // null name = no change
  
    Serial.print(F("\nSet BT connection status polarity?"));
    if (yorn()) {
      Serial.print(F("\nWhen connected, set STATE pin LOW(0) or HIGH(1)? [0/1] "));
      do {
        while (!Serial.available()); // wait for input
        polarity = Serial.read();
      } while (polarity != '0' && polarity != '1'); // i.e. until recognized input
      discardInput();
      Serial.println((char) polarity);
    } else
      polarity = '\0'; // nul = no change
  
    Serial.println(F("\nSupported serial baud rates:"));
    for (uint8_t a = 0; a < numRates; a++) {
      Serial.print((char) (a + 'a'));
      Serial.print(F(": "));
      Serial.println(rateParms[a].bps);
    }
    Serial.print(F("Select new speed: [a-"));
    Serial.print((char) (numRates-1 + 'a'));
    Serial.print(F("] "));   
    bps = 0; // re-use
    do {
      while (!Serial.available());
      uint8_t speed = ((Serial.read() & 0b11011111) /*fold case*/ - 'A');
      if (speed >= 0 && speed < numRates) {
        bps = rateParms[speed].bps;
        Serial.println((char) (speed + 'a'));
      }
    } while (bps == 0);
    
    Serial.println(F("\n==== New parameters ===="));
    if (textBuffer[0]) {
      Serial.print(F("BT Name: "));
      Serial.println(textBuffer);
    }
    if (polarity) {
      Serial.print(F("Connected STATE signal level: "));
      Serial.println((char) polarity);
    }
    Serial.print(F("Baud:  "));
    Serial.println(bps, DEC);

    commandStart();

    if (textBuffer[0]) {
      Serial.print(F("AT+NAME="));
      Serial.println(textBuffer);
      #ifndef NOFLUSH
      Serial.flush();
      #endif
      delay(100);
    }
    if (polarity) {
      Serial.print(F("AT+POLAR=1,"));
      Serial.println((char) polarity);
      #ifndef NOFLUSH
      Serial.flush();
      #endif
      delay(100);
    }
    Serial.print(F("AT+UART="));
    Serial.print(bps);
    Serial.println(F(",0,0"));
    delay(100);
    Serial.println(F("AT+RESET"));
    Serial.end(); // hw Serial.end calls flush; sw Serial writes not buffered
    Serial.begin(38400); // do now because sw serial.end() breaks delay()

    commandStop();

    Serial.println(F("AT+RESET"));
    delay(500); // let that happen before next TX
    Serial.end();
    
  } else { // not basic config -> arbitrary AT commands
    
    while (true) {
      discardInput();
      Serial.print(F("\nEnter command: AT"));
      textBuffer[read7BitText(textBuffer, textBufferSize-1, ULONG_MAX)] = '\0';
      Serial.println(textBuffer);
      
      commandStart();
      
      Serial.print(F("AT"));
      Serial.println(textBuffer);
      #ifndef NOFLUSH
      Serial.flush();
      #endif
      textBuffer[read7BitText(textBuffer, textBufferSize-1, 1000)] = '\0'; // read for ~1 second and add null terminator to make a string

      commandStop();
    
      Serial.println(F("\nResult:"));
      Serial.println(textBuffer);
    }
  }
}


/***** functions *****/


boolean yorn() {
  char inChar;
  discardInput();
  Serial.print(F(" [y/n] "));
  do {
    while ((inChar = Serial.read()) == -1);
    inChar &= 0b11011111; // fold case
  } while (inChar != 'Y' && inChar != 'N');
  Serial.println(inChar);
  discardInput();
  return (inChar == 'Y');
}


void discardInput() { 
  do delay(5); // byte receive may be in process at low bit rate
  while (Serial.read() != -1);
}


uint8_t read7BitText(char *buffer, uint8_t length, unsigned long timeout) {
  // - read 7-bit ASCII text up to next control character or timeout
  // - returns # characters read
  // - solves CR vs LF vs CRLF by stopping at any control character
  // - respects DEL (0x7F)
  // - single byte index & return values constrain buffer <= 255 bytes
  unsigned long startMillis = millis();
  uint8_t index = 0;
  int c;
  while (index < length) {
    do {
      c = Serial.read();
    } while ((c == -1) && (millis() - startMillis < timeout));
    if (c < ' ') break; // control character or timeout
    if (!(c & 0b10000000)) { // ignore > 7-bit chars
      if (c == 0b01111111) { // DEL
        if (index != 0) index--; // have to catch it; may as well do it
      } else {
        buffer[index++] = (char)c; // ' ' <= c < DEL
      }
    }
  }
  return(index);
}


void commandStart() {
  Serial.println(F("\nGet ready to press HC-05 command mode button ..."));
  Serial.println(F("Press when LED lights; release when LED flashes."));
  discardInput();
  Serial.println(F("\nReady? [any key]"));
  while (!Serial.available());
  discardInput();
  Serial.println(F("\nGo..."));
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1500);
}


void commandStop() {
  for (uint8_t a=0; a<11; a++) { //five flashes then off (if on at start)
    delay(100);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); 
  }
  delay(500);
}
