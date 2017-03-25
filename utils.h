/*
  The Final Key is an encrypted hardware password manager, 
  this is the sourcecode for the firmware. 

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __utils_H__
#define __utils_H__

#include <Wire.h>
#include <TermTool.h>

void displayMessage(char* msg, int X, int Y); 
void displayCenteredMessage(char* msg);  
void displayCenteredMessageFromStoredString(uint8_t* storedString);
void testEEPROM(int index);
void messUpEEPROMFormat();
byte getStringFromFlash(char* buffer, uint8_t* storedString);
void printHexBuff(byte* buff, char* name, int len);
void paintStack();
uint16_t StackMarginWaterMark(void);
void printSRAMMap();
void I2Cscan();
int mod(int x, int m);

#endif
