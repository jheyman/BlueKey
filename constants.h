#ifndef __constants_H__
#define __constants_H__

#define OLED_RESET 4
#define DISPLAY_I2C_ADDR 0x3C

#define EEPROM_I2C_ADDR 0x50
#define EEPROM_TEST_OFFSET 1280

#ifdef BLUEKEY_KNOB
#define knobInterruptPin 3
#define knobDataPin 12
#define knobSwitchPin 2
#endif

#ifdef BLUEKEY_SNES

#define UpButtonPin 4
#define UpButtonIndex 0

#define DownButtonPin A1
#define DownButtonIndex 1

#define LeftButtonPin 3
#define LeftButtonIndex 2

#define RightButtonPin 5
#define RightButtonIndex 3


#define XButtonPin 9
#define XButtonIndex 4

#define YButtonPin 12
#define YButtonIndex 5

#define AButtonPin 10
#define AButtonIndex 6

#define BButtonPin 11
#define BButtonIndex 7


#define StartButtonPin 13
#define StartButtonIndex 8

#define SelectButtonPin A0
#define SelectButtonIndex 9


#define SideLeftButtonPin 6
#define SideLeftButtonIndex 10

#define SideRightButtonPin 7
#define SideRightButtonIndex 11

#endif


#ifdef BLUEKEY_NES

#define UpButtonPin 2
#define UpButtonIndex 0

#define DownButtonPin 3
#define DownButtonIndex 1

#define LeftButtonPin 4
#define LeftButtonIndex 2

#define RightButtonPin 5
#define RightButtonIndex 3

#define AButtonPin 6
#define AButtonIndex 4

#define BButtonPin 7
#define BButtonIndex 5

#define StartButtonPin 9
#define StartButtonIndex 6

#define SelectButtonPin 8
#define SelectButtonIndex 7

#endif

#define BT_connected_pin 8
#define ENTROPY_PIN A3


#define DEBOUNCE_TIME 10 //ms
#define BUTTON_HOLD_TIME 300 //ms

#define debounceDelay 50

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32

#define CHAR_XSIZE 6
#define CHAR_YSIZE 8
#define SCREEN_MAX_NB_CHARS_PER_LINE 21
#define SCREEN_MAX_NB_LINES 4
#define CURSOR_Y_FIRST_LINE 0
#define CURSOR_Y_SECOND_LINE CHAR_YSIZE
#define CURSOR_Y_THIRD_LINE CHAR_YSIZE*2
#define CURSOR_Y_FOURTH_LINE CHAR_YSIZE*3

#define CODE_SLOT_CHAR '_'
#define CODE_HIDE_CHAR '*'
#define PIVOT_CHAR_INDEX 10

#define USERCODE_LENGTH 6 
#define USERCODE_BUFF_LEN 32 // must be = EEPROM_PASS_CIPHER_LENGTH since ES.format modifies buffer content
#define DEVICENAME_LENGTH 12 
#define DEVNAME_BUFF_LEN 32 // must be = EEPROM_DEVICENAME_LENGTH since ES.format modifies buffer content

#define PASSWORD_MAX_LENGTH 16

#define ACCOUNT_TITLE_LENGTH 12
#define ACCOUNT_LOGIN_LENGTH 12

// Bluetooth address format is 12 letters/numbers
#define USERDATA_BT_ADDRESS_LEN 12

#define MSG_DISPLAY_DELAY 1000

#define RET_EMPTY -1
#define RET_CANCEL -2 

typedef struct {
  char* buffers[5]; 
  char buffer_size[5];
  byte nbBuffers;
} MultilineInputBuffer;

enum MainMenuSelection {
  MAIN_MENU_SENDPWD = 0,
  MAIN_MENU_MANAGEPWD,
  MAIN_MENU_SETUP
};

enum SendPasswordMenuSelection {
  SENDPWD_MENU_LOGINONLY = 0,
  SENDPWD_MENU_PWDNONLY,
  SENDPWD_MENU_LOGIN_TAB_PWD
};

enum ManagePasswordsMenuSelection {
  MANAGEPWD_MENU_SETPWD = 0,
  MANAGEPWD_MENU_DELPWD,
  MANAGEPWD_MENU_FORMAT,
  MANAGEPWD_MENU_CHECKNBENTRIES,
  MANAGEPWD_MENU_TEST1,
  MANAGEPWD_MENU_TEST2,
};

enum SetPasswordMenuSelection {
  SETPWD_MENU_GENERATE = 0,
  SETPWD_MENU_MANUALLY,
};

enum SetupMenuSelection {
  SETUP_MENU_BTCONF = 0,
  SETUP_MENU_BTCONNECT,
};

#endif

