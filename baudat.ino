/*
  hc05-baudat -- an OTA config tool for HC-05 or similar Bluetooth SPP modules
  Set baud & issue AT commands, with a hat tip to Émile Baudot
  * UI over Bluetooth connection
  * immediately indicate serial/UART bit rate/bps
  * autodetect serial bit rate
  * compatible with Digispark (ATtiny85)
  * suitable for dedicated device
  * e.g. TODO hw project link

  Setup:
    Send banner at all rates -- legible banner indicates current rate
    Detect serial bit rate
    Begin Serial stream at detected rate
    Ask: guided prompts for new name/polarity/rate?
      Prompt for new info
      Prompt user to switch HC-05 to command mode
      Configure HC-05 -- new bit rate effective after module reset
      loop forever: at old bit rate prompt to reset HC-05

  Loop:   // reached only if user declines guided config
    Prompt for AT command
    Prompt user to switch HC-05 to command mode
    Send command to HC-05
    Read/Store response from HC-05
    Prompt user to release HC-05 command mode
    Display stored HC-05 response

  Readme at github TODO

  Copyright 2019 Paul McClay <mcclay at g mail>
  MIT License & disclaimer https://opensource.org/licenses/MIT
*/

//#define BITTIMER // uncomment to loop reporting detected bit times & rates
#define BITTIMERBPS 57600

#ifdef ARDUINO_AVR_DIGISPARK // Runs on Digispark ATtiny85

// The bootloaders on early/real Digispark boards may calibrate clock only when connected to a real USB host.
// See https://digistump.com/wiki/digispark/tricks "Detect if system clock was calibrated".
// The "aggressive" micronucleus bootloader does not calibrate the clock.

#define LED_BUILTIN 1 // early units may have LED on pin 0

// Use Digispark_SoftSerial-INT0 library
// https://github.com/J-Rios/Digispark_SoftSerial-INT0/archive/master.zip
#include <SoftSerial_INT0.h> // This one goes to 11 (11.52 myriabits/sec) and saves ~240 bytes
// #include <SoftSerial.h> // Supplied in IDE, but limited to 57.6k & too big
const uint8_t serialRxPin = 2; // required by Digispark_SoftSerial-INT0
SoftSerial mySerial(serialRxPin, 0); // or TX on pin 1 if LED on 0
#define Serial mySerial

const uint8_t textBufferSize = _SS_MAX_RX_BUFF; //TODO +1?
#define USECVAR 3 // accept +/- 1/(2^3) variation of bit time in uSec
                  // Digispark clock may vary. If marginal, usually at least 1st char received is good. HC-05s seem to receive marginal rates well
                  // TODO what was ^^^ about?



#else // not ARDUINO_AVR_DIGISPARK



// assume hardware serial with receive on pin 0
const uint8_t serialRxPin = 0;
const uint8_t textBufferSize = SERIAL_RX_BUFFER_SIZE;
#define USECVAR 4 // accept +/- 1/(2^4) variation of bit time
                  // Expect accurate clock. 5 would likely work.


#endif // not ARDUINO_AVR_DIGISPARK


#include <limits.h>

long bps = 0;
int bitTime; // microseconds
const uint8_t samples = 8; // number of mark/1/high bits to try to measure
                           // any >1 non-extended ASCII chars will have 010 msb-stop-start sequence
                           // assuming no parity & stop bit is one bit time
                           // send 0x0000 to time stop bit
const int patience = 1000; // millis allowance for human reaction TODO tie more delays to this
const uint8_t maxNameLen = 32; // max 32 chars per at least one reference

char textBuffer[textBufferSize];
int8_t chrPtr; //TODO why not unsigned?
uint8_t polarity;


struct rateParms {
  long bps; // "standard" bit rate
  int uSec; // bit duration TODO compare code size to calculate from bps
};

const struct rateParms rateParms[] {
	
#ifndef ARDUINO_AVR_DIGISPARK // don't let Digispark set a speed it can't reconnect to
  {500000, 2}, // possible with hw serial at 16MHz but not commonly supported by SPP modules
//{460800, 2}, // commonly supported by SPP modules but not possible with hw serial at 16MHz
  {230400, 4},
#endif

#if !defined ARDUINO_AVR_DIGISPARK || defined SoftSerialINT0_h
  {115200, 9}, // ok for not-Digispark or Digispark with SoftSerial_INT0 (SS_INT0 receive not bulletproof - go slower to set a long bt name)
#endif

  {57600, 17},
  {38400, 26},
//  {28800, 35}, // not used by known SPP modules
  {19200, 52},
//  {14400, 69}, // not used by known SPP modules
  {9600, 104},
  {4800, 208},
  {2400, 417}
};

const uint8_t numRates = (sizeof(rateParms) / sizeof(struct rateParms));

///////////////////////////

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
  Serial.print(F(" [y/n] "));
  do {
    while (!Serial.available());
    inChar = Serial.read() & 0b11011111; // fold case
  } while (inChar != 'Y' && inChar != 'N');
  Serial.println(inChar);
  return (inChar == 'Y');
}

void commandStart() {
  Serial.println(F("Get ready to press HC-05 command mode button ..."));
  Serial.println(F("Press when LED lights; release when LED flashes."));
  discardInput();
  Serial.println(F("Ready? [any key]"));
  while (!Serial.available());
  discardInput();
  Serial.println();
  Serial.println(F("Go..."));
  digitalWrite(LED_BUILTIN, HIGH);
  delay(2000);
}

void commandStop() {
  Serial.flush();
  for (uint8_t a=0; a<11; a++) { //five flashes then off (if on at start)
    delay(100);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); 
  }
  delay(2000);
}

uint8_t readText(char *buffer, uint8_t length, unsigned long timeout) {
  // - read printable text up to next control character or timeout
  // - returns # characters read
  // - blunt solution for CR vs LF vs CRLF
  // - "printable" is loosely defined as ASCII values >= 32 (space)
  //   which should be good up to 126 then gets questionable
  // - single byte index assumes buffer <= 255 bytes (256 == 0)
  unsigned long startMillis = millis();
  uint8_t index = 0;
  int c;
  while (index < length) {
    do {
      c = Serial.read();
    } while ((c == -1) && (millis() - startMillis < timeout));
    if (c < 32) break; // control character or timeout
    buffer[index++] = (char)c;
  }
  return(index);
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
    Serial.println(); // new line after preceding noise
    Serial.println(); // blank line
    Serial.print(F("This is "));
    Serial.print(rateParms[a].bps, DEC);
    Serial.println(F(" bps. Type something. 'U' is robust.")); //'U' = x10101010101x (8N)
    Serial.println(); // blank line before follwing noise
    Serial.end();
    delay(2); // seems to help the next slower rate start esp. for slowest rates TODO really?
  }

  /*
   * Detect bit rate
   * loop
   * * loop
   * * * pulsein() to measure shortest of several line state periods
   * * compare with expected bitTimes +/- fraction
   * until a measured bit period matches
   */
#ifdef BITTIMER // just loop reporting detecting times/rates
  Serial.begin(BITTIMERBPS);
  Serial.println();
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

#ifdef BITTIMER
    if (bitTime != rateParms[numRates - 1].uSec * 2) {
      Serial.print(F("bitTime "));
      Serial.println(bitTime, DEC);
      Serial.print(F("bps "));
      Serial.println(bps, DEC);
      delay(500);
    }
#endif

  } while (!bps); // i.e. until bit rate found within range

#ifdef BITTIMER
} while (true); // BITTIMER never exits this loop
#endif


  Serial.begin(bps);
  Serial.println();
  Serial.println();
  Serial.print(F("Hello at "));
  Serial.print(bps, DEC);
  Serial.println(F(" bps"));
// TODO kill?
//  Serial.print(F("Measured bit duration: "));
//  Serial.print(bitTime);
//  Serial.println(F(" uSec"));
  

  Serial.println();
  Serial.println(F("baudat HC-05 configutron tool"));
  Serial.println();

  Serial.print(F("Set name|polar|bps?"));
  if (yorn()) {
  

    Serial.print(F("Update Bluetooth device name?"));
    if (yorn()) {
      Serial.println();
      discardInput();
      Serial.print(F("New name: "));
      textBuffer[readText(textBuffer, maxNameLen, ULONG_MAX)] = '\0';
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
#if !defined ARDUINO_AVR_DIGISPARK || defined SoftSerialINT0_h
  Serial.print(F("Enter command: AT"));
  discardInput();
//  textBuffer[tinyReadBytesUntil('\r', textBuffer, textBufferSize-1, ULONG_MAX)] = '\0'; // wait for text up to CR and add null terminator to make a string
  textBuffer[readText(textBuffer, textBufferSize-1, ULONG_MAX)] = '\0'; // read up to CR or LF and add null terminator to make a string
  Serial.println(textBuffer);
  commandStart();
  discardInput();
  Serial.print(F("AT"));
  Serial.println(textBuffer);
  Serial.flush();
//  textBuffer[tinyReadBytesUntil('\0', textBuffer, textBufferSize-1, 1000)] = '\0'; // not expecting \0; reusing rBUntil w/short timeout
  textBuffer[readText(textBuffer, textBufferSize-1, 1000)] = '\0'; // read for in ~1 second and add null terminator to make a string
  commandStop();
  Serial.println(F("Result:"));
  Serial.println(textBuffer);
  Serial.println();
#else // assuming Digispark with default SoftSerial library
  Serial.println("Requires SoftSerial-INT0 library"); // smaller lib leaves more space for code
  while (true){};
#endif
  
}
