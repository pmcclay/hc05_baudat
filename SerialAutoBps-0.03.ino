// derived from retrolefty's auto baud - posted 30 March 2012
// https://forum.arduino.cc/index.php?topic=98911.15
// https://forum.arduino.cc/index.php?topic=98911.msg742530#msg742530
/*
  First stab at a auto baudrate detection function. This uses the pulseIn command
  to determine what speed an unknown serial link is running at. Detects and sets the standard baud
  rates supported by the Arduino IDE serial monitor. Uses character "U" as a test character

  I'm sure characters with multible zero bits in a row may fake out proper detection. If data is
  garbled in the serial monitor then the auto baud rate function failed.

  This is just a demo sketch to show concept. After uploading the sketch, open the serial monitor
  and pick a random baudrate and then start sending U charaters. The monitor will print out
  what baudrate it detected. It will default to 9600 baud if found a bit
  width too long for a standard rate used by the serial monitor. Note that as written, this is a
  'blocking' function that will wait forever if no character are being sent.

  Note that while the serial monitor has a 300 baud option, the Arduino hardware serial library
  does not seem to support that baud rate, at least for version 22, in my testing.

  By "retrolefty" 1/22/11
*/

#ifdef ARDUINO_AVR_DIGISPARK // generalize to other ATTiny?
#include <SoftSerial_INT0.h>
const uint8_t recPin = 2; // also used elsewhere
SoftSerial mySerial(2, 0); // Rx, Tx TODO
#define Serial mySerial
#else // not a DigiSpark
// assume hardware serial with receive on pin 0
const uint8_t recPin = 0;
#endif

long bps;   // global in case useful elsewhere in a sketch
#define temp bps // overload bps for memory frugality
int bitTime; // micros - also global
const uint8_t samples = 8; // number of mark bits to measure
const int patience = 1000;
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
  {2400, 600},
  {1200, 1200},
  {0, 2400} // too slow
};

const uint8_t numRates = sizeof(rateParms) / sizeof(struct rateParms);

void discardInput() {
  delay(1); // time to receive a byte at 1200 bps
  while (Serial.available()) {
    Serial.read();
    delay(1);
  }
}

void setup()
{
  // blast out all the preferred rates - should display one recognizable number amidst noise
  for (uint8_t a = 1; a <= numRates; a++) // start after first; end before last
  {
    Serial.begin(rateParms[a].bps);
    Serial.println();
    // terse, otherwise slow to send at several slower rates
    Serial.print(rateParms[a].bps, DEC);
    Serial.println(F(" send 'U'"));
    Serial.end();
    delay(2); // seems to help the next slower rate start esp. for slowest rates TODO how
  }

  pinMode(recPin, INPUT);      // make sure serial in is a input pin
  digitalWrite (recPin, HIGH); // pull up enabled just for noise protection

  do {
    bitTime = rateParms[numRates - 1].maxMicros + 1; // longer than any time we care about
    while (digitalRead(recPin) == HIGH) {} // wait for input low
    for (uint8_t a = 0; a < samples; a++)
    {
      cli(); // yes, disable interrupts. doc is rwong. this works.
      temp = pulseIn(recPin, HIGH, bitTime);  // measure the next mark bit width - up to shortest already recorded
      sei();
      if (temp > 0) // if not too long (0) or too short (-1)
        bitTime = temp; // timeout limits detections to no longer than shortest so far
    }
    uint8_t a = 0;
    while (bitTime > rateParms[a].maxMicros && a < numRates - 1) a++; // scan for nearest preferred rate
    bps = rateParms[a].bps;
  } while (bps < 1); // i.e. until bit rate found within range

  Serial.begin(bps);
  Serial.println();
  Serial.print(F("Hello at "));
  Serial.print(bps, DEC);
  Serial.println(F(" bps"));
  Serial.print(F("Measured bit duration: "));
  Serial.print(bitTime);
  Serial.println(F(" uSec"));


  Serial.println();
  Serial.println(F("HC-05 configutron"));
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
  for (uint8_t a = 1; a < numRates - 1; a++) {
    Serial.print((char) (a + 'a' - 1));
    Serial.print(": ");
    Serial.println(rateParms[a].bps);
  }
  bps = 0; // re-use
  do {
    while (!Serial.available()); // wait for input
    uint8_t speed = Serial.read() - 'a' + 1;
    if (speed < numRates)
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
  /*delay(100);
  Serial.println("AT+RESET"); //doesn't work on at least one HC-05*/
  delay(1000);
}





void loop()
{
  //would be more cool to switch to new rate and say so -- if AT+RESET worked
  delay(1000);
  Serial.println("power cycle/reset HC-05");

  /*if (Serial.available()) {
    // get incoming byte:
    int inByte = Serial.read();
    Serial.println(inByte, BIN);
    }*/
}
