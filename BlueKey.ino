#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#define EEPROM_I2C_ADDR  0x50
#define DISPLAY_I2C_ADDR 0x3C

////////////////////
// EEPROM Read/Write
////////////////////

#define I2E_Write(addr, data, len) dataOp(addr, data, len, 1)
#define I2E_Read(addr, data, len) dataOp(addr, data, len, 0)

uint16_t dataOp(uint16_t eeaddress, byte* data, uint8_t len, uint8_t write)
{
  while(len)
  {
    uint8_t lenForPage;
    lenForPage = (len > 31)?32:len;
    
    uint8_t currentPageOffset = eeaddress%128;
    uint8_t nextPageOffset = (eeaddress+lenForPage)%128;
    
    if( nextPageOffset < currentPageOffset )
    {
      lenForPage -= nextPageOffset;
    }
    
    //Start communication with eeprom, send address
    Wire.beginTransmission(EEPROM_I2C_ADDR); //Last one tells to WRITE  
    Wire.write((uint16_t)(eeaddress >> 8)); // MSB
    Wire.write((uint16_t)(eeaddress & 0xFF)); // LSB    
        
    if(write)
    {
      //Write the length for current page
      for(uint8_t i = 0; i < lenForPage; i++)
      {
        Wire.write(data[i]);
      }
      Wire.endTransmission();
      data+=lenForPage;
    } else {
      //Stop and request data back from address.
      Wire.endTransmission();
      Wire.requestFrom((uint8_t)(EEPROM_I2C_ADDR),(uint8_t)lenForPage);
      while( Wire.available() )
      {
        *(data++)=Wire.read();
      }
    }

   // Successive write with no delays may produce errors, depending on the EEPROM used
   // 5ms works, 8ms is for margin.
   delay(8); 
    
    eeaddress+=lenForPage;
    len -= lenForPage;
  }
  
  return(eeaddress);
}

#define EEPROM_TEST_OFFSET 0
void testEEPROM() {

  byte writeBuffer[32];
  strcpy((char*)writeBuffer, "Write test in EEPROM");
  
  byte readbackBuffer[32];
  strcpy((char*)readbackBuffer, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
  
  uint16_t offset = 0; 
  offset=I2E_Write(EEPROM_TEST_OFFSET, writeBuffer, 32 );

  I2E_Read(EEPROM_TEST_OFFSET, readbackBuffer, 32);

   display.clearDisplay();
   display.setCursor(0,0);
   display.print("OFF:");
   display.print(offset);
   display.print(",RB:");
   display.print((char*)readbackBuffer);   
   display.display(); 
}

////////////////////
// KEYPAD management
////////////////////

const byte ROWS = 4; 
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = { 10, 9, 8, 7 };
byte colPins[COLS] = { 6, 5, 4, 3 }; 

const byte interruptPin = 2;

int loopidx=0;
bool newCharReceived=false;
int rowPressed =-1;
int colPressed = -1;

void uponIT() {

  // Clear all columns to HIGH level 
  for (byte c=0; c<COLS; c++) {
    digitalWrite(colPins[c], HIGH);  
  }
  
  // Scan columns
  for (byte c=0; c<COLS; c++) {
    digitalWrite(colPins[c], LOW);
  
    // Check with row is active
    for (byte r=0; r<ROWS; r++) {
       if(!digitalRead(rowPins[r])) {
          rowPressed = r;
          colPressed = c;
       }
    }
    
    digitalWrite(colPins[c],HIGH);
  }  

  // Now that pressed key is determined, reset all cols to DOWN to prepare for next button push
  for (byte c=0; c<COLS; c++) {
    digitalWrite(colPins[c], LOW);  
  }

  newCharReceived = true;
}

////////////////////
// INITIALISATION
////////////////////
void setup()   {                
  // RN42 is configured by default to use 115200 bauds on UART links
  Serial.begin(115200);
  //Serial1.begin(9600);
  
  Serial.print("Starting...");

  // Pull column lines to ground
  for (byte c=0; c<COLS; c++) {
    pinMode(colPins[c],OUTPUT);
    digitalWrite(colPins[c], LOW);  
  }

  // Configure rows as inputs, with pull-ups.
  for (byte r=0; r<ROWS; r++) {
    pinMode(rowPins[r],INPUT_PULLUP);
    //pinMode(rowPins[r],INPUT);
  }

  // Initialize OLED display
  display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDR);

  // Welcome banner
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Hello, world!");
  display.display();


  // TO BE INVESTIGATED: for some reason the initialisation of column lines left line 4 at high state. Force is to ground now (again).
  pinMode(4,OUTPUT);
  digitalWrite(4, LOW);  



  // Setup the interrupt used for keypad management
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), uponIT, CHANGE);  
}

////////////////////
// MAIN LOOP
////////////////////
void loop() {

  loopidx++;
  
  //if (loopidx % 100 == 0) {
   // Serial.print(".");
  //}

  if (newCharReceived) {

     display.clearDisplay();
     display.setCursor(0,0);
     
     char keyChar = keys[rowPressed][colPressed];
     display.print(keyChar);   
     display.display();

    Serial.print(keyChar);
    newCharReceived=false;
  }

  //testEEPROM();
  delay(1000);
}


///////////////////////////////
// SETUP & LOOP for I2C SCANNER
///////////////////////////////
/*

void setup()   {                
  Serial.begin(9600);
  while (!Serial);             // Leonardo: wait for serial monitor
  Serial.println("\nI2C Scanner");
}

void loop() {
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
*/

