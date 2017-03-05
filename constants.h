#ifndef __constants_H__
#define __constants_H__

#define OLED_RESET 4
#define DISPLAY_I2C_ADDR 0x3C

#define EEPROM_I2C_ADDR 0x50
#define EEPROM_TEST_OFFSET 1280

#define knobInterruptPin 3
#define knobDataPin 12
#define knobSwitchPin 2

#define debounceDelay 50

#define ledPin 13

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32

#define CHAR_XSIZE 6
#define CHAR_YSIZE 8
#define SCREEN_MAX_NB_CHARS_PER_LINE 21
#define CURSOR_Y_FIRST_LINE 0
#define CURSOR_Y_SECOND_LINE CHAR_YSIZE
#define CURSOR_Y_THIRD_LINE CHAR_YSIZE*2
#define CURSOR_Y_FOURTH_LINE CHAR_YSIZE*3

#define CODE_SLOT_CHAR '_'
#define CODE_HIDE_CHAR '*'
#define PIVOT_CHAR_INDEX 10

#define USERCODE_LENGTH 6
#define USERNAME_LENGTH 9
#endif

