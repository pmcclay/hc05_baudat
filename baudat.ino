//#define DEBUG
#define DEBUGBPS 38400
#define DEBUG2k

/*
  baudat -- an OTA config tool for HC-05 or similar Bluetooth SPP modules
  * immediately indicate serial/UART bit rate/bps
  * autodetect serial bit rate
  * UI over Bluetooth connection
  * compatible with Digispark (ATtiny85)
  * suitable for dedicated standalone device
  Copyright 2019 Paul McClay <mcclay at g mail>
  MIT License https://opensource.org/licenses/MIT

  Send banner at all rates -- legible banner indicates configured bps
  Detect serial bit rate
  Using detected rate, prompt for new name/polarity/rate
  Configure HC-05 or similar for new rate

  Bit rate detection started with example by retrolefty:
  https://forum.arduino.cc/index.php?topic=98911.15 posted 30 March 2012
  and has evolved to mostly different.
 */


#ifdef ARDUINO_AVR_DIGISPARK // Runs on Digispark. (Generalize to other ATtiny?)
//#ifdef ARDUINO_AVR_ATTINYX5 TODO
// TODO hw project URL
// Use Digispark_SoftSerial-INT0 library
// https://github.com/J-Rios/Digispark_SoftSerial-INT0/archive/master.zip
// Early Digispark bootloaders may calibrate clock only when connected to a real USB host.
// See https://digistump.com/wiki/digispark/tricks "Detect if system clock was calibrated".
#include <SoftSerial_INT0.h> // This one goes to 11 (11.52 myriabits/sec) and saves ~240 bytes
// #include <SoftSerial.h> // Supplied in IDE, but limited to 57.6k

/*
 * Using readBytesUntil, but Digispark core lacks.
 * Could use e.g. https://github.com/SpenceKonde/ATTinyCore but would rather keep save-and-upload compat with Arduino + Digispark board pkg
 * -- but already kinda breaking that with SS_INT0
 * Maybe add the missing methods a la https://stackoverflow.com/questions/18804402/add-a-method-to-existing-c-class-in-other-file
 * Maybe within #ifdef to keep single file but then harder to read this.
 * If using github, then maybe not so bad to have another file.
 * Or conditionally define READBYTES as Serial.readBytes or local readBytes function without trying to fake the class member
 * Or copy the readBytes (readBytesUntil, timedRead, setTimeout) code here as local fcns for all
 * TODO
 */
/*
class SoftSerialAdd : public SoftSerial {
  // Selected parts copied from core Stream.h/.cpp
  protected:
    unsigned long _timeout;      // number of milliseconds to wait for the next char before aborting timed read
    unsigned long _startMillis;  // used for timeout measurement
    int timedRead() {
      int c;
      _startMillis = millis();
      do {
        c = read();
        if (c >= 0) return c;
      } while(millis() - _startMillis < _timeout);
      return -1;     // -1 indicates timeout
    };
  public:
    SoftSerialAdd(uint8_t receivePin, uint8_t transmitPin, bool inverse_logic = false) :
      SoftSerial(receivePin, transmitPin, inverse_logic) {}
    void setTimeout(unsigned long timeout) {
      _timeout = timeout;
    }
    size_t readBytesUntil( char terminator, char *buffer, size_t length) {
      size_t index = 0;
      while (index < length) {
        int c = timedRead();
        if (c < 0 || c == terminator) break;
        *buffer++ = (char)c;
        index++;
      }
      return index; // return number of characters, not including null terminator
    }
  // End copy from core Stream.h/.cpp
};
*/
/*
size_t tinyReadBytesUntil(char terminator, char *buffer, /*size_t*//* uint8_t length, long timeout) {
  /*size_t*//* uint8_t index = 0;
  while (index < length) {
//    int c = timedRead();

    int c;
    unsigned long _startMillis = millis();
    do {
      c = Serial.read();
      if (c >= 0) break;
    } while(millis() - _startMillis < timeout);
//    return -1;     // -1 indicates timeout
    
    if (c < 0 || c == terminator) break;
    *buffer++ = (char)c;
    index++;
  }
  return index; // return number of characters, not including null terminator
}
*/
const uint8_t serialRxPin = 2; // required by Digispark_SoftSerial-INT0
//SoftSerialAdd mySerial(2, 0);
SoftSerial mySerial(2, 0);
#define Serial mySerial
const uint8_t textBufferSize = _SS_MAX_RX_BUFF;
#define USECVAR 3 // accept +/- 1/(2^3) variation of bit time in uSec
                  // Digispark clock may vary. If marginal, usually at least 1st char received is good. HC-05s seem to receive marginal rates well
#define LED_BUILTIN 1


#else // not a Digispark
// assume hardware serial with receive on pin 0
const uint8_t serialRxPin = 0;
const uint8_t textBufferSize = SERIAL_RX_BUFFER_SIZE;
#define USECVAR 4 // accept +/- 1/(2^4) variation of bit time
                  // Expect accurate clock. 5 would likely work.
#endif

#include <limits.h>

long bps = 0;
// #define temp bps // overload bps for memory frugality TODO or not
int bitTime; // microseconds
const uint8_t samples = 8; // number of mark/1/high bits to try to measure
                           // any >1 non-extended ASCII chars will have 010 msb-stop-start sequence
                           // assuming no parity & stop bit is one bit time
                           // send 0x0000 to time stop bit
const int patience = 1000; // millis allowance for human reaction
const uint8_t maxNameLen = 32; // max 32 chars per at least one reference

char textBuffer[textBufferSize];
int8_t chrPtr;
#define polarity chrPtr // overload chrPtr;

struct rateParms {
  long bps; // "standard" bit rate, or fault flag
  int uSec; // bit duration
  // could calculate uSec but expect more compact to store than to add division code
  // TODO storing second uSec for range < code to calc +/- 1/16?
};


const struct rateParms rateParms[] {
#ifndef ARDUINO_AVR_DIGISPARK
// don't let Digispark set a speed it can't reconnect to
  {500000, 2}, // possible with hw serial at 16MHz but not commonly supported by SPP modules
//{460800, 2}, // commonly supported by SPP modules but not possible with hw serial at 16MHz
  {230400, 4},
#endif
#if !defined ARDUINO_AVR_DIGISPARK || defined SoftSerialINT0_h
  {115200, 9}, // ok for not-Digispark or Digispark with SoftSerial_INT0 (SS receive not bulletproof - go slower to set a long bt name)
#endif
  {57600, 17},
  {38400, 26},
//  {28800, 35}, // not used by known SPP modules
  {19200, 52},
//  {14400, 69}, // not used by known SPP modules
  {9600, 104},
  {4800, 208},
  {2400, 417}
#ifdef DEBUG2k
, {2000, 500} 
#endif
};

const uint8_t numRates = (sizeof(rateParms) / sizeof(struct rateParms));

void discardInput() {
  delay(5); // follow-on bytes can be slow
  while (Serial.available()) {
    Serial.read();
    delay(5);
  }
}

boolean yorn() {
  char inChar;
  discardInput();
  Serial.print(F(" y/n:"));
  do {
    while (!Serial.available()); // wait for input
    inChar = Serial.read() & 0b11011111; // fold case
  } while (inChar != 'Y' && inChar != 'N');
  Serial.println(inChar);
  return (inChar == 'Y');
}

void commandStart() {
  Serial.println(F("Get ready to press the HC-05 command mode button ..."));
  Serial.println(F("Press when LED lights; release when LED flashes."));
  discardInput();
  Serial.println(F("Ready? [any key]"));
  while (!Serial.available());
  discardInput();
  Serial.println(F("Go..."));
  digitalWrite(LED_BUILTIN, HIGH);
  delay(2000);
}

void commandStop() {
  Serial.flush();
  for (uint8_t a=0; a<11; a++) { //five flashes
    delay(100);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); 
  }
  delay(2000);
}

size_t tinyReadBytesUntil(char terminator, char *buffer, /*size_t*/ uint8_t length, long timeout) {
  /*size_t*/ uint8_t index = 0;
  while (index < length) {
//    int c = timedRead();

    int c;
    unsigned long _startMillis = millis();
    do {
      c = Serial.read();
      if (c >= 0) break;
    } while(millis() - _startMillis < timeout);
//    return -1;     // -1 indicates timeout
    
    if (c < 0 || c == terminator) break;
    *buffer++ = (char)c;
    index++;
  }
  return index; // return number of characters, not including null terminator
}
void setup()
{
  // TODO benefit?
  pinMode(serialRxPin, INPUT);      // make sure serial in is a input pin
  digitalWrite (serialRxPin, HIGH); // pull up enabled just for noise protection
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite (LED_BUILTIN, LOW);
  
  // blast at all rates - should display one recognizable number amidst noise
  // TODO rateIdx?
  for (uint8_t a = 0; a < numRates; a++)
  {
    Serial.begin(rateParms[a].bps);
    Serial.println();
    Serial.println();
    Serial.print(F("This is "));
    Serial.print(rateParms[a].bps, DEC);
    Serial.println(F(" bps. Type something. 'U' is robust.")); //'U' = x10101010101x (8N)
    Serial.println();
    Serial.end();
    delay(2); // seems to help the next slower rate start esp. for slowest rates TODO really?
  }

  /*
   * Detect bit rate
   * loop
   * * loop
   * * * pulsein() to measure shortest of several line state periods
   * * compare with expected bitTimes +/- 1/16
   * until a measured bit period matches
   */
#ifdef DEBUG
do {
  bps=0;
#endif

  do {
    bitTime = rateParms[numRates - 1].uSec * 2; // longer than any accepted bit duration
    for (uint8_t a = 0; a < samples; a++)
    {
#ifndef ARDUINO_AVR_DIGISPARK
      cli(); // yes, disable interrupts. since ~2015.
#endif       // but Digispark core uses old style pulseIn()
      long temp = pulseIn(serialRxPin, HIGH, bitTime);  // measure the next mark bit width - up to shortest already recorded
#ifndef ARDUINO_AVR_DIGISPARK
      sei();
#endif
      if (temp)
        bitTime = temp; // only decrease - timeout limits detections to no longer than shortest so far
    }
    
    uint8_t a = 0;
    while (a != numRates && bitTime > rateParms[a].uSec + (rateParms[a].uSec >> USECVAR ? rateParms[a].uSec >> USECVAR : 1 /*hack: 230400 often 5uSec*/)) a++;
    if (a < numRates && bitTime >= rateParms[a].uSec - (rateParms[a].uSec >> USECVAR))
      bps = rateParms[a].bps;

#ifdef DEBUG
    if (bitTime != rateParms[numRates - 1].uSec * 2) {
      Serial.begin(DEBUGBPS);
      Serial.println();
      Serial.print(F("bitTime "));
      Serial.println(bitTime, DEC);
      Serial.print(F("bps "));
      Serial.println(bps, DEC);
      Serial.end();
      delay(250);
    }
#endif

  } while (!bps); // i.e. until bit rate found within range

#ifdef DEBUG
} while (true);
#endif


  Serial.begin(bps);
  Serial.println();
  Serial.println();
  Serial.print(F("Hello at "));
  Serial.print(bps, DEC);
  Serial.println(F(" bps"));
  Serial.print(F("Measured bit duration: "));
  Serial.print(bitTime);
  Serial.println(F(" uSec"));
  

  Serial.println();
  Serial.println(F("baudat HC-05 configutron"));
  Serial.println();

  Serial.print(F("Set name|polar|bps?"));
  if (yorn()) {
  

    Serial.print(F("Update Bluetooth device name?"));
    if (yorn()) {
      Serial.println();
      discardInput();
      Serial.print(F("New name: "));

/*
      // There's got to be a better way to read a string with length variable up to a limit...
      chrPtr = -1; // before first because will first increment
      do {
        chrPtr++;
        while (!Serial.available()); // may be getting characters slowly as typed
        textBuffer[chrPtr] = Serial.read();
        if (textBuffer[chrPtr] == '\r' || textBuffer[chrPtr] == '\n')
          textBuffer[chrPtr] = '\0';
        else if (chrPtr == maxNameLen - 1)
          textBuffer[++chrPtr] = '\0'; // terminate automagically at maximum length
// TODO        else
//          Serial.write(textBuffer[chrPtr]);
      } while (textBuffer[chrPtr] != '\0'); // i.e. until terminated
*/

//      Serial.setTimeout(60000);
//      textBuffer[Serial.readBytesUntil('\r', textBuffer, maxNameLen)] = '\0';
textBuffer[tinyReadBytesUntil('\r', textBuffer, maxNameLen, 60000)] = '\0';
      Serial.println(textBuffer);
    } else textBuffer[0] = '\0'; // null name = no change
  
    Serial.println();
    
    Serial.print(F("Update connection status polarity?"));
    if (yorn()) {
      Serial.println();
      Serial.print(F("When connected, set status signal LOW or HIGH? 0/1:"));
      do {
        while (!Serial.available()); // wait for input
        polarity = Serial.read();
      } while (polarity != '0' && polarity != '1'); // i.e. until recognized input
      Serial.write((char) polarity);
    } else
      polarity = '\0'; // nul = no change
  
    Serial.println();

    Serial.println(F("Select new serial speed"));
    for (uint8_t a = 0; a < numRates; a++) {
      Serial.print((char) (a + 'a'));
      Serial.print(F(": "));
      Serial.println(rateParms[a].bps);
    }
    bps = 0; // re-use
    do {
      while (!Serial.available()); // wait for input
      uint8_t speed = (Serial.read() - 'A') & 0b11011111; // fold case
      if (speed >= 0 && speed < numRates)
        bps = rateParms[speed].bps;
    } while (bps == 0);
  
    Serial.println();
    Serial.println(F("New parameters"));
    if (textBuffer[0]) {
      Serial.print(F("Name: "));
      Serial.println(textBuffer);
    }
    if (polarity) {
      Serial.print(F("Connected state signal: "));
      Serial.println((char) polarity);
    }
    Serial.print(F("speed "));
    Serial.println(bps, DEC);
    Serial.println();

/*    Serial.println(F("Get ready to press the HC-05 command mode button for ~5 seconds..."));
    discardInput();
    Serial.println(F("ready?"));
    while (!Serial.available());
    discardInput();
    Serial.println(F("go..."));
    delay(2000);
*/

    commandStart();
    
    if (textBuffer[0]) {
      Serial.print(F("AT+NAME="));
      Serial.println(textBuffer);
      Serial.flush();
      delay(100);
    }
    if (polarity) {
      Serial.print(F("AT+POLAR=1,"));
      Serial.println((char) polarity);
      Serial.flush();
      delay(100);
    }
    Serial.print(F("AT+UART="));
    Serial.print(bps);
    Serial.println(F(",0,0"));

    commandStop();
    
    while(true) {
      delay(1000);
      Serial.println(F("power cycle/reset HC-05"));
    }
  }
}





void loop()
{
  Serial.print(F("Enter command: AT"));
  discardInput();
//  Serial.setTimeout(LONG_MAX);
//  textBuffer[Serial.readBytesUntil('\r', textBuffer, textBufferSize-1)] = '\0';
textBuffer[tinyReadBytesUntil('\r', textBuffer, textBufferSize-1, LONG_MAX)] = '\0';
  Serial.println(textBuffer);
  commandStart();
  discardInput();
  Serial.print(F("AT"));
  Serial.println(textBuffer);
  Serial.flush();
//  Serial.setTimeout(1000);
//  textBuffer[Serial.readBytesUntil('\0', textBuffer, textBufferSize-1)] = '\0'; // not expecting \0; using readBytesUntil to avoid adding readBytes to code size
textBuffer[tinyReadBytesUntil('\0', textBuffer, textBufferSize-1, 1000)] = '\0';
  commandStop();
  Serial.println(F("Result:"));
  Serial.println(textBuffer);
  Serial.println();
  
}
