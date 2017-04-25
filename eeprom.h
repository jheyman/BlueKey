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

#ifndef __eeprom_H__
#define __eeprom_H__
#include <Arduino.h>

class EEPROM
{
public:
  void power(uint8_t state);
  
  //Requires page-aligned eeaddress, any len valid, returns addresss after last byte read/written.
  uint16_t dataOp(uint16_t eeaddress, byte* data, uint8_t len, uint8_t write);

};

extern EEPROM eeprom;

#define I2E_Write(addr, data, len) eeprom.dataOp(addr, data, len, 1)
#define I2E_Read(addr, data, len) eeprom.dataOp(addr, data, len, 0)

#endif
