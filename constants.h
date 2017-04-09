#ifndef __constants_H__
#define __constants_H__

#define OLED_RESET 4
#define DISPLAY_I2C_ADDR 0x3C

#define EEPROM_I2C_ADDR 0x50
#define EEPROM_TEST_OFFSET 1280

#define knobInterruptPin 3
#define knobDataPin 12
#define knobSwitchPin 2

#define UpButtonPin 3
#define UpButtonIndex 0

#define DownButtonPin 5
#define DownButtonIndex 1

#define LeftButtonPin 2
#define LeftButtonIndex 2

#define RightButtonPin 4
#define RightButtonIndex 3

#define YButtonPin 6
#define YButtonIndex 4

#define DEBOUNCE_TIME 10 //ms
#define BUTTON_REPEAT_TIME_VERYSLOW 500 //ms
#define BUTTON_REPEAT_TIME_SLOW 100 //ms
#define BUTTON_REPEAT_TIME_FAST 0 //ms
#define BUTTON_HOLD_TIME 300 //ms

#define debounceDelay 50

#define ledPin 13

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

typedef struct {
  char* buffers[5]; 
  char buffer_size[5];
  byte nbBuffers;
} MultilineInputBuffer;

#endif

