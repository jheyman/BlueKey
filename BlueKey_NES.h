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
const static char BT_ADDRESS_INPUT[] PROGMEM = "BT@: ";
const static char WHATEVER_INVITE[] PROGMEM = "TEST:";
const static char NEW_CODE_MISMATCH[] PROGMEM = "ERROR: mismatch";
const static char INVALID_VALUE[] PROGMEM = "ERROR: invalid value";
const static char DEVICE_EMPTY[] PROGMEM = "Device is empty";
const static char LOGIN_SENT[] PROGMEM = "Login sent";
const static char PASSWORD_SENT[] PROGMEM = "Password sent";
const static char LOGIN_PASSWORD_SENT[] PROGMEM = "Login & Pwd sent";
const static char NEED_FORMAT[] PROGMEM = "Format needed";
const static char LOGIN_GRANTED[] PROGMEM = "OK";
const static char LOGIN_DENIED[] PROGMEM = "DENIED";
const static char DEVICE_FULL[] PROGMEM = "Error: device full";
const static char ACCOUNT_TITLE_INPUT[] PROGMEM = "Account? ";
const static char ACCOUNT_LOGIN_INPUT[] PROGMEM = "Login? ";
const static char PASSWORD_LENGTH_INPUT[] PROGMEM = "Length? [0-16] ";
const static char PASSWORD_VALUE_INPUT[] PROGMEM = "Pwd? ";
const static char STORING_NEW_PASSWORD[] PROGMEM = "Storing...";
const static char DELETING_ENTRY[] PROGMEM = "Deleting...";

// Menu entries texts
// Rules:
// max 21 characters per line, minus left triangle cursor and one space at the beginning, and one char for arrow cursors on the right  => max 18 chars max per line, including starting and trailing space
// also, to avoid to explicitly clear the display, make all entries the same size so that all text gets overwritten.
                                            // "<-----MAX------>";
const static char MENU_GETPWD[] PROGMEM      = "Password List   ";
const static char MENU_MANAGEPWD[] PROGMEM   = "Manage Passwords";
const static char MENU_SETUP[] PROGMEM       = "Setup           ";
#define MAIN_MENU_NB_ENTRIES 3

const static char MENU_SETPWD[] PROGMEM      = "New Password   ";
const static char MENU_CLEARPWD[] PROGMEM    = "Delete Password";
const static char MENU_FORMAT[] PROGMEM      = "Format         ";
const static char MENU_NB_ENTRIES[] PROGMEM  = "Check entries  ";
//const static char MENU_TEST1[] PROGMEM       = "TEST1          ";
//const static char MENU_TEST2[] PROGMEM       = "TEST2          ";
#define MENU_MANAGE_PASSWORDS_NB_ENTRIES 6
#define MENU_MANAGE_PASSWORDS_NB_ENTRIES 4

const static char MENU_SETPWD_GENERATE[] PROGMEM    = "Generate";
const static char MENU_SETPWD_MANUALINPUT[] PROGMEM = "Manually";
#define MENU_SETPWD_NB_ENTRIES 2

const static char MENU_SETPWD_GENERATE_LENGTH[] PROGMEM = "Length?";

const static char MENU_SENDPWD_LOGINONLY[] PROGMEM = "Login only   ";
const static char MENU_SENDPWD_PWDONLY[] PROGMEM   = "Password only";
const static char MENU_SENDPWD_LOGINPWD[] PROGMEM  = "Login/Tab/Pwd";
#define MENU_SENDPWD_NB_ENTRIES 3

const static char MENU_SETUP_CONFIG_BT_MODULE[] PROGMEM   = "BT configuration";
const static char MENU_SETUP_CONNECT_BT_MODULE[] PROGMEM  = "BT force connect";
#define MENU_SETUP_NB_ENTRIES 2

////////////////////////////////
// Gamepad buttons management //
////////////////////////////////

byte buttons[] = {UpButtonPin, DownButtonPin, LeftButtonPin, RightButtonPin, AButtonPin, BButtonPin, StartButtonPin, SelectButtonPin };
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
          button_justpressed[index] = true;
          pressTime[index] = millis();
      } else if ((button_pressed[index] == true) && (newstate == HIGH)) {
          button_held[index] = false;
      }
      // Keep track of current confirmed button state
      button_pressed[index] = !newstate;
    } 

    // Check if button has been pressed long enough since last press/repeat time
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
      if (button_justpressed[AButtonIndex]) {
        return confirmed;
      }
      // Cancel button pushed
      else if (button_justpressed[BButtonIndex]) {
        return false;
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
char SpecialCharacters[]="@!\"#$%&'()*+,-./";
char Numbers[]="0123456789";

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
      // Cancel button pushed
      if (button_justpressed[BButtonIndex]) {
          return false;
      }       
      // If user validated the currently selected character
      else if (button_justpressed[AButtonIndex]) {         
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
  char menuEntryText[ENTRY_TITLE_SIZE];
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
    if (button_justpressed[AButtonIndex]) {
      return menu_line_offset + selector_line_index;
    }
    // Cancel button pushed
    else if (button_justpressed[BButtonIndex]) {
      return RET_CANCEL;
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
  menutexts[1] =   (uint8_t*)&MENU_MANAGEPWD;
  menutexts[2] =   (uint8_t*)&MENU_SETUP;
  return generic_menu(MAIN_MENU_NB_ENTRIES, menutexts);
}

int __attribute__ ((noinline)) menu_manage_passwords() {
  uint8_t* menutexts[MENU_MANAGE_PASSWORDS_NB_ENTRIES];
  menutexts[0] =   (uint8_t*)&MENU_SETPWD;
  menutexts[1] =   (uint8_t*)&MENU_CLEARPWD;
  menutexts[2] =   (uint8_t*)&MENU_FORMAT;
  menutexts[3] =   (uint8_t*)&MENU_NB_ENTRIES;  
  //menutexts[4] =   (uint8_t*)&MENU_TEST1;
  //menutexts[5] =   (uint8_t*)&MENU_TEST2;  
  return generic_menu(MENU_MANAGE_PASSWORDS_NB_ENTRIES, menutexts);
}

int __attribute__ ((noinline)) menu_set_pwd() {
  uint8_t* menutexts[MENU_SETPWD_NB_ENTRIES];
  menutexts[0] =   (uint8_t*)&MENU_SETPWD_GENERATE;
  menutexts[1] =   (uint8_t*)&MENU_SETPWD_MANUALINPUT;
  return generic_menu(MENU_SETPWD_NB_ENTRIES, menutexts);
}

int __attribute__ ((noinline)) menu_send_pwd() {
  uint8_t* menutexts[MENU_SENDPWD_NB_ENTRIES];
  menutexts[0] =   (uint8_t*)&MENU_SENDPWD_LOGINONLY;
  menutexts[1] =   (uint8_t*)&MENU_SENDPWD_PWDONLY;
  menutexts[2] =   (uint8_t*)&MENU_SENDPWD_LOGINPWD;
  return generic_menu(MENU_SENDPWD_NB_ENTRIES, menutexts);
}

int __attribute__ ((noinline)) menu_setup() {
  uint8_t* menutexts[MENU_SETUP_NB_ENTRIES];
  menutexts[0] =   (uint8_t*)&MENU_SETUP_CONFIG_BT_MODULE;
  menutexts[1] =   (uint8_t*)&MENU_SETUP_CONNECT_BT_MODULE;
  return generic_menu(MENU_SETUP_NB_ENTRIES, menutexts);
}


void __attribute__ ((noinline)) putRandomChars( char* dst, uint8_t len)
{
  char pool[14+10+26+26];
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
    dst[idx] = pool[(uint8_t)Entropy.random(13+10+26+26)];
  }

  // terminate string
  dst[len]=0;
}

void __attribute__ ((noinline)) generatePassword() {
  uint16_t entryNum = ES.getNbEntries();
  entry_t entry;
  char len_string[3];
  int len;
  bool validated=false;

  if( entryNum == NUM_ENTRIES ) {
    displayCenteredMessageFromStoredString((uint8_t*)&DEVICE_FULL);
    delay(MSG_DISPLAY_DELAY);
    return;
  } 
  else {

    memset(&entry, 0, sizeof(entry));
    char buf[32];

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
    validated = getStringFromUser(entry.title, ACCOUNT_TITLE_LENGTH, buf, mlib);
    
    // CANCEL management
    if (!validated) return;
    
    // Query user for entry login
    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_LOGIN_INPUT);
    validated = getStringFromUser(entry.data, ACCOUNT_LOGIN_LENGTH, buf, mlib );
    
    // CANCEL management
    if (!validated) return;
    
    // Query user for desired pwd length
    do {
      getStringFromFlash(buf, (uint8_t*)&PASSWORD_LENGTH_INPUT);

      mlib.nbBuffers=1;
      mlib.buffers[0]= Numbers;
      mlib.buffer_size[0] = strlen(mlib.buffers[0]);   
      
      validated = getStringFromUser(len_string, 2, buf, mlib );

      // CANCEL management
      if (!validated) return;
    
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
   
    ES.insertEntry(&entry);
    //delay(MSG_DISPLAY_DELAY);
  } 
}

void __attribute__ ((noinline)) inputPassword() {
  uint16_t entryNum = ES.getNbEntries();
  entry_t entry;
  bool validated=false;

  if( entryNum == NUM_ENTRIES ) {
    displayCenteredMessageFromStoredString((uint8_t*)&DEVICE_FULL);
    delay(MSG_DISPLAY_DELAY);
    return;
  } 
  else {

    memset(&entry, 0, sizeof(entry));
    char buf[ENTRY_TITLE_SIZE+1];

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
    validated = getStringFromUser(entry.title, ACCOUNT_TITLE_LENGTH, buf, mlib );
    // CANCEL management
    if (!validated) return;
    
    // Query user for entry login
    getStringFromFlash(buf, (uint8_t*)&ACCOUNT_LOGIN_INPUT);
    validated = getStringFromUser(entry.data, ACCOUNT_LOGIN_LENGTH, buf, mlib );
    // CANCEL management
    if (!validated) return;
    
    // Query user for entry pwd
    getStringFromFlash(buf, (uint8_t*)&PASSWORD_VALUE_INPUT);
    entry.passwordOffset = strlen(entry.data)+1;
    validated = getStringFromUser((entry.data)+entry.passwordOffset, PASSWORD_MAX_LENGTH, buf, mlib );    
    // CANCEL management
    if (!validated) return;
    
    display.clearDisplay();    

    displayCenteredMessageFromStoredString((uint8_t*)&STORING_NEW_PASSWORD);

    ES.insertEntry( &entry );  

    //delay(MSG_DISPLAY_DELAY);     
  } 
}

// Display a scrolling list of stored entries, and wait for user to select one
int __attribute__ ((noinline)) pickEntry() {

  byte menu_line_offset= 0;
  byte menu_line_offset_max = 0;
  byte selector_line_index = 0;
  byte selector_line_index_max = 0;
  char menuEntryText[ENTRY_TITLE_SIZE];
  bool needMenuRefresh = true;
  bool needSelectorRefresh = true;

  uint8_t nbEntries=0;
  uint8_t entryIdx=0;
  bool hasEntry=false;
  uint8_t maxEntryLength=0;

  // get stats about stored entries
  nbEntries = ES.getNbEntries();

  if (nbEntries==0) return RET_EMPTY;

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
    if (button_justpressed[AButtonIndex]) {
          return menu_line_offset + selector_line_index;
    }
    else if (button_justpressed[BButtonIndex]) {
          return RET_CANCEL;
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
    // Using NES slect/start buttons to do page down / page up in the list
    else if (button_justpressed[SelectButtonIndex] || button_held[SelectButtonIndex]) {
        if (menu_line_offset_max > 0) {
          if (menu_line_offset>3) {
            menu_line_offset = menu_line_offset - 4;
          } else {
            menu_line_offset = 0;
          }
           needMenuRefresh = true;
  
          selector_line_index = 0;
          needSelectorRefresh = true;
        }
    } 
    else if (button_justpressed[StartButtonIndex] || button_held[StartButtonIndex]) {

        if (menu_line_offset_max > 0) {
          if (menu_line_offset < (menu_line_offset_max-4)) {
            menu_line_offset = menu_line_offset + 4;
          } else {
            menu_line_offset = menu_line_offset_max;
          }
           needMenuRefresh = true;
           needSelectorRefresh = true;
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
          // keep track of longest title to optimize nb of characters needing redraw
          if (entryLen > maxEntryLength) maxEntryLength = entryLen;
          display.setCursor(2*CHAR_XSIZE,y);
          display.print(menuEntryText);
          if (entryLen<maxEntryLength) {
             for (int k=0;k<maxEntryLength-entryLen;k++){
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
  analogWrite(ENTROPY_PIN, 250);
  randomSeed( Entropy.random() );
  digitalWrite(ENTROPY_PIN,1);
}

void __attribute__ ((noinline)) getDeviceBTAddress()
{
  char BTAddress[USERDATA_BT_ADDRESS_LEN+1];  
  char buf[32];
  
  // Print invite
  getStringFromFlash(buf, (uint8_t*)&BT_ADDRESS_INPUT);
  
  // Only uppercase letters and numbers are required
  MultilineInputBuffer mlib;
  mlib.nbBuffers=2;
  mlib.buffers[0]= UpperCaseLetters;
  mlib.buffer_size[0] = strlen(mlib.buffers[0]);
  mlib.buffers[1]= Numbers;
  mlib.buffer_size[1] = strlen(mlib.buffers[1]);

  // Query device BT address
  getStringFromUser(BTAddress, USERDATA_BT_ADDRESS_LEN, buf, mlib );
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

void testButtons() {
  bool needRefresh=true;
   display.clearDisplay();

  // Scan buttons indefinitely until user selects a line
  while (1) {

   check_buttons();

    for (byte i=0; i < NUMBUTTONS; i++) {
      bool pushed = button_pressed[i];
      
      if (pushed) {
        needRefresh = true;
        display.setCursor(0,0);
        display.print("           ");
        display.setCursor(0,0);
        switch(i) {
          case 0:
            display.print("Up"); 
            break;
          case 1:
            display.print("Down"); break;
          case 2:
            display.print("Left"); break;
          case 3:
            display.print("Right"); break;
          case 4:
            display.print("A"); break;
          case 5:
            display.print("B"); break;            
          case 6:
            display.print("Start"); break;
          case 7:
            display.print("Select"); break;
          default:
            break;                                                                                                  
        }
      }
    }

   if (needRefresh) {
      needRefresh=false;
    display.display();
/*
        delay(100);
        display.setCursor(0,0);
        display.print("           ");
        display.setCursor(0,0);
        display.display(); 
        */     
   }
        
   delay(1);
  }
}

void configureRN42() {

  char targetBTAddress[USERDATA_BT_ADDRESS_LEN+1];  
  char buf[32];
  
  // Print invite
  getStringFromFlash(buf, (uint8_t*)&BT_ADDRESS_INPUT);
  
  // Only uppercase letters and numbers are required
  MultilineInputBuffer mlib;
  mlib.nbBuffers=2;
  mlib.buffers[0]= UpperCaseLetters;
  mlib.buffer_size[0] = strlen(mlib.buffers[0]);
  mlib.buffers[1]= Numbers;
  mlib.buffer_size[1] = strlen(mlib.buffers[1]);

  // Query device BT address from user
  getStringFromUser(targetBTAddress, USERDATA_BT_ADDRESS_LEN, buf, mlib );

  // Device should not be connected over bluetooth at this point

  // Enter RN41 command mode
  Serial.print("$$$");
  delay(250);

  // Restore factory settings
  Serial.print("SF,1\n");
  delay(250);

  // Setup the HID profile so that device is recognized as a keyboard
  Serial.print("S~,6\n");
  delay(250);

  // Setup the name prefix that will appear on the remote device
  Serial.print("S-,bluekey\n");
  delay(250);

  // Store the BT address of the remote device, for automatic connection at power-up
  Serial.print("SR,");
  Serial.print(targetBTAddress);
  Serial.print("\n");
  delay(250);

  // Reboot
  Serial.print("R,1\n");
}

void connectRN42() {

  // Enter RN41 command mode
  Serial.print("$$$");
  delay(250);
 
  // Connect using pre-stored BT address (via SR,<...> command)
  Serial.print("C\n"); 
}
////////////////////
// INITIALISATION
////////////////////
void setup()   {  
 
  // Initialize stack canary
  DEBUG(paintStack();)

  // Initialize OLED display
  display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDR);
  display.ssd1306_command(SSD1306_SEGREMAP );
  display.ssd1306_command(SSD1306_COMSCANINC);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);

  // Print banner
  char tmp[8];
  strcpy(tmp, "BLUEKEY");
  displayCenteredMessage(tmp); 
  delay(MSG_DISPLAY_DELAY);
 
  char devName[DEVNAME_BUFF_LEN];
  memset(devName,0,DEVNAME_BUFF_LEN);

  // initialize button state
  memset(button_pressed,0,NUMBUTTONS*sizeof(byte));
          
  DEBUG(printSRAMMap(););

  DEBUG(
  int x = StackMarginWaterMark();
  Serial.print("initial Watermark : "); Serial.println(x);
  Serial.print("Current stack size: ");  Serial.println((RAMEND - SP), DEC);)

  // RN42 is configured by default to use 115200 bauds on UART links
  Serial.begin(115200);

  // Auto-connect to stored remote bluetooth device address
  // may fail if no device BT address was set yet, but is harmless
  connectRN42();
      
  // Setup input I/Os connected to buttons
  for (byte i=0; i < NUMBUTTONS; i++) {
    pinMode(buttons[i], INPUT_PULLUP);
  }

  // debug feature to force reformatting
  //messUpEEPROMFormat();
  
  // debug feature to check buttons connections/pin mapping
  //testButtons();
  
  pinMode(ENTROPY_PIN, OUTPUT);
  Entropy.initialize();
  setRng();

  ES.initialize();
  
  // Check if the expected header is found in the EEPROM, else trig a format
  if(!ES.readHeader(devName)) {
    displayCenteredMessageFromStoredString((uint8_t*)&NEED_FORMAT);
    delay(MSG_DISPLAY_DELAY);
    format();
  }
  
  // Login now
  bool login_status = false;
  do {
    login_status = login();
  } while (login_status==false);
}

void printNbEntries()
{
  
  display.clearDisplay();
  display.setCursor(0,0);
  int nbEntries = ES.getNbEntries();
  display.print(nbEntries);
  display.print('/');
  display.print(NUM_ENTRIES);
  display.display();
  delay(2000);

}

// this function is mostly here to document the way to READ responses
// from RN42 while in command mode.
void getRN42FirmwareVersion() {

  display.clearDisplay();
  display.setCursor(0,0);

  // Enter RN42 command mode
  Serial.print("$$$");
  delay(50);

  // By default the RN42 uses 115200 bauds in RX and TX
  // But somehow the arduino pro mini cannot handle the 115200 bauds in the RX direction
  // So set RN42 temporary baud rate to 9600
  Serial.print("U,9600,N\n");

  // Change baud rate on arduino TX side to 9600 too
  Serial.flush();
  Serial.begin(9600);
  delay(250);

  // Flush RX buffer just in case  
  while(Serial.available()) Serial.read();
  
  // Enter command mode again
  Serial.print("$$$");
  
  // wait a bit to make sure response ("CMD") has started arriving
  delay(5);

  // read & display incoming response bytes
  while(Serial.available()) {
    display.print((char)Serial.read());
  }

  // Send command to get RN42 firmware version
  delay(50);
  Serial.print("V\n");
  
  // wait a bit to make sure response (FW version text) has started arriving
  delay(10);

  // read & display incoming response bytes
  while(Serial.available()) {
    display.print((char)Serial.read());
  }

  // Exit command mode
  delay(250); 
  Serial.print("---\n");
  
  // wait a bit to make sure response ("END") has started arriving
  delay(10);

  // read & display incoming response bytes
  while(Serial.available()) {
    display.print((char)Serial.read());
  }
 
  display.display();

  // Wait here until B button is pushed 
  while (1) {
  
      check_buttons();
  
      if (button_justpressed[BButtonIndex]) {
        break;
      }
  
    delay(1);
  }

  // Restore arduino baud rate to 115200.
  Serial.flush();
  Serial.begin(115200);
}

void testFunction1() {

  int idx = 0;
  entry_t entry;
  int res;

  for (int k=0; k<20; k++) {
  
    int idx = ES.getNbEntries();
   
    putRandomChars(entry.title,10);
    putRandomChars(entry.data,10);
    entry.passwordOffset = strlen(entry.data)+1;
    putRandomChars( ((entry.data)+entry.passwordOffset),10);

    res = ES.insertEntry(&entry);

    display.clearDisplay();    
    display.setCursor(0,0);
    display.print("test "); display.println(idx);
    display.print("res="); display.print(res);
    display.display();
  }
}

void testFunction2() {

  int idx = ES.getNbEntries();
  entry_t entry;

  char c = 65 + Entropy.random(20);
  sprintf(entry.title, "testtitle%c_%d", c, idx);
  strcpy(entry.data, "testlogin");
  entry.passwordOffset = strlen(entry.data)+1;
  strcpy(entry.data+entry.passwordOffset, "testpwd");

  ES.insertEntry(&entry);
}

////////////////////
// MAIN LOOP
////////////////////
void loop() {

  int entry_choice1=-1,entry_choice2=-1;
  
  // Launch main menu
  int selection = main_menu();

  switch (selection) {
    entry_t temp;

    case MAIN_MENU_SENDPWD:
      // Let user pick an entry from a list
      entry_choice1 = pickEntry();
      if (entry_choice1 == RET_EMPTY)  {
        displayCenteredMessageFromStoredString((uint8_t*)&DEVICE_EMPTY);
        delay(MSG_DISPLAY_DELAY);
      }
      else if (entry_choice1 != RET_CANCEL) {
        
        DEBUG( Serial.print("picked entry:"); )
        DEBUG( Serial.println(entry_choice1); )
        ES.getEntry(entry_choice1, &temp);
        DEBUG( Serial.print("password for this entry:"); )

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // This is where the critical step happens: send the decoded login and/or password over the serial link (to Bluetooth or wired keyboard interface)
        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        entry_choice2 = 0;
        // Stay in this menu until cancel button is pressed. This is to allow to send login then password (for example) without having to re-select the menu twice.
        while (entry_choice2 != RET_CANCEL) {
          entry_choice2 = menu_send_pwd();
          switch (entry_choice2) {
            case SENDPWD_MENU_LOGINONLY:
              Serial.print(temp.data);
              displayCenteredMessageFromStoredString((uint8_t*)&LOGIN_SENT);
              delay(MSG_DISPLAY_DELAY);               
              break;
            case SENDPWD_MENU_PWDNONLY:
              Serial.print(temp.data+temp.passwordOffset); 
              displayCenteredMessageFromStoredString((uint8_t*)&PASSWORD_SENT);
              delay(MSG_DISPLAY_DELAY);       
              break;
            case SENDPWD_MENU_LOGIN_TAB_PWD:
              Serial.print(temp.data);
              Serial.print((char)9); // tab key
              Serial.print(temp.data+temp.passwordOffset);  
              displayCenteredMessageFromStoredString((uint8_t*)&LOGIN_PASSWORD_SENT);
              delay(MSG_DISPLAY_DELAY);    
              break;              
            default:
              break;
          }
        } 
      }
      break;
  
    case MAIN_MENU_MANAGEPWD:

      // Launch sub-menu
      entry_choice1 = menu_manage_passwords();
      switch (entry_choice1) {

        case MANAGEPWD_MENU_SETPWD:
          // Launch sub-menu
          entry_choice2 = menu_set_pwd();
          switch (entry_choice2) {
            case SETPWD_MENU_GENERATE:
              generatePassword();
              break;
            case SETPWD_MENU_MANUALLY:
              inputPassword();         
              break;
            default:
              break;
          }
          break;
        
        case MANAGEPWD_MENU_DELPWD:
          entry_choice2 = pickEntry();
          DEBUG( Serial.print("clear entry "); Serial.println(entry_choice2);)
          if (entry_choice2 == RET_EMPTY) {
            displayCenteredMessageFromStoredString((uint8_t*)&DEVICE_EMPTY);
            delay(MSG_DISPLAY_DELAY);
          }          
          if (entry_choice2 != RET_CANCEL) {
            if (confirmChoice((char*)"del entry?")) {
              displayCenteredMessageFromStoredString((uint8_t*)&DELETING_ENTRY); 
              ES.removeEntry(entry_choice2);
            }
          } 
          break;
                  
        case MANAGEPWD_MENU_FORMAT:
          if (confirmChoice((char*)"Sure?")) format();
          break;

        case MANAGEPWD_MENU_CHECKNBENTRIES:
          printNbEntries();
          break;      
        /*          
        case MANAGEPWD_MENU_TEST1:
          testFunction1();
          break;      
        
        case MANAGEPWD_MENU_TEST2:
          testFunction2();
          break;
        */
        default:
          break;      
      }
      break;

    case MAIN_MENU_SETUP:
      // Launch sub-menu
      entry_choice1 = menu_setup();
      switch (entry_choice1) {

        case SETUP_MENU_BTCONF:
          if (confirmChoice((char*)"Sure?")) {
            configureRN42();
          }
          break;
        
        case SETUP_MENU_BTCONNECT:
          connectRN42();
          break;
                  
        default:
          break;      
      }
      break;
      
   default:
      break;   
  }

  DEBUG( Serial.print("Watermark after main loop action: "); Serial.println(StackMarginWaterMark()); )
}

