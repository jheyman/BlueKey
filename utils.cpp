#include "utils.h"
#include "eeprom.h"
#include "display.h"
#include "constants.h"

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
