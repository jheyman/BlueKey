#include <TermTool.h>
#include <SPI.h>
#include "eeprom.h"
#include "EncryptedStorage.h"
#include "Entropy.h"
#include "constants.h"
#include "display.h"
#include "utils.h"

// Activate Serial debug traces (or not)
#define DEBUG_ENABLE 0
#define DEBUG(x) if(DEBUG_ENABLE) { x }

// Strings stored in program memory (to spare SRAM)
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

// Menu entries texts
// Rules:
// max 21 characters per line, minus left triangle cursor and one space at the beginning, and one char for arrow cursors on the right  => max 18 chars max per line, including starting and trailing space
// also, to avoid to explicitly clear the display, make all entries the same size so that all text gets overwritten.
                                         // "<-----MAX------>";
const static char MENU_GETPWD[] PROGMEM   = "Get Password  ";
const static char MENU_SETPWD[] PROGMEM   = "Set Password  ";
const static char MENU_CLEARPWD[] PROGMEM = "Clear Password";
const static char MENU_FORMAT[] PROGMEM   = "Format        ";
const static char MENU_TEST1[] PROGMEM    = "TEST1         ";
const static char MENU_TEST2[] PROGMEM    = "TEST2         ";
#define MAIN_MENU_NB_ENTRIES 6

const static char MENU_SETPWD_GENERATE[] PROGMEM      = "Generate";
const static char MENU_SETPWD_MANUALINPUT[] PROGMEM   = "Manually";
#define MENU_SETPWD_NB_ENTRIES 2

const static char MENU_SETPWD_GENERATE_LENGTH[] PROGMEM      = "Length?";

////////////////////////////////
// Gamepad buttons management //
////////////////////////////////

byte buttons[] = {UpButtonPin, DownButtonPin, LeftButtonPin, RightButtonPin, YButtonPin, StartButtonPin};
#define NUMBUTTONS sizeof(buttons)
boolean button_pressed[NUMBUTTONS], button_justpressed[NUMBUTTONS], button_held[NUMBUTTONS];

void check_buttons()
{
  static byte lateststate[NUMBUTTONS];
  static long pressTime[NUMBUTTONS];
  static unsigned long lasttime;
  byte index;

  // reset newly pressed button statuses
  memset(button_justpressed,0,NUMBUTTONS*sizeof(byte));

  // handle case of millis wrapping around
  if (millis()<lasttime) {
     lasttime = millis();
  }

  // if latest check is not yet older than <debounce time>, no need to check yet
  if ((lasttime + DEBOUNCE_TIME) > millis()) {
    return; 
  }
  
  lasttime = millis();

  // Rescan buttons status
  for (index = 0; index<NUMBUTTONS; index++) {
     
    byte newstate = digitalRead(buttons[index]);

    // If two readings in a row give the same value, state is confirmed
    if (newstate == lateststate[index]) {
      // If button was not pressed, but now is (state LOW = input activated):
      if ((button_pressed[index] == false) && (newstate == LOW)) {
          button_justpressed[index] = 1;
          pressTime[index] = millis();
      } else if ((button_pressed[index] == true) && (newstate == HIGH)) {
          button_held[index] = false;
      }
      // Keep track of current confirmed button state
      button_pressed[index] = !newstate;
    }

    // Check is button has been pressed long enough since last press/repeat time
    if (button_pressed[index]) {
           
      if (millis() - pressTime[index] > BUTTON_HOLD_TIME) {
        button_held[index] = true;
      }
    }
        
    lateststate[index] = newstate;   
  }
}
////////////////////////////////
// VARIOUS GUI ROUTINES
////////////////////////////////

bool __attribute__ ((noinline)) confirmChoice(char* text)
{
    int cursorX = (DISPLAY_WIDTH - strlen(text)*CHAR_XSIZE)/2;

    bool confirmed=false;
    bool needRefresh = true;

    // Print question text
    display.clearDisplay();    
    display.setCursor(cursorX,CURSOR_Y_FIRST_LINE);
    display.print(text);     
    display.display();
           
    // Scan buttons indefinitely until choice is validated by user
    while (1) {

      check_buttons();

      // User pushed validation button  
      if (button_justpressed[YButtonIndex]) {
        return confirmed;
      }
      // Change to "Yes"
      else if (button_justpressed[LeftButtonIndex]) {
        if (!confirmed) confirmed=true;
        needRefresh = true;
      }
      // Change to "No"
      else if (button_justpressed[RightButtonIndex]) {
        if (confirmed) confirmed = false;
        needRefresh = true;
      }

      if (needRefresh) {
          needRefresh = false;

          // set cursor so that "Yes No" string is centered
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

      delay(1);  
    }
}

char UpperCaseLetters[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char LowerCaseLetters[]="abcdefghijklmnopqrstuvwxyz";
char SpecialCharacters[]="&~#'@*$!";
char Numbers[]="012345678";

bool __attribute__ ((noinline)) getStringFromUser( char* dst, const uint8_t numChars, const char* invite, MultilineInputBuffer mlib)
{
    byte currentInputBufferLine = 0;
       
    int cursorX = 0;
    int nbCharsValidated = 0;
    byte select_index = 0;
    byte old_select_index = 0;
    byte select_index_max = 0;

    byte bufferOffset = 0;
    byte bufferOffsetMax = 0;

    bool needLettersRefresh=true;
    bool needSelectorRefresh=true;

    // compute scrolling limits 
    if (mlib.buffer_size[currentInputBufferLine] < SCREEN_MAX_NB_CHARS_PER_LINE)
      bufferOffsetMax = 0; 
    else
      bufferOffsetMax = mlib.buffer_size[currentInputBufferLine] - SCREEN_MAX_NB_CHARS_PER_LINE;

    if (mlib.buffer_size[currentInputBufferLine] < SCREEN_MAX_NB_CHARS_PER_LINE)
      select_index_max = mlib.buffer_size[currentInputBufferLine] - 1; 
    else
      select_index_max = SCREEN_MAX_NB_CHARS_PER_LINE-1;

    // Print text invite
    display.clearDisplay();
    display.setCursor(cursorX,0);
    display.print(invite);
    cursorX = display.getCursorX();

    // print the expected number of chars to be input, where they will appear once validated
    for (int n=0; n<numChars; n++) {
      display.print(CODE_SLOT_CHAR);
    }

    // render this initial state
    display.display();
       
    // Scan buttons indefinitely until user has entered max nb of chars, or validated the answer
    while (1) {

      check_buttons();

      // If finish button was pushed, exit
      if (button_justpressed[StartButtonIndex]) {
          break;
      } 
      // If user validated the currently selected character
      else if (button_justpressed[YButtonIndex]) {         
        display.setCursor(cursorX,0);
        display.print(mlib.buffers[currentInputBufferLine][bufferOffset + select_index]);
        display.display();        
        cursorX += CHAR_XSIZE;
          
        dst[nbCharsValidated] = (mlib.buffers[currentInputBufferLine][bufferOffset + select_index]);
        
        nbCharsValidated++;
        // If we have reached max string size, we're done.
        if (nbCharsValidated == numChars)
          break;
      } 
      // Left/Right buttons allow to move character selector
      else if (button_justpressed[RightButtonIndex] || button_held[RightButtonIndex]) {
        if (select_index < select_index_max)
        {
          old_select_index = select_index;
          select_index = select_index + 1;  
          needSelectorRefresh=true;          
        } else {
          if (bufferOffset < bufferOffsetMax) {
            bufferOffset++;   
            needLettersRefresh = true;   
          }
        }
      }  
      else if (button_justpressed[LeftButtonIndex] || button_held[LeftButtonIndex]) {
        if (select_index > 0)
        {
          old_select_index = select_index;
          select_index = select_index - 1;
          needSelectorRefresh=true;          
        } else {
          if (bufferOffset > 0) {
            bufferOffset--;   
            needLettersRefresh = true;   
          }
        }
      }  
      // Up/Down buttons allow to change the currently displayed char set   
      else if (button_justpressed[UpButtonIndex] || button_justpressed[DownButtonIndex]) {
        old_select_index = select_index;
        
        if (button_justpressed[UpButtonIndex] ) {
          currentInputBufferLine = mod(currentInputBufferLine-1,mlib.nbBuffers);
        }
        else if (button_justpressed[DownButtonIndex] ) {
          currentInputBufferLine = mod(currentInputBufferLine+1,mlib.nbBuffers);
        }
        
        if (mlib.buffer_size[currentInputBufferLine] < SCREEN_MAX_NB_CHARS_PER_LINE) {
          bufferOffsetMax = 0; 
          select_index_max = mlib.buffer_size[currentInputBufferLine] - 1; 
        }
        else {
          bufferOffsetMax = mlib.buffer_size[currentInputBufferLine] - SCREEN_MAX_NB_CHARS_PER_LINE;
          select_index_max = SCREEN_MAX_NB_CHARS_PER_LINE-1;
        }            

        if (select_index > select_index_max) select_index = select_index_max;
        if (bufferOffset > bufferOffsetMax) bufferOffset = bufferOffsetMax;
             
        needLettersRefresh=true;
        needSelectorRefresh=true;    
      }

      // Render updated charset line, if updated
      if (needLettersRefresh) {    
        display.setCursor(0,CURSOR_Y_THIRD_LINE);
        for (int i=0; i<SCREEN_MAX_NB_CHARS_PER_LINE; i++) {
          if (i <= select_index_max) {
            char c = mlib.buffers[currentInputBufferLine][bufferOffset+i];
            display.print(c);
          } else {
            display.print(' ');
          }
        }
      }
      
      // Render updated selectors, if updated
      if (needSelectorRefresh) {
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
      }

      // Send modified data to screen when it was changed
      if (needLettersRefresh || needSelectorRefresh) {
        display.display();   
        if (needLettersRefresh) needLettersRefresh= false;
        if (needSelectorRefresh) needSelectorRefresh= false;
     }
      
      delay(1);
    }

    // terminate the C-string
    dst[nbCharsValidated]=0;
    
    return true;
}

int __attribute__ ((noinline)) generic_menu(int nbEntries, uint8_t** menutexts) {
  
  byte menu_line_offset= 0;
  byte menu_line_offset_max = 0;
  byte selector_line_index = 0;
  byte selector_line_index_max = 0;
  char menuEntryText[32];
  bool needMenuRefresh = true;
  bool needSelectorRefresh = true;

  // compute scrolling limits
  if (nbEntries < SCREEN_MAX_NB_LINES) {
    menu_line_offset_max = 0;
    selector_line_index_max = nbEntries-1;    
  } else {
    menu_line_offset_max = nbEntries - SCREEN_MAX_NB_LINES;
    selector_line_index_max = SCREEN_MAX_NB_LINES-1;
  }

  display.clearDisplay();

  // Scan buttons indefinitely until user selects a line
  while (1) {

    check_buttons();

    // If validation button was pushed, return currently selected entry
    if (button_justpressed[YButtonIndex]) {
      return menu_line_offset + selector_line_index;
    }
    // Up/Down buttons allow to scroll through the list 
    else if (button_justpressed[UpButtonIndex] || button_held[UpButtonIndex]) {
      if (selector_line_index>0) {
        selector_line_index--;
        needSelectorRefresh = true;       
      }
      else {
        if (menu_line_offset>0) {
          menu_line_offset--;
          needMenuRefresh = true;
        }
      }
    }
    else if (button_justpressed[DownButtonIndex] || button_held[DownButtonIndex]) {
      if (selector_line_index < selector_line_index_max) {
        selector_line_index++;
        needSelectorRefresh = true;
      }
      else {
        if (menu_line_offset < menu_line_offset_max) {
          menu_line_offset++;  
          needMenuRefresh = true; 
        }
      }     
    }

    // redraw menu texts, if updated
    if (needMenuRefresh) {   
      int y = 0;
      for (int i=0; i < SCREEN_MAX_NB_LINES; i++) {
        if (i>= nbEntries) {
          break;
        } else {
          getStringFromFlash(menuEntryText, menutexts[menu_line_offset+i]);
          display.setCursor(2*CHAR_XSIZE,y);      
          display.print(menuEntryText);         
          y+= CHAR_YSIZE;
        }
      } 

      // If there are extra entries above screen upper limit, display an Up arrow in the upper right corner
      if(menu_line_offset != 0) {
          display.setCursor((SCREEN_MAX_NB_CHARS_PER_LINE-1)*CHAR_XSIZE,CURSOR_Y_FIRST_LINE);      
          display.print((char)24);       
      }
      // If there are no more extra entries above screen upper limit, delete Up arrow char
      else {
          display.setCursor((SCREEN_MAX_NB_CHARS_PER_LINE-1)*CHAR_XSIZE,CURSOR_Y_FIRST_LINE);      
          display.print(' ');       
      }            
      // If there are extra entries below screen lower limit, display a Down arrow in the lower right corner
      if (menu_line_offset != menu_line_offset_max) {
          display.setCursor((SCREEN_MAX_NB_CHARS_PER_LINE-1)*CHAR_XSIZE,CURSOR_Y_FOURTH_LINE);      
          display.print((char)25);  
      }
      // If there are no extra entries below screen lower limit, delete arrow char
      else {
          display.setCursor((SCREEN_MAX_NB_CHARS_PER_LINE-1)*CHAR_XSIZE,CURSOR_Y_FOURTH_LINE);      
          display.print(' ');  
      }            
   }
   
   // Redraw selectors, if they have moved
   if (needSelectorRefresh) {
      int y = 0;
      for (int i=0; i < SCREEN_MAX_NB_LINES; i++) {

        if (i>= nbEntries) {
          break;
        } else {
          
          display.setCursor(0,y);     

          if (i==selector_line_index) {
            display.print((char)16);
            display.print(' ');
          } else {
            display.print("  ");
          }  
          y+= CHAR_YSIZE;
        }
      }
   }

   // Send data to display
   if (needSelectorRefresh || needMenuRefresh) {
      display.display();
      if (needSelectorRefresh) needSelectorRefresh = false;
      if (needMenuRefresh) needMenuRefresh = false;
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
  menutexts[4] =   (uint8_t*)&MENU_TEST1;
  menutexts[5] =   (uint8_t*)&MENU_TEST2;  
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
  char pool[13+10+26+26];
  int pool_index = 0;

  // "!" to "."
  for( uint8_t idx = 33; idx < 47; idx++)
  {
    pool[pool_index++] = idx;
  }

  // "0" to "9"
  for( uint8_t idx = 48; idx < 58; idx++)
  {
    pool[pool_index++] = idx;
  }

  // "A" to "Z"
  for( uint8_t idx = 65; idx < 91; idx++)
  {
    pool[pool_index++] = idx;
  }

  // "a" to "z"
  for( uint8_t idx = 97; idx < 123; idx++)
  {
    pool[pool_index++] = idx;
  }  

  // pick from the pool of letters to fill password randomly
  for( uint8_t idx = 0; idx < len; idx++)
  {
    dst[idx] = pool[(uint8_t)Entropy.random(13+10+26+25)];
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
    delay(MSG_DISPLAY_DELAY);
    return;
  } 
  else {

    memset(&entry, 0, sizeof(entry));
    char buf[16];

    MultilineInputBuffer mlib;
    mlib.nbBuffers=4;
    mlib.buffers[0]= UpperCaseLetters;
    mlib.buffer_size[0] = strlen(mlib.buffers[0]);
    mlib.buffers[1]= LowerCaseLetters;
    mlib.buffer_size[1] = strlen(mlib.buffers[1]);
    mlib.buffers[2]= SpecialCharacters;
    mlib.buffer_size[2] = strlen(mlib.buffers[2]);
    mlib.buffers[3]= Numbers;
    mlib.buffer_size[3] = strlen(mlib.buffers[3]);

    // Query user for entry Title
    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_TITLE_INPUT);  
    getStringFromUser(entry.title, ACCOUNT_TITLE_LENGTH, buf, mlib);

    // Query user for entry login
    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_LOGIN_INPUT);
    getStringFromUser(entry.data, ACCOUNT_LOGIN_LENGTH, buf, mlib );

    // Query user for desired pwd length
    do {
      getStringFromFlash(buf, (uint8_t*)&PASSWORD_LENGTH_INPUT);

      mlib.nbBuffers=1;
      mlib.buffers[0]= Numbers;
      mlib.buffer_size[0] = strlen(mlib.buffers[0]);   
      
      getStringFromUser(len_string, 2, buf, mlib );
        
      entry.passwordOffset = strlen(entry.data)+1;  
      len = atoi(len_string);

      if (len > PASSWORD_MAX_LENGTH){
            displayCenteredMessageFromStoredString((uint8_t*)&INVALID_VALUE);
            delay(MSG_DISPLAY_DELAY);
      }
    } while(len > PASSWORD_MAX_LENGTH);
    
    putRandomChars( ((entry.data)+entry.passwordOffset),len);

    display.clearDisplay();    

    displayCenteredMessageFromStoredString((uint8_t*)&STORING_NEW_PASSWORD);
    delay(MSG_DISPLAY_DELAY);
  
    ES.putEntry( (uint8_t)entryNum, &entry );
  } 
}

void __attribute__ ((noinline)) inputPassword() {
  uint16_t entryNum = ES.getNextEmpty();
  entry_t entry;

  if( entryNum == NUM_ENTRIES ) {
    displayCenteredMessageFromStoredString((uint8_t*)&DEVICE_FULL);
    delay(MSG_DISPLAY_DELAY);
    return;
  } 
  else {

    memset(&entry, 0, sizeof(entry));
    char buf[16];

    MultilineInputBuffer mlib;
    mlib.nbBuffers=4;
    mlib.buffers[0]= UpperCaseLetters;
    mlib.buffer_size[0] = strlen(mlib.buffers[0]);
    mlib.buffers[1]= LowerCaseLetters;
    mlib.buffer_size[1] = strlen(mlib.buffers[1]);
    mlib.buffers[2]= SpecialCharacters;
    mlib.buffer_size[2] = strlen(mlib.buffers[2]);
    mlib.buffers[3]= Numbers;
    mlib.buffer_size[3] = strlen(mlib.buffers[3]);  

    // Query user for entry Title       
    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_TITLE_INPUT);
    getStringFromUser(entry.title, ACCOUNT_TITLE_LENGTH, buf, mlib );

    // Query user for entry login
    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_LOGIN_INPUT);
    getStringFromUser(entry.data, ACCOUNT_LOGIN_LENGTH, buf, mlib );

    // Query user for entry pwd
    getStringFromFlash(buf, (uint8_t*)&PASSWORD_VALUE_INPUT);
    getStringFromUser((entry.data)+entry.passwordOffset, PASSWORD_MAX_LENGTH, buf, mlib );    

    display.clearDisplay();    

    displayCenteredMessageFromStoredString((uint8_t*)&STORING_NEW_PASSWORD);
    delay(MSG_DISPLAY_DELAY);

    ES.putEntry( (uint8_t)entryNum, &entry );   
  } 
}

// Display a scrolling list of stored entries, and wait for user to select one
int __attribute__ ((noinline)) pickEntry() {

  byte menu_line_offset= 0;
  byte menu_line_offset_max = 0;
  byte selector_line_index = 0;
  byte selector_line_index_max = 0;
  char menuEntryText[32];
  bool needMenuRefresh = true;
  bool needSelectorRefresh = true;

  uint8_t entryNum=0;
  uint8_t nbEntries=0;
  bool hasEntry=false;
  uint8_t maxEntryLength=0;

  // compute total number of entries
  do
  {
    hasEntry = ES.getTitle(entryNum, menuEntryText);
    if(hasEntry)
    {
      DEBUG(Serial.print("Entry ");)
      DEBUG(Serial.print(entryNum);)
      DEBUG(Serial.print(": ");)
      DEBUG(Serial.println(menuEntryText);)
      int len = strlen(menuEntryText);
      if (len>maxEntryLength) maxEntryLength = len;
      entryNum++;
    }
  } while (hasEntry);

  nbEntries = entryNum;

  if (nbEntries==0) return -1;

  // compute scrolling limits
  if (nbEntries < SCREEN_MAX_NB_LINES) {
    menu_line_offset_max = 0;
    selector_line_index_max = nbEntries-1;   
  } else {
    menu_line_offset_max = nbEntries - SCREEN_MAX_NB_LINES;
    selector_line_index_max = SCREEN_MAX_NB_LINES-1;
  }

  display.clearDisplay();

  // Scan buttons indefinitely until user validates a selected item
  while (1) {

    check_buttons();

    // If validation button was pushed, return currently selected entry
    if (button_justpressed[YButtonIndex]) {
          return menu_line_offset + selector_line_index;
    } 
    // Up/Down buttons scroll through the list
    else if (button_justpressed[UpButtonIndex] || button_held[UpButtonIndex]) {
      if (selector_line_index>0) {
        selector_line_index--;
        needSelectorRefresh = true;       
      }
      else {
        if (menu_line_offset>0) {
          menu_line_offset--;
          needMenuRefresh = true;
        }
      }
    } 
    else if (button_justpressed[DownButtonIndex] || button_held[DownButtonIndex]) {
      if (selector_line_index < selector_line_index_max) {
        selector_line_index++;
        needSelectorRefresh = true;
      }
      else {
        if (menu_line_offset < menu_line_offset_max) {
          menu_line_offset++;  
          needMenuRefresh = true; 
        }
      }     
    }

    // Render modified text entries if updated
    if (needMenuRefresh) {   
      int y = 0;
      for (int i=0; i < SCREEN_MAX_NB_LINES; i++) {
        if (i>= nbEntries) {
          break;
        } else {

          ES.getTitle(menu_line_offset+i, menuEntryText);
          int entryLen = strlen(menuEntryText);
          display.setCursor(2*CHAR_XSIZE,y);
          display.print(menuEntryText);
          if (entryLen<maxEntryLength) {
             for (int k=0;k<SCREEN_MAX_NB_CHARS_PER_LINE-entryLen-2;k++){
               display.print(' ');
             }
           }        
          y+= CHAR_YSIZE;
        }
      } 

      // If there are extra entries above screen upper limit, display an Up arrow in the upper right corner
      if(menu_line_offset != 0) {
          display.setCursor((SCREEN_MAX_NB_CHARS_PER_LINE-1)*CHAR_XSIZE,CURSOR_Y_FIRST_LINE);      
          display.print((char)24);       
      }
      // If there are no more extra entries above screen upper limit, delete Up arrow char
      else {
          display.setCursor((SCREEN_MAX_NB_CHARS_PER_LINE-1)*CHAR_XSIZE,CURSOR_Y_FIRST_LINE);      
          display.print(' ');       
      }            
      // If there are extra entries below screen lower limit, display a Down arrow in the lower right corner
      if (menu_line_offset != menu_line_offset_max) {
          display.setCursor((SCREEN_MAX_NB_CHARS_PER_LINE-1)*CHAR_XSIZE,CURSOR_Y_FOURTH_LINE);      
          display.print((char)25);  
      }
      // If there are no extra entries below screen lower limit, delete arrow char
      else {
          display.setCursor((SCREEN_MAX_NB_CHARS_PER_LINE-1)*CHAR_XSIZE,CURSOR_Y_FOURTH_LINE);      
          display.print(' ');  
      }        
   }

   // Render updated selector chars, if they moved
   if (needSelectorRefresh) {
      int y = 0;
      for (int i=0; i < SCREEN_MAX_NB_LINES; i++) {

        if (i>= nbEntries) {
          break;
        } else {   
          display.setCursor(0,y);
          if (i==selector_line_index) {
            display.print((char)16);
            display.print(' ');
          } else {
            display.print("  ");
          }  
          y+= CHAR_YSIZE;
        }
      }
   }

   if (needSelectorRefresh || needMenuRefresh) {
      display.display();
      if (needSelectorRefresh) needSelectorRefresh = false;
      if (needMenuRefresh) needMenuRefresh = false;
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

  // Query desired code twice from user, verify both match to validate code.
  while(fail)
  {
    MultilineInputBuffer mlib;
    mlib.nbBuffers=1;
    mlib.buffers[0]= Numbers;
    mlib.buffer_size[0] = strlen(mlib.buffers[0]);
        
    getStringFromFlash(buf, (uint8_t*)&NEW_CODE_INVITE);  
    getStringFromUser(code1, USERCODE_LENGTH, buf, mlib);
       
    getStringFromFlash(buf, (uint8_t*)&REPEAT_CODE_INVITE);
    getStringFromUser(code2, USERCODE_LENGTH, buf, mlib);

    if(memcmp(code1,code2,USERCODE_LENGTH)==0)
    {
      fail=0;
    }
    
    if(fail) {
      displayCenteredMessageFromStoredString((uint8_t*)&NEW_CODE_MISMATCH);
      delay(MSG_DISPLAY_DELAY);
    }      
  }
  fail=1;

  // Query device name from user
  while(fail)
  {
    getStringFromFlash(buf, (uint8_t*)&NAME_INPUT);
    MultilineInputBuffer mlib;
    mlib.nbBuffers=4;
    mlib.buffers[0]= UpperCaseLetters;
    mlib.buffer_size[0] = strlen(mlib.buffers[0]);
    mlib.buffers[1]= LowerCaseLetters;
    mlib.buffer_size[1] = strlen(mlib.buffers[1]);
    mlib.buffers[2]= SpecialCharacters;
    mlib.buffer_size[2] = strlen(mlib.buffers[2]);
    mlib.buffers[3]= Numbers;
    mlib.buffer_size[3] = strlen(mlib.buffers[3]);
    
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

  // Request passcode from user
  getStringFromFlash(buf, (uint8_t*)&CODE_INVITE);

  MultilineInputBuffer mlib;
  mlib.nbBuffers=1;
  mlib.buffers[0]= Numbers;
  mlib.buffer_size[0] = strlen(mlib.buffers[0]);   
 
  getStringFromUser(key, USERCODE_LENGTH, buf, mlib );

  // Use provided code to unlock storage and proceed
  if(ES.unlock((byte*)key)) {
     ret=1;
     displayCenteredMessageFromStoredString((uint8_t*)&LOGIN_GRANTED);
      delay(MSG_DISPLAY_DELAY);
  } else {
     displayCenteredMessageFromStoredString((uint8_t*)&LOGIN_DENIED);
      delay(MSG_DISPLAY_DELAY);
  }

  return ret;
}

////////////////////
// INITIALISATION
////////////////////
void setup()   {  

  // Initialize stack canary
  DEBUG(paintStack();)
    
  char devName[DEVNAME_BUFF_LEN];
  memset(devName,0,DEVNAME_BUFF_LEN);

  // initialize button state
  memset(button_pressed,0,NUMBUTTONS*sizeof(byte));
  
  // RN42 is configured by default to use 115200 bauds on UART links
  Serial.begin(115200);
        
  DEBUG(printSRAMMap(););

  DEBUG(
  int x = StackMarginWaterMark();
  Serial.print("initial Watermark : "); Serial.println(x);
  Serial.print("Current stack size: ");  Serial.println((RAMEND - SP), DEC);)

  Entropy.initialize();
  setRng();
  
  // Initialize OLED display
  display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDR);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  
  // Setup input I/Os connected to buttons
  for (byte i=0; i < NUMBUTTONS; i++) {
    pinMode(buttons[i], INPUT_PULLUP);
  }

  // debug feature to force reformatting
  //messUpEEPROMFormat();

  // Check if the expected header is found in the EEPROM, else trig a format
  if(!ES.readHeader(devName)) {
    displayCenteredMessageFromStoredString((uint8_t*)&NEED_FORMAT);
    delay(MSG_DISPLAY_DELAY);
    format();
  } else {
    char buf[DEVICENAME_LENGTH+11];
    sprintf(buf, "BLUEKEY (%s)", devName);
    displayCenteredMessage(buf);
    delay(MSG_DISPLAY_DELAY);
  }
  
  // Login now
  bool login_status = false;
  do {
    login_status = login();
  } while (login_status==false);
}

void testFunction1() {

  display.clearDisplay();
  display.setCursor(0,0);
  // 24 UP
  // 25 DOWN
  // 26 RIGHT
  // 27 LEFT
  for (char c=24; c<28; c++) {
    display.print((char)c);
  }
  display.display();

  delay(5000);
}

void testFunction2() {

}

////////////////////
// MAIN LOOP
////////////////////
void loop() {

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
        
        // This is where the critical step happens: send the decoded password over the serial link (to Bluetooth or wired keyboard interface)
        // Send password to Serial TX line (hence to Bluetooth module)
        Serial.print(temp.data+temp.passwordOffset);
      } else
            displayCenteredMessageFromStoredString((uint8_t*)&DEVICE_EMPTY);
      break;
    case 1:
      // SET_PWD              
      entry_choice = menu_set_pwd();
      switch (entry_choice) {
        case 0:
          // GENERATED
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
      if (confirmChoice((char*)"del entry?")) ES.delEntry(entry_choice);
      break;
    case 3:
      // FORMAT
      if (confirmChoice((char*)"Sure?")) format();
      break;
    case 4:
      // TEST
      testFunction1();
      break;      
    case 5:
      // TEST
      testFunction2();
      break; 
  }

  DEBUG(
  int x = StackMarginWaterMark();
  Serial.print("Watermark after main loop action: "); Serial.println(x); )
}

