#include <TermTool.h>
#include <SPI.h>
#include "eeprom.h"
#include "EncryptedStorage.h"
#include "Entropy.h"
#include "constants.h"
#include "display.h"
#include "utils.h"

#define DEBUG_ENABLE 0
#define DEBUG(x) if(DEBUG_ENABLE) { x }

// max 21 characters per line
const static char NEW_CODE_INVITE[] PROGMEM = "NEW CODE? ";
const static char REPEAT_CODE_INVITE[] PROGMEM = "REPEAT? ";
const static char NAME_INPUT[] PROGMEM = "NAME? ";
const static char CODE_INVITE[] PROGMEM = "CODE:";
const static char WHATEVER_INVITE[] PROGMEM = "TEST:";
const static char NEW_CODE_MISMATCH[] PROGMEM = "ERROR: mismatch";
const static char INVALID_VALUE[] PROGMEM = "ERROR: invalid value";
const static char DEVICE_EMPTY[] PROGMEM = "Device is empty";
const static char NEED_FORMAT[] PROGMEM = "Format needed";
const static char LOGIN_GRANTED[] PROGMEM = "OK";
const static char LOGIN_DENIED[] PROGMEM = "DENIED";

const static char DEVICE_FULL[] PROGMEM = "Error: device full";

const static char ACCOUNT_TITLE_INPUT[] PROGMEM = "Account? ";
const static char ACCOUNT_LOGIN_INPUT[] PROGMEM = "Login? ";
const static char PASSWORD_LENGTH_INPUT[] PROGMEM = "Length? [0-16] ";
const static char PASSWORD_VALUE_INPUT[] PROGMEM = "Pwd? ";
const static char STORING_NEW_PASSWORD[] PROGMEM = "Storing...";

// Rules:
// max 21 characters per line, minus left triangle cursor and one space => max 19 chars max per line, including starting and trailing space
// also, to avoid avoid to explicitly clear the display, make all entries the same size so that all text gets overwritten.
                                          // "  XXXXXXXXXXXXXXXXX ";
const static char MENU_GETPWD[] PROGMEM   = "Get Password  ";
const static char MENU_SETPWD[] PROGMEM   = "Set Password  ";
const static char MENU_CLEARPWD[] PROGMEM = "Clear Password";
const static char MENU_FORMAT[] PROGMEM   = "Format        ";
#define MAIN_MENU_NB_ENTRIES 4

const static char MENU_SETPWD_GENERATE[] PROGMEM      = "Generate";
const static char MENU_SETPWD_MANUALINPUT[] PROGMEM   = "Manually";
#define MENU_SETPWD_NB_ENTRIES 2

const static char MENU_SETPWD_GENERATE_LENGTH[] PROGMEM      = "Length?";

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

bool __attribute__ ((noinline)) confirmChoice(char* text)
{
    int cursorX = (DISPLAY_WIDTH - strlen(text)*CHAR_XSIZE)/2;

    int nbCharsValidated = 0;
    bool confirmed=false;
    bool needRefresh = true;
    char c = CODE_SLOT_CHAR;

    display.clearDisplay();    
    display.setCursor(cursorX,CURSOR_Y_FIRST_LINE);
    display.print(text);     
    display.display();
           
    // Require numChars input characters from user
    while (1) {

      // If knob switch was pushed, validate current char
      if (checkbuttonPushed()) {
        return confirmed;
      } 
      else if (knobIndexIncreased) {
        knobIndexIncreased = false;
        confirmed = !confirmed;
        needRefresh = true;
      }
      else if (knobIndexDecreased) {
        knobIndexDecreased = false;
        confirmed = !confirmed;
        needRefresh = true;
      }

      if (needRefresh) {
          needRefresh = false;

          display.setCursor((DISPLAY_WIDTH - strlen("Yes No")*CHAR_XSIZE)/2,CURSOR_Y_THIRD_LINE);

          if (confirmed) {
            display.setTextColor(BLACK, WHITE);
            display.print("Yes");     
            display.setTextColor(WHITE, BLACK);
            display.print(" ");
            display.print("No");  
          } else {
            display.print("Yes");  
            display.print(" ");
            display.setTextColor(BLACK, WHITE);
            display.print("No");     
            display.setTextColor(WHITE, BLACK);
          }
          display.display();
       }

      delay(10);  
    }
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
    char buffer[100]; // minimized to save stack space while fitting worst case of firstAscii=33, lastAscii=126, and DONE_STRING=4 bytes => 98 bytes 
   
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

        if ((pivot_char >= DONE_chars_start_index) && (pivot_char <= DONE_chars_stop_index)) 
          break;
          
        display.setCursor(cursorX,0);
        display.print(buffer[pivot_char]); 
        display.display();        
        cursorX += CHAR_XSIZE;
          
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

int __attribute__ ((noinline)) generic_menu(int nbEntries, uint8_t** menutexts) {
  
  byte selected_index = 0;
  byte display_line_index = 0;
  char menuEntryText[32];
  bool needRefresh = true;

  display.clearDisplay();
 
  while (1) {
  
    // If knob switch was pushed, return currently selected entry
    if (checkbuttonPushed()) {
        if (nbEntries > SCREEN_MAX_NB_LINES) {
          return (mod(selected_index+display_line_index, nbEntries));
        } else {
          return selected_index;
        }
    } 
    else if (knobIndexIncreased) {
      knobIndexIncreased = false;
      if (nbEntries > SCREEN_MAX_NB_LINES) {
        selected_index = mod(selected_index+1,nbEntries);
      } else {
        if (selected_index<(nbEntries-1))
          selected_index++;
      }
      if ((display_line_index<SCREEN_MAX_NB_LINES-1) && (display_line_index<nbEntries-1))
        display_line_index++;
      needRefresh = true;      
    }
    else if (knobIndexDecreased) {
      knobIndexDecreased = false;
      if (nbEntries > SCREEN_MAX_NB_LINES) {
        selected_index = mod(selected_index-1,nbEntries);
      } else {
        if (selected_index>0)
          selected_index--;
      }
      if (display_line_index>0)
        display_line_index--;
      needRefresh = true; 
    }
  
   if (needRefresh) {



      unsigned long StartTime = millis();

      needRefresh = false;
      int y = 0;
      for (int i=0; i < SCREEN_MAX_NB_LINES; i++) {


        if (i>= nbEntries) {
          break;
        } else {

          if (nbEntries > SCREEN_MAX_NB_LINES) {
            getStringFromFlash(menuEntryText, menutexts[mod(selected_index+i,nbEntries)]);
          } else {
            getStringFromFlash(menuEntryText, menutexts[i]);
          }
          display.setCursor((DISPLAY_WIDTH - (strlen(menuEntryText)+4)*CHAR_XSIZE)/2,y);
        
          if (i==display_line_index) {
            display.print((char)16);
            display.print(' ');
          } else {
            display.print("  ");
          }  

          display.print(menuEntryText);



          
          y+= CHAR_YSIZE;
        }
      }
          display.display();

      unsigned long CurrentTime = millis();
  unsigned long ElapsedTime = CurrentTime - StartTime;
  Serial.print("get String time:"); Serial.println(ElapsedTime);        
   }
     
   delay(1);
  }
}

int __attribute__ ((noinline)) main_menu() {
  uint8_t* menutexts[MAIN_MENU_NB_ENTRIES];
  menutexts[0] =   (uint8_t*)&MENU_GETPWD;
  menutexts[1] =   (uint8_t*)&MENU_SETPWD;
  menutexts[2] =   (uint8_t*)&MENU_CLEARPWD;
  menutexts[3] =   (uint8_t*)&MENU_FORMAT;
  return generic_menu(MAIN_MENU_NB_ENTRIES, menutexts);
}

int __attribute__ ((noinline)) menu_set_pwd() {
  uint8_t* menutexts[MENU_SETPWD_NB_ENTRIES];
  menutexts[0] =   (uint8_t*)&MENU_SETPWD_GENERATE;
  menutexts[1] =   (uint8_t*)&MENU_SETPWD_MANUALINPUT;
  return generic_menu(MENU_SETPWD_NB_ENTRIES, menutexts);
}

void __attribute__ ((noinline)) putRandomChars( char* dst, uint8_t len)
{
  char pool[10+26+26];
  int pool_index = 0;
  
  for( uint8_t idx = 48; idx < 58; idx++)
  {
    pool[pool_index++] = idx;
  }
  
  for( uint8_t idx = 65; idx < 91; idx++)
  {
    pool[pool_index++] = idx;
  }

  for( uint8_t idx = 97; idx < 123; idx++)
  {
    pool[pool_index++] = idx;
  }  

  // pick from the pool of letters to fill password randomly
  for( uint8_t idx = 0; idx < len; idx++)
  {
    dst[idx] = pool[(uint8_t)Entropy.random(10+26+25)];
  }
  dst[len]=0;
}

void __attribute__ ((noinline)) generatePassword() {
  uint16_t entryNum = ES.getNextEmpty();
  entry_t entry;
  char len_string[3];
  int len;

  if( entryNum == NUM_ENTRIES ) {
    displayCenteredMessageFromStoredString((uint8_t*)&DEVICE_FULL);
    return;
  } 
  else {

    memset(&entry, 0, sizeof(entry));
    char buf[16];
    
    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_TITLE_INPUT);
    getStringFromUser(entry.title, ACCOUNT_TITLE_LENGTH, buf, '!', '~' );

    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_LOGIN_INPUT);
    getStringFromUser(entry.data, ACCOUNT_LOGIN_LENGTH, buf, '!', '~' );

    do {
      getStringFromFlash(buf, (uint8_t*)&PASSWORD_LENGTH_INPUT);
      getStringFromUser(len_string, 2, buf, '0', '9' );
  
      entry.passwordOffset = strlen(entry.data)+1;  
      len = atoi(len_string);

      if (len > PASSWORD_MAX_LENGTH){
            displayCenteredMessageFromStoredString((uint8_t*)&INVALID_VALUE);
      }
    } while(len > PASSWORD_MAX_LENGTH);
    
    putRandomChars( ((entry.data)+entry.passwordOffset),len);
    //strcpy((entry.data)+entry.passwordOffset, "password");

    display.clearDisplay();    

    displayCenteredMessageFromStoredString((uint8_t*)&STORING_NEW_PASSWORD);

    /*
    display.setCursor(0,CURSOR_Y_FIRST_LINE);
    getStringFromFlash(buf, (uint8_t*)&STORING_NEW_PASSWORD);
    display.print(buf);
    
    display.setCursor(0,CURSOR_Y_SECOND_LINE);
    display.print(entry.title);

    display.setCursor(0,CURSOR_Y_THIRD_LINE);      
    display.print(entry.data);

    display.setCursor(0,CURSOR_Y_FOURTH_LINE);    
    for (int i=0; i < len; i++) {
      char * c = (char*)(entry.data+entry.passwordOffset+i);
      display.print(*c);
    }

    display.display();
    */
   
    ES.putEntry( (uint8_t)entryNum, &entry );
  } 
}

void __attribute__ ((noinline)) inputPassword() {
  uint16_t entryNum = ES.getNextEmpty();
  entry_t entry;
  char len_string[3];
  int len;

  if( entryNum == NUM_ENTRIES ) {
    displayCenteredMessageFromStoredString((uint8_t*)&DEVICE_FULL);
    return;
  } 
  else {

    memset(&entry, 0, sizeof(entry));
    char buf[16];
    
    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_TITLE_INPUT);
    getStringFromUser(entry.title, ACCOUNT_TITLE_LENGTH, buf, '!', '~' );

    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_LOGIN_INPUT);
    getStringFromUser(entry.data, ACCOUNT_LOGIN_LENGTH, buf, '!', '~' );
   
    getStringFromFlash(buf, (uint8_t*)&PASSWORD_VALUE_INPUT);
    getStringFromUser((entry.data)+entry.passwordOffset, PASSWORD_MAX_LENGTH, buf, '!', '~' );    

    display.clearDisplay();    

    displayCenteredMessageFromStoredString((uint8_t*)&STORING_NEW_PASSWORD);

    /*
    display.setCursor(0,CURSOR_Y_FIRST_LINE);
    getStringFromFlash(buf, (uint8_t*)&STORING_NEW_PASSWORD);
    display.print(buf);
    
    display.setCursor(0,CURSOR_Y_SECOND_LINE);
    display.print(entry.title);

    display.setCursor(0,CURSOR_Y_THIRD_LINE);      
    display.print(entry.data);

    display.setCursor(0,CURSOR_Y_FOURTH_LINE);    
    for (int i=0; i < len; i++) {
      char * c = (char*)(entry.data+entry.passwordOffset+i);
      display.print(*c);
    }

    display.display();
    */
   
    ES.putEntry( (uint8_t)entryNum, &entry );   
  } 
}

int __attribute__ ((noinline)) pickEntry() {

  char eName[32];
  uint8_t entryNum=0;
  uint8_t nbEntries;
  uint8_t maxEntryLength=0;
  bool hasEntry=false;
 
  do
  {
    hasEntry = ES.getTitle(entryNum, eName);
    if(hasEntry)
    {
      DEBUG(Serial.print("Entry ");)
      DEBUG(Serial.print(entryNum);)
      DEBUG(Serial.print(": ");)
      DEBUG(Serial.println(eName);)
      int len = strlen(eName);
      if (len>maxEntryLength) maxEntryLength = len;
      entryNum++;
    }
  } while (hasEntry);

  nbEntries = entryNum;
  //DEBUG(Serial.print("nbEntries=");Serial.println(nbEntries);)

  if(nbEntries==0) return -1;

  byte selected_index = 0;
  byte display_line_index = 0;
  char menuEntryText[32];
  bool needRefresh = true;

  display.clearDisplay();
 
  while (1) {
  
    // If knob switch was pushed, return currently selected entry
    if (checkbuttonPushed()) {
        if (nbEntries > SCREEN_MAX_NB_LINES) {
          return (mod(selected_index+display_line_index, nbEntries));
        } else {
          return selected_index;
        }
    } 
    else if (knobIndexIncreased) {
      knobIndexIncreased = false;
      if (nbEntries > SCREEN_MAX_NB_LINES) {
        selected_index = mod(selected_index+1,nbEntries);
      } else {
        if (selected_index<(nbEntries-1))
          selected_index++;
      }
      if ((display_line_index<SCREEN_MAX_NB_LINES-1) && (display_line_index<nbEntries-1))
        display_line_index++;
      needRefresh = true;      
    }
    else if (knobIndexDecreased) {
      knobIndexDecreased = false;
      if (nbEntries > SCREEN_MAX_NB_LINES) {
        selected_index = mod(selected_index-1,nbEntries);
      } else {
        if (selected_index>0)
          selected_index--;
      }
      if (display_line_index>0)
        display_line_index--;
      needRefresh = true; 
    }
  
   if (needRefresh) {
    needRefresh = false;
      int y = 0;
      for (int i=0; i < SCREEN_MAX_NB_LINES; i++) {

        if (i>= nbEntries) {
          break;
        } else {

          if (nbEntries > SCREEN_MAX_NB_LINES) {
            entryNum = mod(selected_index+i,nbEntries);
            //getStringFromFlash(menuEntryText, menutexts[mod(selected_index+i,nbEntries)]);
          } else {
            entryNum = i;
            //getStringFromFlash(menuEntryText, menutexts[i]);
          }

          ES.getTitle(entryNum, menuEntryText);
          int entryLen = strlen(menuEntryText);
          display.setCursor(0,y);
        
          if (i==display_line_index) {
            display.print((char)16);
            display.print(' ');
            display.print(menuEntryText);
          } else {
            display.print("  ");
            display.print(menuEntryText);
          }

          if (entryLen<maxEntryLength) {
             for (int k=0;k<maxEntryLength-entryLen;k++){
               display.print(' ');
             }
           }
        
          display.display();
          y+= CHAR_YSIZE;
        }
      }        
   }
     
   delay(1);
  }
  
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
  char code1[USERCODE_BUFF_LEN];
  char code2[USERCODE_BUFF_LEN];
  
  char user[DEVICENAME_LENGTH+1];  
  char buf[32];
  bool fail=1;

  memset( code1, 0, USERCODE_BUFF_LEN);

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
      displayCenteredMessageFromStoredString((uint8_t*)&NEW_CODE_MISMATCH);
    }      
  }
  fail=1;
  
  while(fail)
  {
    getStringFromFlash(buf, (uint8_t*)&NAME_INPUT);
    if( getStringFromUser(user, DEVICENAME_LENGTH, buf, '!', '~' ) )
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
  char key[USERCODE_BUFF_LEN+1];
  char buf[32];
  memset(key,0,USERCODE_BUFF_LEN+1);
  uint8_t ret=0;

  getStringFromFlash(buf, (uint8_t*)&CODE_INVITE);
  getCodeFromUser(key, USERCODE_LENGTH, buf, '0', '9');
   
  if(ES.unlock((byte*)key)) {
     ret=1;
     displayCenteredMessageFromStoredString((uint8_t*)&LOGIN_GRANTED);
      delay(500);
  } else {
     displayCenteredMessageFromStoredString((uint8_t*)&LOGIN_DENIED);
      delay(500);
  }

  return ret;
}

////////////////////
// INITIALISATION
////////////////////
void setup()   {  


 //Serial.begin(115200);
 //return;


 

  // Initialize stack canary
  paintStack();
    
  char devName[DEVNAME_BUFF_LEN];
  memset(devName,0,DEVNAME_BUFF_LEN);
  
  // RN42 is configured by default to use 115200 bauds on UART links
  Serial.begin(115200);
  //Serial.begin(9600);
        
  //printSRAMMap();

  int x = StackMarginWaterMark();
  DEBUG(Serial.print("initial Watermark : "); Serial.println(x);)

  //DEBUG(Serial.print("Current stack size: ");  Serial.println((RAMEND - SP), DEC);)

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

//knobIncrementChanged=false;
  knobIndexIncreased=false;
  knobIndexDecreased=false;
  knobSwitchPushed=false;

  // debug feature to force reformatting
  //messUpEEPROMFormat();

  if(!ES.readHeader(devName)) {
    displayCenteredMessageFromStoredString((uint8_t*)&NEED_FORMAT);
    delay(1000);
    format();
  } else {
    char buf[48];
    sprintf(buf, "BLUEKEY (%s)", devName);
    displayCenteredMessage(buf);
    delay(500);
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


  //Serial.print("toto ");
  //delay(1000);
  //return;




  int entry_choice;
  int selection = main_menu();
  entry_t temp;

  switch (selection) {
    case 0:
      // GET_PWD
      entry_choice = pickEntry();
      if (entry_choice != -1) {
        DEBUG(Serial.print("picked entry:");)
        DEBUG(Serial.println(entry_choice);)
        ES.getEntry(entry_choice, &temp);
        DEBUG(Serial.print("password for this entry:");)
        
        // Send password to Serial TX line (hence to Bluetooth module)
        Serial.print(temp.data+temp.passwordOffset);
      } else
            displayCenteredMessageFromStoredString((uint8_t*)&DEVICE_EMPTY);
      break;
    case 1:
      // SET_PWD     
      //int set_pwd_selection = menu_set_pwd();
          
      switch (menu_set_pwd()) {
        case 0:
          // GENERATE
          generatePassword();
          break;
        case 1:
          // MANUALLY
          inputPassword();         
          break;
      }
      break;
    case 2:
      // CLEAR_PWD
      entry_choice = pickEntry();
      if (confirmChoice("del entry?")) ES.delEntry(entry_choice);
      break;
    case 3:
      // FORMAT
      if (confirmChoice("Sure?")) format();
      break;
  }

  DEBUG(
  int x = StackMarginWaterMark();
  Serial.print("Watermark after main loop action: "); Serial.println(x); )
}

