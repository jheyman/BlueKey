#include <TermTool.h>
#include <SPI.h>
#include "eeprom.h"
#include "EncryptedStorage.h"
#include "Entropy.h"
#include "constants.h"
#include "display.h"

#define ledPin 13

byte getStringFromFlash(char* buffer, uint8_t* storedString) {

    uint8_t* ptr = storedString;
    byte msgLen=0;
    while(1)
    {     
      char c = pgm_read_byte(ptr++);
      if (c != 0) {
        buffer[msgLen++] = c;
      }
      else {
        buffer[msgLen] = 0;
        break;
      }
    }

    return msgLen;
}

////////////////////
// EEPROM Read/Write
////////////////////

#define EEPROM_I2C_ADDR  0x50

#define EEPROM_TEST_OFFSET 1280

void testEEPROM(int index) {

  byte writeBuffer[32];
  sprintf((char*)writeBuffer, (char*)F("Write test in EEPROM %d"), index);

  byte readbackBuffer[32];
  strcpy((char*)readbackBuffer, (char*)F("xxxxxxxx"));
  
  uint16_t offset = 0; 
  offset=I2E_Write(EEPROM_TEST_OFFSET, writeBuffer, 224 );

  I2E_Read(EEPROM_TEST_OFFSET, readbackBuffer, 224);

   display.clearDisplay();
   display.setCursor(0,0);
   display.print("OFF:");
   display.print(offset);
   display.print(",RB:");
   display.print((char*)readbackBuffer);   
   display.display(); 
}

////////////////////
// KNOB management
////////////////////
const byte knobInterruptPin = 3;
const byte knobDataPin = 12;
const byte knobSwitchPin = 2;

volatile int knobIncrements=0;

volatile bool knobIndexIncreased = false;
volatile bool knobIndexDecreased = false;

void uponKnobIT() {
  static unsigned long  lastInterruptTime = 0;
  unsigned long interruptTime = millis();

  // basic anti-glitch filtering
  if (interruptTime - lastInterruptTime > 10) {
    if (digitalRead(knobDataPin)) {
      knobIncrements++;
      knobIndexIncreased = true;
    }  
    else {
      knobIncrements--; 
      knobIndexDecreased = true;
    }
    
    lastInterruptTime = interruptTime;
  }
}

int lastButtonState = LOW;
int buttonState; 
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
bool knobSwitchPushed=false;

bool checkbuttonPushed() {

  bool ret=false;
  int reading = digitalRead(knobSwitchPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle the LED if the new button state is HIGH
      if (buttonState == HIGH) {
        ret = true;
      }
    }
  }

  lastButtonState = reading;
  return ret;
}

////////////////////
// MISC
////////////////////
void setRng()
{
  analogWrite(ledPin, 250);
//  randomSeed( Entropy.random() );
  digitalWrite(ledPin,1);
}

// Modulus function that handles negative numbers
int mod(int x, int m) {
    return (x%m + m)%m;
}

#define CODE_SLOT_CHAR '_'
#define CODE_HIDE_CHAR '*'

const static char CODE_INVITE[] PROGMEM = "CODE:";
const static char WHATEVER_INVITE[] PROGMEM = "Whatever:";
const static char NEW_CODE_MISMATCH[] PROGMEM = "[Error: mismatch]";
const static char NEED_FORMAT[] PROGMEM = "Need format before use";
const static char LOGIN_GRANTED[] PROGMEM = "[Granted]";
const static char LOGIN_DENIED[] PROGMEM = "[Denied]";

void __attribute__ ((noinline)) getCodeFromUser(char* dst, const uint8_t numChars, const char *invite, int firstAsciiChar, int lastAsciiChar)
{
    int nbChars = lastAsciiChar - firstAsciiChar +1;
    int charIndex = 0;
  
    int cursorX = (DISPLAY_WIDTH - (numChars+strlen(invite))*CHAR_XSIZE)/2;
    int cursorY = (DISPLAY_HEIGHT - CHAR_YSIZE)/2;

    int nbCharsValidated = 0;
    char c = CODE_SLOT_CHAR;

    display.clearDisplay();    
    display.setCursor(cursorX,cursorY);
    display.print(invite);
       
    cursorX = display.getCursorX();
    for (int n=0; n<numChars; n++) {
      display.print(CODE_SLOT_CHAR);
    }
      
    display.display();
            
    // Require numChars input characters from user
    while (1) {

      // If knob switch was pushed, validate current char
      if (checkbuttonPushed()) {
        if (c != CODE_SLOT_CHAR)
        {
          dst[nbCharsValidated] = c;
          nbCharsValidated++;
          if (nbCharsValidated == numChars) break;
          display.setCursor(cursorX,cursorY);
          display.print(CODE_HIDE_CHAR);
          display.display();
          cursorX+=CHAR_XSIZE;
          charIndex = 0; 
          c = CODE_SLOT_CHAR;      
        }
      } 
      else if (knobIndexIncreased) {
        knobIndexIncreased = false;
        charIndex++;
        c = firstAsciiChar + mod(charIndex , nbChars);      
        display.setCursor(cursorX,cursorY);
        display.print(c);        
        display.display();         
      }
      else if (knobIndexDecreased) {
        knobIndexDecreased = false;
        charIndex--;
        c = firstAsciiChar + mod(charIndex , nbChars);       
        display.setCursor(cursorX,cursorY);
        display.print(c);
        display.display();        
      }

      delay(10);  
    }

    // terminate the C-string
    dst[nbCharsValidated]=0;
}

#define PIVOT_CHAR_INDEX 10

bool __attribute__ ((noinline)) getStringFromUser( char* dst, const uint8_t numChars, const char* invite, int firstAsciiChar, int lastAsciiChar)
{
    char buffer[128];
   
    int nbChars = lastAsciiChar - firstAsciiChar +1;

    for (int i=0; i<nbChars;i++) {
      buffer[i] = (char)firstAsciiChar+i;
    }

    for (int j=nbChars; j<nbChars+3;j++) {
      buffer[j] = '^';
    }

    nbChars+=3;

   
    int cursorX=0;
    int nbCharsValidated = 0;
    char pivot_char = 0;
    

    display.clearDisplay();
    display.setCursor(cursorX,0);
    display.print(invite);
    cursorX = display.getCursorX();
    
    for (int n=0; n<numChars; n++) {
      display.print(CODE_SLOT_CHAR);
    }
    
    // display the cursors pointing to the currently selected char
    display.setCursor((PIVOT_CHAR_INDEX)*CHAR_XSIZE,CURSOR_Y_SECOND_LINE);
    display.print((char)31); // triangle pointing down
    display.setCursor((PIVOT_CHAR_INDEX)*CHAR_XSIZE,CURSOR_Y_FOURTH_LINE);
    display.print((char)30); // triangle pointing up

    // print the expected number of chars to be input, where they will appear once validated
    display.setCursor(cursorX,0);
    for (int n=0; n<numChars; n++) {
      display.print('_');
    }
    
    // display rolling list of chars on third line
    display.setCursor(0,CURSOR_Y_THIRD_LINE);

    for (int i=0; i<SCREEN_MAX_NB_CHARS_PER_LINE; i++) {
      //char c = firstAsciiChar + mod(pivot_char + i - PIVOT_CHAR_INDEX, nbChars);
      char c = buffer[mod(pivot_char + i - PIVOT_CHAR_INDEX, nbChars)];
      display.print(c);
    }

    // render
    display.display();
        
    // Loop until required numChars characters are input by user
    while (1) {

      // If knob switch was pushed, validate current char
      if (checkbuttonPushed()) {

        if (buffer[pivot_char] == '^') 
          break;
          
        display.setCursor(cursorX,0);
        //display.print((char)(pivot_char+firstAsciiChar)); 
        display.print(buffer[pivot_char]); 
        display.display();        
        cursorX += CHAR_XSIZE;
          
        //dst[nbCharsValidated] = (char)(pivot_char+firstAsciiChar);
        dst[nbCharsValidated] = (buffer[pivot_char]);
        
        nbCharsValidated++;
        if (nbCharsValidated == numChars)
          break;
      } 
      else if (knobIndexIncreased) {
        knobIndexIncreased = false;

        pivot_char = mod(pivot_char+1,nbChars);
        
        display.setCursor(0,CURSOR_Y_THIRD_LINE);
    
        for (int i=0; i<SCREEN_MAX_NB_CHARS_PER_LINE; i++) {
          //char c = firstAsciiChar + mod(pivot_char + i - PIVOT_CHAR_INDEX, nbChars);
          char c = buffer[mod(pivot_char + i - PIVOT_CHAR_INDEX, nbChars)];
          
          display.print(c);          
        }    
       
        display.display();
      }
      else if (knobIndexDecreased) {
        knobIndexDecreased = false;

        pivot_char = mod(pivot_char-1,nbChars);
        
        display.setCursor(0,CURSOR_Y_THIRD_LINE);
    
        for (int i=0; i<SCREEN_MAX_NB_CHARS_PER_LINE; i++) {
          //char c = firstAsciiChar + mod(pivot_char + i - PIVOT_CHAR_INDEX, nbChars);
          char c = buffer[mod(pivot_char + i - PIVOT_CHAR_INDEX, nbChars)];
          
          display.print(c);
        }  
        
        display.display();    
      }

      delay(1);
    }

    // terminate the C-string
    dst[nbCharsValidated]=0;
    
    return true;
}


/*
int XfreeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}*/

void printSRAMMap() {
    int dummy;
    int free_ram; 
    extern int __heap_end;
    extern int __heap_start;
    extern int __stack;
    extern int __bss_start;  

    extern int __data_end;
    extern int __data_start; 
    extern int * __brkval; 
    extern int __bss_end; 

    int stack=&__stack; 
    free_ram =  (int) &dummy - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
  
    Serial.print("\n%Memory map:");

    Serial.print("\n  __data_start=");
    Serial.print((int)&__data_start); 
        
    Serial.print("\n  __data_end=");
    Serial.print((int)&__data_end);   

    Serial.print("\n, __bss_start=");
    Serial.print((int)&__bss_start);   
        
    Serial.print("\n, __bss_end=");
    Serial.print((int)&__bss_end);  

    Serial.print("\n, __heap_start=");
    Serial.print((int)&__heap_start);

    Serial.print("\n, __brkval=");
    Serial.print((int)__brkval);   
    
    Serial.print("\n, __heap_end=");
    Serial.print((int)&__heap_end);   

    Serial.print("\n, free=");
    Serial.print(free_ram);
    
    Serial.print("\n, stack bottom=");
    Serial.print((int)&dummy);
         
    Serial.print("\n, __stack top=");
    Serial.print((int)&__stack);  
        
    Serial.print("\n, RAMEND=");
    Serial.print(RAMEND);
}


void __attribute__ ((noinline)) printCentered(char* msg) {
      
    int cursorX;
    int cursorY;
  
    cursorX = (DISPLAY_WIDTH - (strlen(msg))*CHAR_XSIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CHAR_YSIZE)/2;

    display.clearDisplay();    
    display.setCursor(cursorX,cursorY);
    display.print(msg);
    cursorX = display.getCursorX();
    display.display(); 

    delay(1000);
}

void __attribute__ ((noinline)) printCenteredStoredString(uint8_t* storedString) {
    char msgBuffer[32];
    getStringFromFlash(msgBuffer, storedString);
    printCentered(msgBuffer);
}

void __attribute__ ((noinline)) format()
{
  //Two input buffers, for easy password comparison.
  char code1[32];
  char code2[32];
  char user[33];
   
  bool fail=1;
/*
  while(fail)
  {
    getCodeFromUser(code1, 6, "NEW CODE? ", '0', '9');
    
    ptxt("\r\nRepeat:");
    
    getCodeFromUser(code2, 6, "REPEAT? ", '0', '9');

    if(memcmp(code1,code2,6)==0)
    {
      fail=0;
    }
    
    if(fail) {
      printCenteredStoredString((uint8_t*)&NEW_CODE_MISMATCH);
    }
      
  }
  fail=1;
  
  
  while(fail)
  {
    if( getStringFromUser(user, 9, (char*)"Name? ", '!', '~' ) )
    {
      fail=0;
    }
  }
  */
  
  sprintf(code1, (char*)F("123456"));
  sprintf(user, (char*)F("bids"));
  ES.format( (byte*)code1, user );
}

bool __attribute__ ((noinline)) login()
{
  char key[7];
  char devName[32];
  memset(key,0,7);
  memset(devName,0,32);
  uint8_t ret=0;

  if(!ES.readHeader(devName)) {
    printCenteredStoredString((uint8_t*)&NEED_FORMAT);
    delay(2000);
    format();
  } else {
    
   // Serial.print("\nlogin:");
   // Serial.print(devName);
    
   printCentered(devName);
    delay(1000);

    char invite[32];
    getStringFromFlash(invite, (uint8_t*)&CODE_INVITE);
    getCodeFromUser(key, 6, invite, '0', '9');
    /*
    if(ES.unlock((byte*)key)) {
       ret=1;
       printCenteredStoredString((uint8_t*)&LOGIN_GRANTED);
        delay(1000);
    } else {
       printCenteredStoredString((uint8_t*)&LOGIN_DENIED);
        delay(1000);
    }
    */
  }
  
  ret=1;

  return ret;
}

////////////////////
// INITIALISATION
////////////////////
void setup()   {  
  
  // RN42 is configured by default to use 115200 bauds on UART links
  //Serial.begin(115200);

  Serial.begin(9600);

  printSRAMMap();
  Entropy.initialize();
  setRng();
  
  // Initialize OLED display
  display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDR);

  // Welcome banner
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.setCursor(0,0);
  display.println(F("Hello, world!"));
  display.display();
  delay(2000);

  // Setup the interrupt used for knob management
  pinMode(knobInterruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(knobInterruptPin), uponKnobIT, FALLING);   

  // Setup the pin used for knob management
  pinMode(knobSwitchPin, INPUT_PULLUP);

//  knobIncrementChanged=false;
  knobIndexIncreased=false;
  knobIndexDecreased=false;
  knobSwitchPushed=false;
}

////////////////////
// MAIN LOOP
////////////////////
static long loopidx=0;
void loop() {

  if(login()) {
    char tmp[6];
    char invite[32];
    getStringFromFlash(invite, (uint8_t*)&WHATEVER_INVITE);    
    getStringFromUser(tmp, 5, invite,'!', '~');

    display.clearDisplay();
    display.setCursor(0,0);
    display.print("GOT=");
    display.print(tmp);
    display.display();
    delay(2000);
  }

  while(1) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.print(F("LOOP!"));
    display.print(loopidx);
    display.display();
    loopidx++;
    delay(500);
  }
}
