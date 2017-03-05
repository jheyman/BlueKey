#include <TermTool.h>
#include <SPI.h>
#include "eeprom.h"
#include "EncryptedStorage.h"
#include "Entropy.h"
#include "constants.h"
#include "display.h"
#include "utils.h"

const static char NEW_CODE_INVITE[] PROGMEM = "NEW CODE? ";
const static char REPEAT_CODE_INVITE[] PROGMEM = "REPEAT? ";
const static char NAME_INPUT[] PROGMEM = "NAME? ";
const static char CODE_INVITE[] PROGMEM = "CODE:";
const static char WHATEVER_INVITE[] PROGMEM = "TEST:";
const static char NEW_CODE_MISMATCH[] PROGMEM = "ERROR: mismatch";
const static char NEED_FORMAT[] PROGMEM = "Format needed";
const static char LOGIN_GRANTED[] PROGMEM = "OK";
const static char LOGIN_DENIED[] PROGMEM = "DENIED";

////////////////////
// KNOB management
////////////////////

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
    int DONE_chars_start_index, DONE_chars_stop_index;

    char DONE_STRING[] = "DONE";
    for (int i=0; i<nbChars;i++) {
      buffer[i] = (char)firstAsciiChar+i;
    }

    DONE_chars_start_index = nbChars;
    for (int j=nbChars; j<nbChars+strlen(DONE_STRING);j++) {
      buffer[j] = DONE_STRING[j-nbChars];
    }
    DONE_chars_stop_index = nbChars+strlen(DONE_STRING);
    nbChars+=strlen(DONE_STRING);
   
    int cursorX=0;
    int nbCharsValidated = 0;
    char pivot_char = 0;
    
    display.clearDisplay();
    display.setCursor(cursorX,0);
    display.print(invite);
    cursorX = display.getCursorX();

    // print the expected number of chars to be input, where they will appear once validated
    for (int n=0; n<numChars; n++) {
      display.print(CODE_SLOT_CHAR);
    }
    
    // display the cursors pointing to the currently selected char
    display.setCursor((PIVOT_CHAR_INDEX)*CHAR_XSIZE,CURSOR_Y_SECOND_LINE);
    display.print((char)31); // triangle pointing down
    display.setCursor((PIVOT_CHAR_INDEX)*CHAR_XSIZE,CURSOR_Y_FOURTH_LINE);
    display.print((char)30); // triangle pointing up

    // display rolling list of chars on third line
    display.setCursor(0,CURSOR_Y_THIRD_LINE);

    for (int i=0; i<SCREEN_MAX_NB_CHARS_PER_LINE; i++) {
      //char c = firstAsciiChar + mod(pivot_char + i - PIVOT_CHAR_INDEX, nbChars);
      int index = mod(pivot_char + i - PIVOT_CHAR_INDEX, nbChars);
      char c = buffer[index];

      if ((index >= DONE_chars_start_index) && (index <= DONE_chars_stop_index)) {
        display.setTextColor(BLACK, WHITE);
        display.print(c);
        display.setTextColor(WHITE, BLACK);
      } else  {
        display.print(c);
      }
    }

    // render
    display.display();
        
    // Loop until required numChars characters are input by user
    while (1) {

      // If knob switch was pushed, validate current char
      if (checkbuttonPushed()) {

        //if (buffer[pivot_char] == '^') 
        //if (buffer[pivot_char] == 0) 
        if ((pivot_char >= DONE_chars_start_index) && (pivot_char <= DONE_chars_stop_index)) 
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
          int index = mod(pivot_char + i - PIVOT_CHAR_INDEX, nbChars);
          char c = buffer[index];

          if ((index >= DONE_chars_start_index) && (index <= DONE_chars_stop_index)) {
            display.setTextColor(BLACK, WHITE);
            display.print(c);
            display.setTextColor(WHITE, BLACK);
          } else  {
            display.print(c);
          }    
        }    
        display.display();
      }
      else if (knobIndexDecreased) {
        knobIndexDecreased = false;

        pivot_char = mod(pivot_char-1,nbChars);
        
        display.setCursor(0,CURSOR_Y_THIRD_LINE);
    
        for (int i=0; i<SCREEN_MAX_NB_CHARS_PER_LINE; i++) {
          int index = mod(pivot_char + i - PIVOT_CHAR_INDEX, nbChars);
          char c = buffer[index];
          
          if ((index >= DONE_chars_start_index) && (index <= DONE_chars_stop_index)) {
            display.setTextColor(BLACK, WHITE);
            display.print(c);
            display.setTextColor(WHITE, BLACK);
          } else  {
            display.print(c);
          }  
        }  
        
        display.display();    
      }

      delay(1);
    }

    // terminate the C-string
    dst[nbCharsValidated]=0;
    
    return true;
}

////////////////////
// MISC
////////////////////
void setRng()
{
  analogWrite(ledPin, 250);
  randomSeed( Entropy.random() );
  digitalWrite(ledPin,1);
}

/////////////////////
// INITIAL FORMATTING 
/////////////////////
void __attribute__ ((noinline)) format()
{
  char code1[32];
  char code2[32];
  char user[33];
  char buf[32];
  bool fail=1;

  memset( code1, 0, 32 );

  while(fail)
  {
    getStringFromFlash(buf, (uint8_t*)&NEW_CODE_INVITE);
    getCodeFromUser(code1, USERCODE_LENGTH, buf, '0', '9');
    
    getStringFromFlash(buf, (uint8_t*)&REPEAT_CODE_INVITE);
    getCodeFromUser(code2, USERCODE_LENGTH, buf, '0', '9');

    if(memcmp(code1,code2,USERCODE_LENGTH)==0)
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
    getStringFromFlash(buf, (uint8_t*)&NAME_INPUT);
    if( getStringFromUser(user, USERNAME_LENGTH, buf, '!', '~' ) )
    {
      fail=0;
    }
  }
  
  ES.format( (byte*)code1, user );
}
/////////
// LOGIN 
/////////
bool __attribute__ ((noinline)) login()
{
  char key[33];
  char buf[32];
  memset(key,0,33);
  uint8_t ret=0;

  getStringFromFlash(buf, (uint8_t*)&CODE_INVITE);
  getCodeFromUser(key, 6, buf, '0', '9');
  
  if(ES.unlock((byte*)key)) {
     ret=1;
     printCenteredStoredString((uint8_t*)&LOGIN_GRANTED);
      delay(1000);
  } else {
     printCenteredStoredString((uint8_t*)&LOGIN_DENIED);
      delay(1000);
  }
    
  return ret;
}

////////////////////
// INITIALISATION
////////////////////
void setup()   {  

  char devName[32];
  memset(devName,0,32);
  
  // RN42 is configured by default to use 115200 bauds on UART links
  //Serial.begin(115200);
  Serial.begin(9600);

  printSRAMMap();
  Entropy.initialize();
  setRng();
  
  // Initialize OLED display
  display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDR);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  
  // Setup the interrupt used for knob management
  pinMode(knobInterruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(knobInterruptPin), uponKnobIT, FALLING);   

  // Setup the pin used for knob management
  pinMode(knobSwitchPin, INPUT_PULLUP);

//  knobIncrementChanged=false;
  knobIndexIncreased=false;
  knobIndexDecreased=false;
  knobSwitchPushed=false;

  // debug feature to force reformatting
  //messUpEEPROMFormat();

  if(!ES.readHeader(devName)) {
    printCenteredStoredString((uint8_t*)&NEED_FORMAT);
    delay(1000);
    format();
  } else {
    char buf[48];
    sprintf(buf, "BLUEKEY(%d)", devName);
    printCentered(buf);
    delay(1000);
  }
  
  // Login now
  bool login_status = false;
  do {
    login_status = login();
  } while (login_status==false);
  
}

////////////////////
// MAIN LOOP
////////////////////
static long loopidx=0;
void loop() {

  char tmp[6];
  char invite[32];
  getStringFromFlash(invite, (uint8_t*)&WHATEVER_INVITE);    
  getStringFromUser(tmp, 5, invite,'!', '~');

  display.clearDisplay();
  display.setCursor(0,0);
  display.print("GOT=");
  display.print(tmp);
  display.display();

  while(1) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.print(F("LOOP!"));
    display.print(loopidx);
    display.display();
    loopidx++;
    Serial.print('.');
    if (loopidx % 25 == 0)
      Serial.println("");
    delay(1000);
  }
}
