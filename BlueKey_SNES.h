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


byte buttons[] = {UpButtonPin, DownButtonPin, LeftButtonPin, RightButtonPin, YButtonPin};
#define NUMBUTTONS sizeof(buttons)
boolean pressed[NUMBUTTONS], justpressed[NUMBUTTONS], repeat[NUMBUTTONS], held[NUMBUTTONS];

void check_buttons(int repeatDelay)
{
  static byte lateststate[NUMBUTTONS];
  static long repeatTime[NUMBUTTONS];
  static long pressTime[NUMBUTTONS];
  static long lasttime;
  byte index;

  if (millis()<lasttime) {// we wrapped around, lets just try again
     lasttime = millis();
  }
  
  if ((lasttime + DEBOUNCE_TIME) > millis()) {
    return; 
  }
  
  lasttime = millis();
  
  for (index = 0; index<NUMBUTTONS; index++) {
    justpressed[index] = 0;
     
    byte newstate = digitalRead(buttons[index]);

    // If two readings in a row give the same value, state is confirmed
    if (newstate == lateststate[index]) {
      // If button was not pressed, but now is (state LOW = input activated):
      if ((pressed[index] == false) && (newstate == LOW)) {
          justpressed[index] = 1;
          pressTime[index] = millis();
          repeatTime[index] = pressTime[index];
      } else if ((pressed[index] == true) && (newstate == HIGH)) {
          repeat[index] = false;
          held[index] = false;
      }
      // Keep track of current confirmed button state
      pressed[index] = !newstate;
      
    }

    // if button repeat time had been reached last time, reset it now to wait again before repeating again
    if (repeat[index]== true){
      repeat[index] = false;
      repeatTime[index] = millis();
    }      

    // Check is button has been pressed long enough since last press/repeat time
    if (pressed[index]) {
      
      if ( millis() - repeatTime[index] > repeatDelay) {
        repeat[index] = true;
      }
      
      if (millis() - pressTime[index] > BUTTON_HOLD_TIME) {
        held[index] = true;
      }
    }
        
    lateststate[index] = newstate;   
  }
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

      check_buttons(BUTTON_REPEAT_TIME_SLOW);

      // If knob switch was pushed, validate current char
      if (justpressed[YButtonIndex]) {
        return confirmed;
      } 
      else if (justpressed[UpButtonIndex] || repeat[UpButtonIndex]) {
        confirmed = !confirmed;
        needRefresh = true;
      }
      else if (justpressed[DownButtonIndex] || repeat[DownButtonIndex]) {
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

      check_buttons(BUTTON_REPEAT_TIME_SLOW);
 
      if (justpressed[YButtonIndex]) {
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
      else if (justpressed[UpButtonIndex] || repeat[UpButtonIndex]) {
          charIndex++;
          c = firstAsciiChar + mod(charIndex , nbChars);      
          display.setCursor(cursorX,cursorY);
          display.print(c);        
          display.display();  
      }
      else if (justpressed[DownButtonIndex] || repeat[DownButtonIndex]) {
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


char * UpperCaseLetters="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char * LowerCaseLetters="abcdefghijklmnopqrstuvwxyz";
char * SpecialCharacters="&~#'@*$!";
char * Numbers="012345678";
char * Done="[DONE]";

typedef struct {
  char* buffers[5]; 
  char buffer_size[5];
  byte nbBuffers;
} MultilineInputBuffer;

bool __attribute__ ((noinline)) getStringFromUser( char* dst, const uint8_t numChars, const char* invite, MultilineInputBuffer mlib)
{
    byte currentInputBufferLine = 0;
       
    int cursorX=0;
    int nbCharsValidated = 0;
    byte select_index = PIVOT_CHAR_INDEX;
    byte old_select_index = PIVOT_CHAR_INDEX;
    byte select_index_max = 0;

    byte bufferOffset = 0;
    byte bufferOffsetMax = 0;

    bool needLettersRefresh=true;
    bool needSelectorRefresh=true;
     
    if (mlib.buffer_size[currentInputBufferLine] < SCREEN_MAX_NB_CHARS_PER_LINE)
      bufferOffsetMax = 0; 
    else
      bufferOffsetMax = mlib.buffer_size[currentInputBufferLine] - SCREEN_MAX_NB_CHARS_PER_LINE;

    if (mlib.buffer_size[currentInputBufferLine] < SCREEN_MAX_NB_CHARS_PER_LINE)
      select_index_max = mlib.buffer_size[currentInputBufferLine] - 1; 
    else
      select_index_max = SCREEN_MAX_NB_CHARS_PER_LINE-1;

    display.clearDisplay();
    display.setCursor(cursorX,0);
    display.print(invite);
    cursorX = display.getCursorX();

    // print the expected number of chars to be input, where they will appear once validated
    for (int n=0; n<numChars; n++) {
      display.print(CODE_SLOT_CHAR);
    }
    
    // display the cursors pointing to the currently selected char
    display.setCursor((select_index)*CHAR_XSIZE,CURSOR_Y_SECOND_LINE);
    display.print((char)31); // triangle pointing down
    display.setCursor((select_index)*CHAR_XSIZE,CURSOR_Y_FOURTH_LINE);
    display.print((char)30); // triangle pointing up

    // display rolling list of chars on third line
    display.setCursor(0,CURSOR_Y_THIRD_LINE);

    for (int i=0; i<SCREEN_MAX_NB_CHARS_PER_LINE; i++) {
      char c = mlib.buffers[currentInputBufferLine][bufferOffset+i];
      display.print(c);
    }

    // render
    display.display();
        
    // Loop until required numChars characters are input by user
    while (1) {

      check_buttons(BUTTON_REPEAT_TIME_VERYSLOW);

      // If validation was pushed, validate current char
      if (justpressed[YButtonIndex]) {

        if (currentInputBufferLine == mlib.nbBuffers - 1) 
          break;
          
        display.setCursor(cursorX,0);
        display.print(mlib.buffers[currentInputBufferLine][bufferOffset + select_index]);
        display.display();        
        cursorX += CHAR_XSIZE;
          
        dst[nbCharsValidated] = (mlib.buffers[currentInputBufferLine][bufferOffset + select_index]);
        
        nbCharsValidated++;
        if (nbCharsValidated == numChars)
          break;
      } 
      else if (justpressed[LeftButtonIndex]) {
        if (select_index < select_index_max)
        {
          old_select_index = select_index;
          select_index = select_index + 1;  
          needSelectorRefresh=true;          
        }        
      }
      else if (held[LeftButtonIndex]) {
        if (select_index < select_index_max)
        {
          old_select_index = select_index;
          select_index = select_index + 1;  
          needSelectorRefresh=true;          
        }        
      }      
      else if (justpressed[RightButtonIndex]) {
        if (select_index > 0)
        {
          old_select_index = select_index;
          select_index = select_index - 1;
          needSelectorRefresh=true;          
        }
      }
      else if (held[RightButtonIndex]) {
        if (select_index > 0)
        {
          old_select_index = select_index;
          select_index = select_index - 1;
          needSelectorRefresh=true;          
        }
      }      
      else if (justpressed[UpButtonIndex] ) {
        old_select_index = select_index;
        currentInputBufferLine = mod(currentInputBufferLine-1,mlib.nbBuffers);
        if (mlib.buffer_size[currentInputBufferLine] < SCREEN_MAX_NB_CHARS_PER_LINE)
          bufferOffsetMax = 0; 
        else
          bufferOffsetMax = mlib.buffer_size[currentInputBufferLine] - SCREEN_MAX_NB_CHARS_PER_LINE;
    
        if (mlib.buffer_size[currentInputBufferLine] < SCREEN_MAX_NB_CHARS_PER_LINE)
          select_index_max = mlib.buffer_size[currentInputBufferLine] - 1; 
        else
          select_index_max = SCREEN_MAX_NB_CHARS_PER_LINE-1;     

        if (select_index > select_index_max) select_index = select_index_max;
             
        needLettersRefresh=true;
        needSelectorRefresh=true;    
      }
      else if (justpressed[DownButtonIndex] ) {

        old_select_index = select_index;
        currentInputBufferLine = mod(currentInputBufferLine+1,mlib.nbBuffers);
        if (mlib.buffer_size[currentInputBufferLine] < SCREEN_MAX_NB_CHARS_PER_LINE)
          bufferOffsetMax = 0; 
        else
          bufferOffsetMax = mlib.buffer_size[currentInputBufferLine] - SCREEN_MAX_NB_CHARS_PER_LINE;
    
        if (mlib.buffer_size[currentInputBufferLine] < SCREEN_MAX_NB_CHARS_PER_LINE)
          select_index_max = mlib.buffer_size[currentInputBufferLine] - 1; 
        else
          select_index_max = SCREEN_MAX_NB_CHARS_PER_LINE-1;

        if (select_index > select_index_max) select_index = select_index_max;
        
        needLettersRefresh=true;
        needSelectorRefresh=true;     
      }

      // Render updated screen
      if (needLettersRefresh) {
        needLettersRefresh=false;
       
        display.setCursor(0,CURSOR_Y_THIRD_LINE);
        for (int i=0; i<SCREEN_MAX_NB_CHARS_PER_LINE; i++) {
          if (i <= select_index_max) {
            char c = mlib.buffers[currentInputBufferLine][bufferOffset+i];
            display.print(c);
          } else {
            display.print(' ');
          }
        }
        display.display();   
      }
      
      // Render updated selectors
      if (needSelectorRefresh) {
        needSelectorRefresh=false;

        // Overwrite previous selectors with blank char
        display.setCursor((old_select_index)*CHAR_XSIZE,CURSOR_Y_SECOND_LINE);
        display.print(' '); // space
        display.setCursor((old_select_index)*CHAR_XSIZE,CURSOR_Y_FOURTH_LINE);
        display.print(' '); // space

        // Redraw selectors at right position
        display.setCursor((select_index)*CHAR_XSIZE,CURSOR_Y_SECOND_LINE);
        display.print((char)31); // triangle pointing down
        display.setCursor((select_index)*CHAR_XSIZE,CURSOR_Y_FOURTH_LINE);
        display.print((char)30); // triangle pointing up
       
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
 
  while (1) {

    check_buttons(BUTTON_REPEAT_TIME_FAST);

    // If validation button was pushed, return currently selected entry
    if (justpressed[YButtonIndex]) {
        if (nbEntries > SCREEN_MAX_NB_LINES) {
          return (mod(selected_index+display_line_index, nbEntries));
        } else {
          return selected_index;
        }
    } 
    else if (justpressed[UpButtonIndex]) {
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
    else if (held[UpButtonIndex]) {
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
    else if (justpressed[DownButtonIndex]) {
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
    else if (held[DownButtonIndex]) {
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
   
   
    if (needRefresh) {


      
    //  unsigned long StartTime = millis();

      needRefresh = false;
      int y = 0;
      for (int i=0; i < SCREEN_MAX_NB_LINES; i++) {


        if (i>= nbEntries) {
          break;
        } else {

        //  if (nbEntries > SCREEN_MAX_NB_LINES) {
        //    getStringFromFlash(menuEntryText, menutexts[mod(selected_index+i,nbEntries)]);
        //  } else {
        //    getStringFromFlash(menuEntryText, menutexts[i]);
        //  }
          display.setCursor((DISPLAY_WIDTH - (strlen(menuEntryText)+4)*CHAR_XSIZE)/2,y);
        
          if (i==display_line_index) {
            display.print((char)16);
            display.print(' ');
          } else {
            display.print("  ");
          }  

      //    display.print(menuEntryText);



          
          y+= CHAR_YSIZE;
        }
      }
          display.display();

 //     unsigned long CurrentTime = millis();
 // unsigned long ElapsedTime = CurrentTime - StartTime;
 // Serial.print("get String time:"); Serial.println(ElapsedTime);        
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


    MultilineInputBuffer mlib;
    mlib.nbBuffers=5;
    mlib.buffers[0]= UpperCaseLetters;
    mlib.buffer_size[0] = strlen(mlib.buffers[0]);
    mlib.buffers[1]= LowerCaseLetters;
    mlib.buffer_size[1] = strlen(mlib.buffers[1]);
    mlib.buffers[2]= SpecialCharacters;
    mlib.buffer_size[2] = strlen(mlib.buffers[2]);
    mlib.buffers[3]= Numbers;
    mlib.buffer_size[3] = strlen(mlib.buffers[3]);
    mlib.buffers[4]= Done;
    mlib.buffer_size[4] = strlen(mlib.buffers[4]);
    
    getStringFromUser(entry.title, ACCOUNT_TITLE_LENGTH, buf, mlib);

    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_LOGIN_INPUT);
    getStringFromUser(entry.data, ACCOUNT_LOGIN_LENGTH, buf, mlib );

    do {
      getStringFromFlash(buf, (uint8_t*)&PASSWORD_LENGTH_INPUT);

      mlib.nbBuffers=2;
      mlib.buffers[0]= Numbers;
      mlib.buffer_size[0] = strlen(mlib.buffers[0]);   
      mlib.buffers[1]= Done;
      mlib.buffer_size[1] = strlen(mlib.buffers[4]);
      
      getStringFromUser(len_string, 2, buf, mlib );
        
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

    MultilineInputBuffer mlib;
    mlib.nbBuffers=5;
    mlib.buffers[0]= UpperCaseLetters;
    mlib.buffer_size[0] = strlen(mlib.buffers[0]);
    mlib.buffers[1]= LowerCaseLetters;
    mlib.buffer_size[1] = strlen(mlib.buffers[1]);
    mlib.buffers[2]= SpecialCharacters;
    mlib.buffer_size[2] = strlen(mlib.buffers[2]);
    mlib.buffers[3]= Numbers;
    mlib.buffer_size[3] = strlen(mlib.buffers[3]);
    mlib.buffers[4]= Done;
    mlib.buffer_size[4] = strlen(mlib.buffers[4]);    
        
    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_TITLE_INPUT);
    getStringFromUser(entry.title, ACCOUNT_TITLE_LENGTH, buf, mlib );

    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_LOGIN_INPUT);
    getStringFromUser(entry.data, ACCOUNT_LOGIN_LENGTH, buf, mlib );
   
    getStringFromFlash(buf, (uint8_t*)&PASSWORD_VALUE_INPUT);
    getStringFromUser((entry.data)+entry.passwordOffset, PASSWORD_MAX_LENGTH, buf, mlib );    

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

    check_buttons(BUTTON_REPEAT_TIME_FAST);

    // If knob switch was pushed, return currently selected entry
    if (justpressed[YButtonIndex]) {
        if (nbEntries > SCREEN_MAX_NB_LINES) {
          return (mod(selected_index+display_line_index, nbEntries));
        } else {
          return selected_index;
        }
    } 
    else if (justpressed[UpButtonIndex] || repeat[UpButtonIndex]) {
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
    else if (justpressed[DownButtonIndex] || repeat[DownButtonIndex]) {
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
    MultilineInputBuffer mlib;
    mlib.nbBuffers=5;
    mlib.buffers[0]= UpperCaseLetters;
    mlib.buffer_size[0] = strlen(mlib.buffers[0]);
    mlib.buffers[1]= LowerCaseLetters;
    mlib.buffer_size[1] = strlen(mlib.buffers[1]);
    mlib.buffers[2]= SpecialCharacters;
    mlib.buffer_size[2] = strlen(mlib.buffers[2]);
    mlib.buffers[3]= Numbers;
    mlib.buffer_size[3] = strlen(mlib.buffers[3]);
    mlib.buffers[4]= Done;
    mlib.buffer_size[4] = strlen(mlib.buffers[4]);    
    
    if( getStringFromUser(user, DEVICENAME_LENGTH, buf, mlib ) )
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



  //memset(buttonState,0,16*sizeof(int));
  //memset(lastButtonState,0,16*sizeof(int));
  //memset(lastDebounceTime,0,16*sizeof(long));

  memset(pressed,0,NUMBUTTONS*sizeof(byte));
  memset(justpressed,0,NUMBUTTONS*sizeof(byte));
  

  
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
  //pinMode(knobInterruptPin, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(knobInterruptPin), uponKnobIT, FALLING);   

  // Setup the pin used for knob management
//  pinMode(knobSwitchPin, INPUT_PULLUP);

  pinMode(UpButtonPin, INPUT_PULLUP);
  pinMode(DownButtonPin, INPUT_PULLUP);
  pinMode(LeftButtonPin, INPUT_PULLUP);
  pinMode(RightButtonPin, INPUT_PULLUP);
  pinMode(YButtonPin, INPUT_PULLUP);


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

