/*
  hcofiver TODO hcbtat
  * OTA config tool for HC-05 or similar Bluetooth SPP modules
  * autodetect serial bit rate
  * UI over Bluetooth connection
  * compatible with Digispark (ATtiny85)
  * suitable for dedicated standalone device
  Copyright 2019 Paul McClay <mcclay at g mail>
  MIT License https://opensource.org/licenses/MIT

  Detect serial bit rate
  Using detected rate, prompt for new rate
  Configure HC-05 or similar for new rate

  Bit rate detection method derived from example by retrolefty:
  * Loop pulsein() to measure shortest of n line state periods
  * Compare with static list of geometric means of bit times for canonical rates
  * Favor "U" for test character
  There are no invalid rates within min-max range; 3300 is 2400 and 3400 is 4800
  Rate breaks are approximated to round numbers. Doubtful that anything critically close to a rate break would work either way.
  https://forum.arduino.cc/index.php?topic=98911.15 posted 30 March 2012
 */


#ifdef ARDUINO_AVR_DIGISPARK // generalize to other ATtiny?
// #include <SoftSerial_INT0.h> // smaller
#include <SoftSerial.h> // this fits now
const uint8_t serialRxPin = 2; // also compatible with Digispark_SoftSerial-INT0
SoftSerial mySerial(2, 0);
#define Serial mySerial

#else // not a Digispark
// assume hardware serial with receive on pin 0
const uint8_t serialRxPin = 0;
#endif

long bps;
// TODO #define temp bps // overload bps for memory frugality
int bitTime; // in micros
const uint8_t samples = 8; // number of mark/1/high bits to measure
                           // any >1 non-extended ASCII chars will have 010 msb-stop-start sequence
                           // which will work if stop is one bit time
                           // send 0x0000 to time stop bit
const int patience = 1000; // millis allowance for human reaction
const uint8_t maxNameLen = 32; // max 32 chars per at least one reference
char deviceName[maxNameLen + 1]; // +1 nul terminator
int8_t chrPtr;
#define polarity chrPtr // overload chrPtr;

struct rateParms {
  long bps; // "standard" bit rate, or fault flag
  int maxMicros; // threshold bit duration between this and next slower bit rate
};

const struct rateParms rateParms[] {
  { -1, 6}, // too fast or noisy
  {115200, 12},
  {57600, 20},
  {38400, 30},
  {28800, 40},
  {19200, 60},
  {14400, 90},
  {9600, 150},
  {4800, 300},
  {2400, 600}
};

const uint8_t numRates = (sizeof(rateParms) / sizeof(struct rateParms)) - 1; // n-1 non-error rates

void discardInput() {
  delay(5); // follow-on bytes can be slow
  while (Serial.available()) {
    Serial.read();
    delay(5);
  }
}

void setup()
{
  // blast out all the preferred rates - should display one recognizable number amidst noise
  // TODO rateIdx?
  for (uint8_t a = 1; a <= numRates; a++) // start after first; end before last // TODO or skip first
  {
    Serial.begin(rateParms[a].bps);
    Serial.println();
    Serial.println();
    Serial.print(F("This is "));
    Serial.print(rateParms[a].bps, DEC);
    Serial.println(F(" bps. Type something. 'U' is robust."));
    Serial.println();
    Serial.end();
    delay(2); // seems to help the next slower rate start esp. for slowest rates TODO how
  }

  pinMode(serialRxPin, INPUT);      // make sure serial in is a input pin
  digitalWrite (serialRxPin, HIGH); // pull up enabled just for noise protection

  do {
    bitTime = rateParms[numRates].maxMicros; // longer than any accepted bit duration
    for (uint8_t a = 0; a < samples; a++)
    {
      cli(); // yes, disable interrupts. since ~2015.
      long temp = pulseIn(serialRxPin, HIGH, bitTime);  // measure the next mark bit width - up to shortest already recorded
      sei();
      if (temp)
        bitTime = temp; // only decrease - timeout limits detections to no longer than shortest so far
    }
    if (bitTime == rateParms[numRates].maxMicros)
      bps=0;
    else {
      uint8_t a = 0;
      while (bitTime >= rateParms[a].maxMicros && a < numRates) a++; // scan for nearest preferred rate
      bps = rateParms[a].bps;
    }
  } while (bps < 1); // i.e. until bit rate found within range

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
  Serial.println(F("hcofiver HC-05 configutron"));
  Serial.println();

  discardInput();
  Serial.print(F("Update Bluetooth device name? y/n:"));
  while (!Serial.available());
  char inChar = Serial.read();
  if (inChar == 'y' || inChar == 'Y') {
    Serial.println();
    discardInput();
    Serial.print(F("New name:"));

    // There's got to be a better way to read a string with length variable up to a limit...
    chrPtr = -1; // before first because will first increment
    do {
      chrPtr++;
      while (!Serial.available()); // may be getting characters slowly as typed
      deviceName[chrPtr] = Serial.read();
      if (deviceName[chrPtr] == '\r' || deviceName[chrPtr] == '\n')
        deviceName[chrPtr] = '\0';
      else if (chrPtr == maxNameLen - 1)
        deviceName[++chrPtr] = '\0'; // terminate automagically at maximum length
      else
        Serial.write(deviceName[chrPtr]);
    } while (deviceName[chrPtr] != '\0'); // i.e. until terminated
  } else deviceName[0] = '\0'; // null name = no change

  Serial.println();
  discardInput();
  Serial.print(F("Update connection status polarity? y/n:"));
  while (!Serial.available());
  inChar = Serial.read();
  if (inChar == 'y' || inChar == 'Y') {
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
  discardInput();
  Serial.println("Select new serial speed");
  for (uint8_t a = 1; a <= numRates; a++) {
    Serial.print((char) (a + 'a' - 1));
    Serial.print(": ");
    Serial.println(rateParms[a].bps);
  }
  bps = 0; // re-use
  do {
    while (!Serial.available()); // wait for input
    uint8_t speed = (Serial.read() - 'A' + 1) & 0b11011111; // fold case
    if (speed > 0 && speed <= numRates)
      bps = rateParms[speed].bps;
  } while (bps == 0);

  Serial.println();
  Serial.println(F("New parameters"));
  if (deviceName[0]) {
    Serial.print(F("Name: "));
    Serial.println(deviceName);
  }
  if (polarity) {
    Serial.print(F("Connected state signal: "));
    Serial.println((char) polarity);
  }
  Serial.print(F("speed "));
  Serial.println(bps, DEC);
  Serial.println();

  Serial.println(F("Get ready to press the HC-05 command mode button for ~5 seconds..."));
  discardInput();
  Serial.println("ready?");
  while (!Serial.available());
  discardInput();
  Serial.println("go...");
  delay(2000);
  if (deviceName[0]) {
    Serial.print("AT+NAME=");
    Serial.println(deviceName);
    Serial.flush();
    delay(100);
  }
  if (polarity) {
    Serial.print("AT+POLAR=1,");
    Serial.println((char) polarity);
    Serial.flush();
    delay(100);
  }
  Serial.print("AT+UART=");
  Serial.print(bps);
  Serial.println(",0,0");
}





void loop()
{
  delay(1000);
  Serial.println("power cycle/reset HC-05");
}
