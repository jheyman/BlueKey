#include <TermTool.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "eeprom.h"
#include "EncryptedStorage.h"
#include "Entropy.h"

#define ledPin 13
const char **textArrayPointer = NULL;
volatile int textArrayIndex=0;
volatile int textArraySize=0;

#define MAIN_MENU_TEXTARRAY_SIZE 4
const char * MAIN_MENU_TEXTARRAY[] = {"un", "deux", "trois", "quatre"};

#define CODEINPUT_MENU_CHARS_SIZE 10
const char CODEINPUT_MENU_CHARS[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

#define TEXTINPUT_MENU_CHARS_SIZE 75
const char TEXTINPUT_MENU_CHARS[] = {'~','@','!','?', '"','#','$','%','&','*','+','-','.','0','1','2','3','4','5','6','7','8','9',
'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};

/////////////////////////
// OLD DISPLAY management
/////////////////////////

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);
#define DISPLAY_I2C_ADDR 0x3C

////////////////////
// EEPROM Read/Write
////////////////////

#define EEPROM_I2C_ADDR  0x50

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
byte rowPins[ROWS] = { 11, 10, 9, 8 };
byte colPins[COLS] = { 7, 6, 5, 4 }; 

//const byte keypadInterruptPin = 2;


/*
bool newCharReceived=false;
int rowPressed =-1;
int colPressed = -1;

void uponKeyPadIT() {

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
*/
////////////////////
// KNOB management
////////////////////

const byte knobInterruptPin = 3;
const byte knobDataPin = 12;
const byte knobSwitchPin = 2;

volatile int knobIncrements=0;
//bool knobIncrementChanged=false;

volatile bool knobIndexIncreased = false;
volatile bool knobIndexDecreased = false;

void uponKnobIT() {

  static unsigned long  lastInterruptTime = 0;
  unsigned long interruptTime = millis();

  // basic anti-glitch filtering
  if (interruptTime - lastInterruptTime > 10) {
    if (digitalRead(knobDataPin)) {
      knobIncrements++;
      //textArrayIndex=textArrayIndex+1;
      //if (textArrayIndex > (textArraySize-1)) textArrayIndex = 0;
      knobIndexIncreased = true;
    }  
    else {
      knobIncrements--; 
      //textArrayIndex=(textArrayIndex-1);
      //if (textArrayIndex < 0) textArrayIndex = textArraySize - 1;
      knobIndexDecreased = true;
    }
    
    //knobIncrementChanged = true;
    lastInterruptTime = interruptTime;
  }
}


bool knobSwitchPushed=false;

/*
void uponKnobSwitchIT() {

  static unsigned long  lastInterruptTime = 0;
  unsigned long interruptTime = millis();

  // basic anti-glitch filtering
  if (interruptTime - lastInterruptTime > 10) {
    knobSwitchPushed = true;
    lastInterruptTime = interruptTime;
  }
}
*/


void setRng()
{
  analogWrite(ledPin, 250);
  randomSeed( Entropy.random() );
  digitalWrite(ledPin,1);
}

////////////////////
// INITIALISATION
////////////////////
void setup()   {  

  Entropy.initialize();
  setRng();

  textArrayPointer = MAIN_MENU_TEXTARRAY;
  textArrayIndex=0;
  textArraySize=MAIN_MENU_TEXTARRAY_SIZE;
 
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
  display.setTextColor(WHITE, BLACK);
  display.setCursor(0,0);
  display.println("Hello, world!");
  display.display();


  // TO BE INVESTIGATED: for some reason the initialisation of column lines left line 4 at high state. Force is to ground now (again).
  pinMode(4,OUTPUT);
  digitalWrite(4, LOW);  



  // Setup the interrupt used for keypad management
  //pinMode(keypadInterruptPin, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(keypadInterruptPin), uponKeyPadIT, CHANGE);  

  // Setup the interrupt used for knob management
  pinMode(knobInterruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(knobInterruptPin), uponKnobIT, FALLING);   

    // Setup the interrupt used for knob management
  pinMode(knobSwitchPin, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(knobSwitchPin), uponKnobSwitchIT, RISING);   

  //knobIncrementChanged=false;
  knobIndexIncreased=false;
  knobIndexDecreased=false;
  knobSwitchPushed=false;


}

int lastButtonState = LOW;
int buttonState; 
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

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

bool getStr( char* dst, const uint8_t numChars, const char charArray[], int charArraySize)
{
    int charIndex = 0;
    int cursorX = display.getCursorX();
    int nbCharsValidated = 0;
    char c = '?';
      
    display.setCursor(cursorX,0);
    display.print('?');
    display.display();
        
    // Require numChars input characters from user
    while (1) {






      // If knob switch was pushed, validate current char
      //if(knobSwitchPushed) {
      if (checkbuttonPushed()) {
        //knobSwitchPushed = false;
        if (c != '?')
        {
          dst[nbCharsValidated] = c;
          nbCharsValidated++;
          if (nbCharsValidated == numChars) break;
          cursorX+=6;
          charIndex = 0; 
          c = '?';      
          display.setCursor(cursorX,0);
          display.print(c);
          display.display();
        }
      } 
      else if (knobIndexIncreased) {
        knobIndexIncreased = false;
        charIndex = charIndex + 1;
        if (charIndex > (charArraySize-1)) charIndex = 0; 
        c = charArray[charIndex];
        display.setCursor(cursorX,0);
        display.print(c);        
        display.display();         
      }
      else if (knobIndexDecreased) {
        knobIndexDecreased = false;
        charIndex = charIndex - 1;
        if (charIndex < 0) charIndex = charArraySize - 1;  
        c = charArray[charIndex];     
        display.setCursor(cursorX,0);
        display.print(c);
        display.display();        
      }

      delay(10);
      
    }

    // terminate the C-string
    dst[nbCharsValidated]=0;
    
    return true;
}

#define SCREEN_MAX_NB_CHARS_PER_LINE 21
#define PIVOT_CHAR_INDEX 10

#define FIRST_ASCII_CHAR 33
#define LAST_ASCII_CHAR 126
#define NB_CHARS (LAST_ASCII_CHAR - FIRST_ASCII_CHAR + 1)

// Modulus function that handles negative numbers
int mod(int x, int m) {
    return (x%m + m)%m;
}

bool getStr2( char* dst, const uint8_t numChars)
{
    int cursorX = display.getCursorX();
    int nbCharsValidated = 0;
    char pivot_char = 0;

    display.setCursor((PIVOT_CHAR_INDEX)*6,8);
    display.print((char)31);
    display.setCursor((PIVOT_CHAR_INDEX)*6,24);
    display.print((char)30); 
    display.display();
    
    display.setCursor(0,16);

    for (int i=0; i<SCREEN_MAX_NB_CHARS_PER_LINE; i++) {
      char c = FIRST_ASCII_CHAR + mod(pivot_char + i - PIVOT_CHAR_INDEX, NB_CHARS);
      display.print(c);
    }
    
    display.display();
        
    // Require numChars input characters from user
    while (1) {

      // If knob switch was pushed, validate current char
      //if(knobSwitchPushed) {
      //  knobSwitchPushed = false;
      if (checkbuttonPushed()) {
        display.setCursor(0,24);
        display.print("     ");
        display.setCursor(0,24);
        display.print(knobIncrements);

        display.setCursor(cursorX,0);
        display.print((char)(pivot_char+FIRST_ASCII_CHAR));  
        //display.print('('); 
        //display.print(pivot_char+FIRST_ASCII_CHAR); 
        //display.print(')');  
        display.display();        
        cursorX += 6;
          
        dst[nbCharsValidated] = (char)(pivot_char+FIRST_ASCII_CHAR);
        nbCharsValidated++;
        if (nbCharsValidated == numChars) break;
      } 
      else if (knobIndexIncreased) {
        knobIndexIncreased = false;

        pivot_char = mod(pivot_char+1,NB_CHARS);
        
        display.setCursor(0,16);
    
        for (int i=0; i<SCREEN_MAX_NB_CHARS_PER_LINE; i++) {
          char c = FIRST_ASCII_CHAR + mod(pivot_char + i - PIVOT_CHAR_INDEX, NB_CHARS);
          display.print(c);       
        }    

        display.setCursor(0,24);
        display.print("     ");
        display.setCursor(0,24);
        display.print(knobIncrements);
        
        display.display();
      }
      else if (knobIndexDecreased) {
        knobIndexDecreased = false;

        pivot_char = mod(pivot_char-1,NB_CHARS);
        
        display.setCursor(0,16);
    
        for (int i=0; i<SCREEN_MAX_NB_CHARS_PER_LINE; i++) {
          char c = FIRST_ASCII_CHAR + mod(pivot_char + i - PIVOT_CHAR_INDEX, NB_CHARS);
          display.print(c);        
        }  

        display.setCursor(0,24);
        display.print("     ");
        display.setCursor(0,24);
        display.print(knobIncrements);   
        
        display.display();    
      }

      delay(1);
    }

    // terminate the C-string
    dst[nbCharsValidated]=0;
    
    return true;
}


void format()
{
  //Two imput buffers, for easy password comparison.
  char bufa[33];
  char bufb[33];
  ptxtln("Choose password (1-32)");
  bool fail=1;

  while(fail)
  {
    ptxt("Psw:");
    if( getStr(bufa, 32, CODEINPUT_MENU_CHARS, CODEINPUT_MENU_CHARS_SIZE ) )
    {
      ptxt("\r\nRepeat:");
      if( getStr(bufb, 32, CODEINPUT_MENU_CHARS, CODEINPUT_MENU_CHARS_SIZE ) )
      {
        if(memcmp(bufa,bufb,32)==0)
        {
          fail=0;
        }
      }
    }
    if(fail)
      ptxtln("\r\n[ERROR]");
  }
  fail=1;
  while(fail)
  {
    ptxtln("\r\nName, (0-31):");
    if( getStr(bufb, 31, CODEINPUT_MENU_CHARS, CODEINPUT_MENU_CHARS_SIZE ) )
    {
      fail=0;
    }
  }
  Serial.println();
  ES.format( (byte*)bufa, bufb ); 
}


bool login()
{
  char tmp[32];
  getStr(tmp, 5, CODEINPUT_MENU_CHARS, CODEINPUT_MENU_CHARS_SIZE);

  display.clearDisplay();
  display.setCursor(0,0);
  display.print("LOGIN=[");
  display.print(tmp);
  display.print("]");
  display.display();

  delay(3000);
  if (!strcmp(tmp, "12345"))
    return true;
  else
    return false;
 /*

  char key[33];
  char devName[32];
  memset(key,0,33);
  memset(devName,0,32);
  uint8_t ret=0;

  if( !ES.readHeader(devName) )
  {
    ptxtln("Need format before use");
    format();
  } else {

    Serial.write('{');
    Serial.print(devName);
    ptxt("}\r\nPass:");

    getStr(key,32, CODEINPUT_MENU_CHARS, CODEINPUT_MENU_CHARS_SIZE);
   
    if( ES.unlock( (byte*)key ) )
    {
       ret=1;
       ptxtln("\r\n[Granted]");
     } else {
         ptxtln("\r\n[Denied]");
     }
  }
  */
}



////////////////////
// MAIN LOOP
////////////////////
int loopidx=0;

void loop() {

  loopidx++;
  
  //if (loopidx % 100 == 0) {
   // Serial.print(".");
  //}

/*
  if (knobSwitchPushed) {
     knobSwitchPushed = false;
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("PUSHED!");
    display.display();
  }
*/
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("CODE:");
  display.display();

  //if (login()) {
  while(1) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("GRANTED, text=");
    display.display();

    char tmp[6];
    getStr2(tmp, 5);

    display.clearDisplay();
    display.setCursor(0,0);
    display.print("getStr2=");
    display.print(tmp);
    display.display();

    delay(1000);
  }; 

  /*

  if (knobIncrementChanged) {

    knobIncrementChanged=false;

    display.clearDisplay();
    display.setCursor(0,0);
    
    //char keyChar = keys[rowPressed][colPressed];

    //display.print("Key=");
    //display.print(keyChar);
    display.print("inc=");
    display.print(knobIncrements);

    const char* text = textArrayPointer[textArrayIndex];

    display.print("text=");
    display.print(text);
    
    display.display();

    //Serial.print(keyChar);
    
    //if (newCharReceived) newCharReceived=false;
    
  }
*/
  //testEEPROM();
  delay(10);
}
