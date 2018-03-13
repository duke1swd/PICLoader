/*
 * This sketch is a program to program a PIC chip using
 * an Arduino.  There are two basic problems to solve.
 * First, communicating with the computer that has the
 * PIC tool chain, second programming the PIC.
 *
 * A very simple protocol over the serial port solves
 * the first problem.
 *
 * The programming code was tested on a PIC 12F1822 using
 * a particular in-circuit, low-voltage method.  Modifications
 * might well be required for any other PIC or circuit.
 *
 * This program does very low-level chip manipulation.
 * The higher level functions and flow charts are implemented
 * by the program running on the host computer.  This
 * results in a very slow programming style as the host
 * and the Arduino talk a lot.  However, it works.
 *
 * If you uncomment the #define TESTPIC line you get a standalone
 * program that just verifies we can talk to the PIC.
 */

// #define  TESTPIC  1
 
#define  DEFINE_COMMANDS
#include "commands.h"
 
/*
 * PIN ASSIGNMENTS
 */
#define  PIN_LED  13

#define  PIN_PIC_ICSPCLK  3    // this is pin 6 on the PIC
#define  PIN_PIC_ICSPDAT  5    // this is pin 7 on the PIC
#define  PIN_PIC_MCLR     7    // this is pin 4 on the PIC

/*
 * In addition to the 3 pins above, PIC pin 1 is VDD
 * and PIC pin 8 is VSS (gnd)
 */

/*
 * Protocol:
 * The host sends commands.  The arduino responds. All commands are 1 letter.
 * Some commands have a parameter.  Parameters are hex numbers terminated by
 * a newline.
 *
 * Here are the commands:
 *  A  start the handshake.  The response is "B\n".
 *  I  go to idle mode.  This completes the three way handshake.  No response.
 *  E  force the PIC into programming mode.  Response is either "Y\n" or "N\n".
 *  X  good bye.  Go back to awaiting a handshake.
 *
 * All of the folowing are programming commands.  Only available after "E"
 * command.  All respond with "!\n" when they complete.
 *
 *  a  Load Configuration
 *  b  Load Data for Program Memory
 *  c  Load Data for Data Memory
 *  d  Read Data from Program Memory
 *  e  Read Data from Data Memory
 *  f  Increment Address
 *  g  Reset Address
 *  h  Begin Programming
 *  k  Bulk Erase Program Memory
 *  l  Bulk Erase Data Memory
 *  x  Exit programming mode.
 */


/*
 * The protocol is stateful.  Here are the states.
 */
#define  P_S0    0  // not yet connected.
#define  P_C1    1  // got the init handshake, waiting for the second
#define  P_CON   2  // connected.  Awaiting a command.
#define  P_PROG  3  // PIC is in programming mode.

byte state;
byte c;  // the current command

static void
blnk(int n)
{
    for (; n > 0; n--) {
        digitalWrite(PIN_LED, HIGH);
        delay(200);
        digitalWrite(PIN_LED, LOW);
        delay(200);
    }
    delay(1000);
}

void resetPIC()
{
  digitalWrite(PIN_PIC_MCLR, LOW);  // reset the PIC
  delayMicroseconds(10);
  pinMode(PIN_PIC_ICSPCLK, OUTPUT);
  digitalWrite(PIN_PIC_ICSPCLK, LOW);
  pinMode(PIN_PIC_ICSPDAT, OUTPUT);
  digitalWrite(PIN_PIC_ICSPDAT, LOW);
}

void releasePIC()
{
  pinMode(PIN_PIC_ICSPCLK, INPUT);
  pinMode(PIN_PIC_ICSPDAT, INPUT);
  digitalWrite(PIN_PIC_MCLR, HIGH);
}
  
void setup() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_PIC_MCLR, OUTPUT); 
  resetPIC();
  delay(10);
  releasePIC();
  
  Serial.begin(9600);
  state = P_S0;
}

/*
 * Sends LSB first.
 * Assumes the data pin is in output mode.
 *
 * Clock is low on entry and on exit.
 * Sends exactly "bits" number of complete clock pulses.
 */
void
sendToPic(int bits, unsigned int val)
{
  byte i;
  byte b;
  
  for (i = 0; i < bits; i++) {
    b = val & 0x1;
    val >>= 1;
    
    digitalWrite(PIN_PIC_ICSPCLK, HIGH);
    digitalWrite(PIN_PIC_ICSPDAT, b);
    delayMicroseconds(3);                // requirement is that data is stable on the falling clock.
    digitalWrite(PIN_PIC_ICSPCLK, LOW);
    delayMicroseconds(3);
  }
}

/*
 * Get some bits from the PIC.
 * Presumed to come off LSB first.
 */
unsigned int
getFromPic(int bits)
{
  byte i;
  byte b;
  unsigned int value, tv;
  
  value = 0;
  
  pinMode(PIN_PIC_ICSPDAT, INPUT);
  
  for (i = 0; i < bits; i++) {
    digitalWrite(PIN_PIC_ICSPCLK, HIGH);
    delayMicroseconds(3);
    digitalWrite(PIN_PIC_ICSPCLK, LOW);
    delayMicroseconds(3);
    tv = 0;
    if (digitalRead(PIN_PIC_ICSPDAT))
      tv = (1 << i);
    value |= tv;
  }
  pinMode(PIN_PIC_ICSPDAT, OUTPUT);
  
  return value;
}


/*
 * Try to put the PIC into programming mode.
 */
void enterProgramMode()
{
  resetPIC();
  sendToPic(8, 'P');
  sendToPic(8, 'H');
  sendToPic(8, 'C');
  sendToPic(8, 'M');
  sendToPic(1, 0);    // one more clock pulse needed

// OK, it should be in programming mode.
  Serial.println("Y");
}

void copySignal()
{
  byte b;
  blnk(5);
  delay(1000);
  
    pinMode(PIN_PIC_ICSPDAT, INPUT);
    pinMode(PIN_PIC_ICSPCLK, INPUT);
    for (;;) {
      b = digitalRead(PIN_PIC_ICSPDAT);
      digitalWrite(PIN_LED, b);
    }
}

/*
 * This routine processes most commands.
 */
void command() {
  switch (c) {
    case 'X':
      state = P_S0;
      releasePIC();
      copySignal();
      /* NOTREACHED* */
      break;
    case 'E':
      enterProgramMode();
      state = P_PROG;
      break;
    case 'R':
      releasePIC();
      break;
    default:    // on communications error, go back to state 0, but don't let the PIC run.
      state = P_S0;
      break;
  }
}

/*
 * Handle programming commands.
 */

byte
getHexC()
{
    int c;
    
    for (;;) {
      while (!Serial.available())
        ;
      c = (Serial.read() & 0x7f);
    
      if (c >= '0' and c <= '9')
        return c - '0';
      
      if (c >= 'a' and c <= 'f')
        return c - 'a' + 10;
    
      if (c >= 'A' and c <= 'F')
        return c - 'A' + 10;
/*xxx*/blnk(2);
    }
}
      

unsigned int
read_word_from_serial()
{
  unsigned int value;
  
  value = getHexC();
  value <<= 4;
  value |= getHexC();
  value <<= 4;
  value |= getHexC();
  value <<= 4;
  value |= getHexC();
  
  return value;
}
 
void sendCmd(byte cmd) {
  pinMode(PIN_PIC_ICSPDAT, OUTPUT);
  sendToPic(6, cmd);
}

void sendWord(byte cmd) {
  unsigned int value;
  byte b;
  sendCmd(cmd);
  
  value = read_word_from_serial();
  b = (value & 0x7f) << 1;
  sendToPic(8, b);
  b = (value >> 7) & 0x7f;
  sendToPic(8, b);
}

void readWord(byte cmd) {
  unsigned int value;
  
  sendCmd(cmd);
  value = 0;
  // get the word from the PIC
  value = getFromPic(16);
  value >>= 1;
  value &= 0x3fff;
  
  Serial.print((value >> 12) & 0xf, HEX);
  Serial.print((value >>  8) & 0xf, HEX);
  Serial.print((value >>  4) & 0xf, HEX);
  Serial.print( value        & 0xf, HEX);
}

void
programming_command()
{
  switch(c) {
    // Load Configuration
    case 'a':
      sendWord(0x0);
      break;
      
    // Load Data for Program Memory
    case 'b':
      sendWord(0x2);
      break;
      
    // Load Data for Data Memory
    case 'c':
      sendWord(0x3);
      break;
      
    // Read Data from Program Memory
    case 'd':
      readWord(0x4);
      break;
          // Read Data from Data Memory
    case 'e':
      readWord(0x5);
      break;
      
    // Increment Address
    case 'f':
      sendCmd(0x6);
      break;
    
    // Reset Address
    case 'g':
      sendCmd(0x16);
      break;
    
    // Begin Programming
    case 'h':
      sendCmd(0x8);
      // need to wait some here
      delay(5);
      break;
    
    // Bulk Erase Program Memory
    case 'k':
      sendCmd(0x9);
      // need to wait some here
      delay(5);
      break;
    
    // Bulk Erase Data Memory
    case 'l':
      sendCmd(0xb);
      // need to wait some here
      delay(5);
      break;
      
    // Exit programming mode.
    case 'x':
      state = P_CON;
      break;
      
    // default case is an error.  
    default:
      state = P_S0;
      return;
  }
  Serial.println("!");
}

#ifdef TESTPIC

static void
flag(char *s)
{
    Serial.println(s);
}

static void
testpic()
{
  unsigned int value;
  char buffer[32];
  byte r;

  Serial.println("Testing PIC Programming.  Hit Enter to start.");
  
  do {
    r = Serial.readBytesUntil('\n', buffer, 32);
  } while (r < 1 || r >= 32);
 Serial.println("Programming"); 
  enterProgramMode();
  sendCmd(0x9);  // bulk erase
  delay(10);
/*xxx*/flag("-- A");
  // sequence is to load 1 word of program memory is 0x1555, then read it back.
  sendCmd(0x2);  // load data for program memory
  sendToPic(16, 0x2AAA);
  sendCmd(0x8);  // begin programming
  delay(10);      // wait for programming to complete.
  //sendCmd(0x16); // reset address
/*xxx*/flag("-- B");
  sendCmd(0x4);  // read data from program memory
  value = getFromPic(16);
  Serial.println("Value returned is");
  Serial.print(value, HEX);
  Serial.println(" Done");
  releasePIC();
}
#endif

void loop() {
  byte b;
  
#ifdef TESTPIC
  testpic();
  if(1)return;
#endif

  if (!Serial.available()) {
    // if we are not doing anything else, reflect the state of the ICSPDAT pin.
    if (state == P_S0) {
      pinMode(PIN_PIC_ICSPDAT, INPUT);
      b = digitalRead(PIN_PIC_ICSPDAT);
      digitalWrite(PIN_LED, b);  
    }
    return;
  }
  c = Serial.read();

  if (c == '\n' || c == '\r' || c == ' ')
    return;
  switch (state) {
    case P_S0:
      if (c != 'A')
        break;
/*xxx*/digitalWrite(PIN_LED, HIGH);
    Serial.println("B");
    state = P_C1;
    break;
  case P_C1:
    digitalWrite(PIN_LED, LOW);
    if (c == 'I')
      state = P_CON;
    else
      state = P_S0;
    break;
  case P_CON:
    digitalWrite(PIN_LED, HIGH);
    command();
    break;
  case P_PROG:
    digitalWrite(PIN_LED, LOW);
    programming_command();
    break;
  }
}
