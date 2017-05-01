#include "utils.h"
#include "eeprom.h"
#include "display.h"
#include "constants.h"

#define STACK_CANARY_VAL 0xfd

extern int __heap_end;
extern int __heap_start;
extern int __stack;
extern int __bss_start;  

extern int __data_end;
extern int __data_start; 
extern int * __brkval; 
extern int __bss_end; 

void __attribute__ ((noinline)) displayMessage(char* msg, int X, int Y) {  

    display.clearDisplay();    
    display.setCursor(X,Y);
    display.print(msg);
    display.display();
}

void __attribute__ ((noinline)) displayCenteredMessage(char* msg) {  
    int cursorX;
    int cursorY;
  
    cursorX = (DISPLAY_WIDTH - (strlen(msg))*CHAR_XSIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CHAR_YSIZE)/2;

    displayMessage(msg, cursorX, cursorY);
}

void __attribute__ ((noinline)) displayCenteredMessageFromStoredString(uint8_t* storedString) {
    char msgBuffer[32];
    getStringFromFlash(msgBuffer, storedString);
    displayCenteredMessage(msgBuffer);
}

byte __attribute__ ((noinline)) getStringFromFlash(char* buffer, uint8_t* storedString) {

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

void testEEPROM(int index) {

  byte readbackBuffer[32];
  strcpy((char*)readbackBuffer, (char*)F("xxxxxxxx"));
  
  uint16_t offset = 0; 
  offset = I2E_Write( EEPROM_TEST_OFFSET,(byte*)"test4", 224 );
  
  I2E_Read(EEPROM_TEST_OFFSET, (byte*)readbackBuffer, 32);

   display.clearDisplay();
   display.setCursor(0,0);
   display.print("OFF:");
   display.print(offset);
   display.print(",RB:");
   display.print((char*)readbackBuffer);   
   display.display(); 
}

void messUpEEPROMFormat() {

  // overwrite the identifier
  byte writeBuffer[32];
  I2E_Write(0, writeBuffer, 12);
}
void printHexBuff(byte* buff, char* name, int len) {
    Serial.print("\n");
    Serial.print(name);
    for(uint8_t i = 0 ; i < len; i++ ) {
      Serial.write(' ');
      if (buff[i] < 0x10) {
        Serial.print('0');
        Serial.print(buff[i], HEX);
      } else {
        Serial.print(buff[i], HEX);
      }
    }
}

void paintStack() {
  uint8_t *p = (uint8_t *)&__bss_end;
  while(p <= SP) *p++ = STACK_CANARY_VAL;
}
 
uint16_t StackMarginWaterMark(void) {
    const uint8_t *p = (uint8_t *)&__bss_end;
    uint16_t c = 0;
     
    //while(*p ==STACK_CANARY_VAL && p <= (uint8_t *)&__stack) {
    while(*p ==STACK_CANARY_VAL && p <= SP) {
        p++;
        c++;
    }
    return c;
}

void printSRAMMap() {
    int dummy;
    int free_ram; 

    free_ram =  (int) &dummy - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
  
    Serial.print("\nMemory map:");

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
      
    Serial.print("\n, RAMEND=");
    Serial.println(RAMEND);

/*
  uint8_t *p = (uint8_t *)&__bss_end;
   
  while(p <= RAMEND) {
    Serial.print("@");
    Serial.print((int)p);
    Serial.print(": 0x");
    Serial.println(*p++, HEX);
  }

*/
    
}

void printCurrentStackMargin() 
{
  Serial.print("margin="); Serial.println(SP-(int)&__bss_end);
}

void I2Cscan() {
  byte error, address;
  int nDevices;
 
  Serial.println("Scanning...");
 
  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println("  !");
 
      nDevices++;
    }
    else if (error==4)
    {
      Serial.print("Unknow error at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
 
  delay(5000);           // wait 5 seconds for next scan
}

// Modulus function that handles negative numbers
int mod(int x, int m) {
    return (x%m + m)%m;
}
