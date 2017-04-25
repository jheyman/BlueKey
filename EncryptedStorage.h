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

#ifndef EncryptedStorage_H
#define EncryptedStorage_H
#include "eeprom.h"
#include "AES.h"

//Each entry is 16 bytes for iv, followed by encrypted struct of  bytes 
//Struct must be %16 == 0

#define ENTRY_TITLE_SIZE 32

typedef struct {
  char title[ENTRY_TITLE_SIZE]; // Title needs to be two blocks for seperate decryption
  uint8_t passwordOffset;	//Where the password starts in the string of data 
  //char data[190];
  //char data[79];
  char data[47];
} entry_t;

#define NUM_ENTRIES 64

class EncryptedStorage
{
public:
  void initialize();
  bool readHeader(char* deviceName);
  
  bool unlock( byte* k );
  void lock();

  bool getTitle( uint8_t entryNum, char* title);
  bool getEntry( uint8_t entryNum, entry_t* entry ); 
  
  void putEntry( uint8_t entryNum, entry_t* entry );
  void delEntry ( uint8_t entryNum);

  int8_t insertEntry(entry_t* entry);
  void removeEntry (uint8_t entryNum); 
  
  void format( byte* pass, char* name );
  uint8_t getNbEntries();

private:
  void putPass( byte* pass );
  void putIv( byte* dst );
  AES aes;
  uint8_t nbEntries;
  uint8_t crc8(const uint8_t *addr, uint8_t len);  
};

extern EncryptedStorage ES;

#endif
