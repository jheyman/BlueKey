/*
  The Final Key is an encrypted hardware password manager, 
  This lib handles encryption/decryption/eeprom layout. 

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

#include "EncryptedStorage.h"
#include "eeprom.h"
#include "Entropy.h"
#include "display.h" 
#include "utils.h"
#include <util/atomic.h>
#include <Wire.h>
#include <TermTool.h>

#define FALSE 0
#define TRUE 1

//Header:
//0-11  	- Identifier (12 bytes)
//12-43 	- Devicename (32 bytes)
//44-59		- Iv (16 bytes)
//60-91		- Encrypted password (32 bytes)
//92-123	- Background noise for password (32 bytes)
//124-124	- Keyboard Layout (1 byte)

//We reserve 1024 bytes for a rainy day
#define EEPROM_ENTRY_START_ADDR 1280

#define ENTRY_SIZE sizeof(entry_t) // 80
#define EEPROM_ENTRY_DISTANCE 96 // EntrySize + 16 for iv
#define ENTRY_FULL_CBC_BLOCKS 5 //Blocksize / 16 for encryption
#define ENTRY_NAME_CBC_BLOCKS 2 //Blocksize of decryption of title

#define EEPROM_IDENTIFIER_LOCATION 0
#define HEADER_EEPROM_IDENTIFIER_LEN 12
const static char eepromIdentifierTxt[HEADER_EEPROM_IDENTIFIER_LEN] PROGMEM  =  "[**BlueKey]";

#define EEPROM_DEVICENAME_LOCATION (EEPROM_IDENTIFIER_LOCATION + HEADER_EEPROM_IDENTIFIER_LEN)
#define EEPROM_DEVICENAME_LENGTH 32

#define EEPROM_IV_LOCATION (EEPROM_DEVICENAME_LOCATION + EEPROM_DEVICENAME_LENGTH)
#define EEPROM_IV_LENGTH 16

#define EEPROM_PASS_CIPHER_LOCATION (EEPROM_IV_LOCATION + EEPROM_IV_LENGTH)
#define EEPROM_PASS_CIPHER_LENGTH 32

#define EEPROM_PASS_BACKGROUND_LOCATION (EEPROM_PASS_CIPHER_LOCATION + EEPROM_PASS_CIPHER_LENGTH)
#define EEPROM_PASS_BACKGROUND_LENGTH 32 

#define EEPROM_NB_ENTRIES_LOCATION	(EEPROM_PASS_BACKGROUND_LOCATION + EEPROM_PASS_BACKGROUND_LENGTH)
#define EEPROM_NB_ENTRIES_LENGTH 1

//Read the IV which comes after 12 + 32 bytes and is 16 bytes long. (Identifier + Unit name)
#define headerIdentifierOffsetAndIv(iv) I2E_Read( EEPROM_IV_LOCATION, iv, EEPROM_IV_LENGTH )
#define entryOffset( entryNum ) ((EEPROM_ENTRY_START_ADDR)+(EEPROM_ENTRY_DISTANCE*(entryNum)))

void EncryptedStorage::initialize()
{
  I2E_Read(EEPROM_NB_ENTRIES_LOCATION, &nbEntries, EEPROM_NB_ENTRIES_LENGTH);
}

uint8_t EncryptedStorage::getNbEntries()
{
  return nbEntries;
}

bool __attribute__ ((noinline)) EncryptedStorage::readHeader(char* deviceName)
{
  byte buf[HEADER_EEPROM_IDENTIFIER_LEN];
  
  I2E_Read(0, buf, HEADER_EEPROM_IDENTIFIER_LEN);

  for(uint8_t i = 0; i < HEADER_EEPROM_IDENTIFIER_LEN; i++)
  {     
    if( buf[i] != pgm_read_byte(& eepromIdentifierTxt[i])  )
    {
      return(FALSE);
    }
  }
  
  I2E_Read(EEPROM_DEVICENAME_LOCATION, (byte*)deviceName, EEPROM_DEVICENAME_LENGTH);

  return(TRUE);
}

bool __attribute__ ((noinline)) EncryptedStorage::unlock( byte* k )
{
  byte key[EEPROM_PASS_CIPHER_LENGTH];
  byte bck[EEPROM_PASS_BACKGROUND_LENGTH];
  byte iv[EEPROM_IV_LENGTH];
  bool success = FALSE;

  uint16_t offset = headerIdentifierOffsetAndIv(iv);

  //Read the encrypted password which comes after the IV
  offset = I2E_Read( offset, key, EEPROM_PASS_CIPHER_LENGTH );

  //Read the background noise for the password
  I2E_Read( offset, bck, EEPROM_PASS_BACKGROUND_LENGTH );
  
  //xor it with zero padded key
  for(uint8_t i = 0 ; i < EEPROM_PASS_CIPHER_LENGTH; i++ )
  {
    k[i] ^= bck[i];
  }
  
  //Set key
  aes.set_key(k, 256);  

  //Decrypt
  if( aes.cbc_decrypt (key, key, 2, iv) == SUCCESS )
  {        
    success=TRUE;
    for(uint8_t i = 0 ; i < EEPROM_PASS_CIPHER_LENGTH; i++ )
    {
      if( key[i] != k[i] )
      {
	      success=FALSE;
      }
    }
  }
  return(success);
}

void __attribute__ ((noinline)) EncryptedStorage::lock()
{
  aes.clean();
}

static uint16_t __attribute__ ((noinline)) getIVandStartAddressForEntry( uint8_t entryNum, byte* iv )
{
  uint16_t offset = entryOffset(entryNum);
  offset = I2E_Read( offset, iv, EEPROM_IV_LENGTH );
  return(offset);
}

static bool __attribute__ ((noinline)) ivIsEmpty( byte* iv )
{
  uint8_t r = 0;
  for(uint8_t i=0; i<EEPROM_IV_LENGTH;i++)
  {
    r |= iv[i];
  }
  return( (r==0) );
}

bool __attribute__ ((noinline)) EncryptedStorage::getTitle( uint8_t entryNum, char* title)
{
  byte iv[EEPROM_IV_LENGTH];
  byte cipher[ENTRY_TITLE_SIZE];
  uint16_t offset = getIVandStartAddressForEntry( entryNum, iv);
   
  if( ivIsEmpty( iv ) )
  {
    return(FALSE);
  }
  
  //Read bytes of entry corresponding to title only.
  I2E_Read( offset, cipher, ENTRY_TITLE_SIZE );

  //Decrypt title
  aes.cbc_decrypt( cipher, (byte*)title, ENTRY_NAME_CBC_BLOCKS, iv );

  return(TRUE);
}

bool __attribute__ ((noinline)) EncryptedStorage::getEntry( uint8_t entryNum, entry_t* entry )
{
  byte iv[EEPROM_IV_LENGTH];
  uint16_t offset = getIVandStartAddressForEntry( entryNum, iv);
  if( ivIsEmpty( iv ) )
  {
    return(FALSE);
  }  

  //Read entry
  I2E_Read( offset, (byte*)entry, ENTRY_SIZE );
  
  //Decrypt entry
  aes.cbc_decrypt( (byte*)entry,(byte*)entry, ENTRY_FULL_CBC_BLOCKS, iv );
  return(TRUE);
}

int8_t __attribute__ ((noinline)) EncryptedStorage::insertEntry(entry_t* entry) 
{
  char tmp[ENTRY_TITLE_SIZE];
  uint8_t insertIndex=nbEntries; // by default assume we will insert the entry after the last valid entry 
  int entryIdx = 0;
  char tmp_entry[EEPROM_ENTRY_DISTANCE];

  if (nbEntries == NUM_ENTRIES)  return -1;
    
  // parse all active EEPROM entries and figure out at which location to insert it to preserve alphabetical ordering
  for (entryIdx = 0; entryIdx < nbEntries; entryIdx++)
  {
    if(ES.getTitle(entryIdx, tmp))
    {       
       if (strcmp(entry->title, tmp) < 0)
       {
         insertIndex = entryIdx;
         break;
       }          
    }
  } 

  if (nbEntries > 0)
  {
    // Move all entries from this index to the end up one slot
    for (entryIdx = nbEntries-1; entryIdx >= insertIndex; entryIdx--)
    {
        // Get element
        uint16_t offsetsrc = entryOffset(entryIdx);
        I2E_Read( offsetsrc, (byte*)tmp_entry, EEPROM_ENTRY_DISTANCE );
        uint16_t offsetdst = entryOffset(entryIdx+1);
        I2E_Write( offsetdst,(byte*)tmp_entry, EEPROM_ENTRY_DISTANCE );
    }
  }

  // Now store new entry at the freed index slot
  ES.putEntry(insertIndex, entry);

  //Increment current nb of entries and save to EEPROM
  nbEntries++;
  I2E_Write(EEPROM_NB_ENTRIES_LOCATION, &nbEntries, EEPROM_NB_ENTRIES_LENGTH);

  return insertIndex;
}

void __attribute__ ((noinline)) EncryptedStorage::removeEntry (uint8_t entryNum)
{
  char tmp_entry[EEPROM_ENTRY_DISTANCE];
    
  if (nbEntries == 0) return;

  // delete entry
  ES.delEntry(entryNum);

  //shift other entries
  for (uint8_t entryIdx = entryNum; entryIdx < nbEntries-1; entryIdx++)
  {
    //Serial.print("moving idx: "); Serial.println(entryIdx);
      uint16_t offsetsrc = entryOffset(entryIdx+1);
      I2E_Read( offsetsrc, (byte*)tmp_entry, EEPROM_ENTRY_DISTANCE );
      uint16_t offsetdst = entryOffset(entryIdx);
      I2E_Write( offsetdst,(byte*)tmp_entry, EEPROM_ENTRY_DISTANCE );
  }

  // clean-up last slot
  ES.delEntry(nbEntries-1);
    
  // update nb of entries and save new value to EEPROM
  nbEntries--;
  I2E_Write(EEPROM_NB_ENTRIES_LOCATION, &nbEntries, EEPROM_NB_ENTRIES_LENGTH);
}

void __attribute__ ((noinline)) EncryptedStorage::putEntry( uint8_t entryNum, entry_t* entry )
{
  uint16_t offset = entryOffset(entryNum);
  byte iv[EEPROM_IV_LENGTH];
  
  //Create IV
  putIv(iv);
  
  //Write IV
  offset=I2E_Write( offset , iv, EEPROM_IV_LENGTH );
 
  //Encrypt entry
  aes.cbc_encrypt((byte*)entry,(byte*)entry, ENTRY_FULL_CBC_BLOCKS, iv);

  //Write entry
  I2E_Write( offset,(byte*)entry,  ENTRY_SIZE );
}

void __attribute__ ((noinline)) EncryptedStorage::delEntry(uint8_t entryNum)
{
  uint16_t offset = entryOffset(entryNum);
  entry_t dat;

  memset(&dat,0,EEPROM_IV_LENGTH); //Zero out first 16 bytes of entry so we can write an all zero iv.  
  
  //Write an all zero iv to indicate it's empty
  offset = I2E_Write( offset, (byte*)&dat, EEPROM_IV_LENGTH );
  
  //Fill entry with random numbers
  for(uint8_t i=0; i < ENTRY_SIZE; i++)
  {
    ((byte*)(&dat))[i] = random(255);
  }
  
  //Write random data
  I2E_Write( offset, (byte*)&dat, ENTRY_SIZE );
}

void __attribute__ ((noinline)) EncryptedStorage::format( byte* pass, char* name )
{
  byte identifier[HEADER_EEPROM_IDENTIFIER_LEN];

  for(uint16_t i=0; i < NUM_ENTRIES; i++ )
  {
    char tmp[24];
    delEntry((uint8_t)i);
    sprintf(tmp, "Formatting: %d/%d", i+1, NUM_ENTRIES);
    displayCenteredMessage(tmp);    
  }
  
  putPass(pass);
    
  //Copy Identifier to memory
  for(uint8_t i=0; i < HEADER_EEPROM_IDENTIFIER_LEN; i++)
    identifier[i]=pgm_read_byte (& eepromIdentifierTxt[i]);
  
  //Write the cleartext stuff. Identifier and Name.
  I2E_Write( EEPROM_IDENTIFIER_LOCATION, identifier, HEADER_EEPROM_IDENTIFIER_LEN );
  I2E_Write( EEPROM_DEVICENAME_LOCATION,(byte*)name, EEPROM_DEVICENAME_LENGTH );

  //Serial.print("device name written:"); Serial.println(name);

  nbEntries = 0;
  I2E_Write(EEPROM_NB_ENTRIES_LOCATION, &nbEntries, EEPROM_NB_ENTRIES_LENGTH); 
}

//Find used and all 0  IV's so we can avoid them (0 avoided because we use it for detecting empty entry)
static bool __attribute__ ((noinline)) ivIsInvalid( byte* dst )
{
  bool invalid = FALSE;
  byte iv[EEPROM_IV_LENGTH];
  
  //check against all zero, all zero is unused entry
  if(ivIsEmpty(dst))
  {
    invalid=TRUE;
  }
  
  //The first one is the one for the header.
  headerIdentifierOffsetAndIv(iv);
  if(memcmp(iv, dst, EEPROM_IV_LENGTH) == 0)
  {    
    invalid=TRUE;
  } else {
    //Loop thorugh the entries
    for( uint16_t e = 0 ; e < NUM_ENTRIES; e++ )
    {
      getIVandStartAddressForEntry(e, iv);
      if( memcmp( iv, dst, EEPROM_IV_LENGTH ) == 0)
      {
	      invalid=TRUE;
	      break;
      }
    }
  }

  return(invalid);
}

void __attribute__ ((noinline)) EncryptedStorage::putPass( byte* pass )
{
  byte iv[EEPROM_IV_LENGTH];
  byte key[EEPROM_PASS_CIPHER_LENGTH];
  byte bck[EEPROM_PASS_BACKGROUND_LENGTH];
    
  //Generate background noise for password
  putIv( bck );
  putIv( (bck+16) );

  //xor it into existing password
  for(uint8_t i = 0 ; i < EEPROM_PASS_CIPHER_LENGTH; i++ )
  {
    pass[i] ^= bck[i];
  }
 
  //Generate IV
  putIv( iv );
  
  //Write the IV before it's changed by the encryption.
  I2E_Write( EEPROM_IV_LOCATION, iv, EEPROM_IV_LENGTH );

  //Encrypt the password.
  aes.set_key(pass, 256);  
  aes.cbc_encrypt(pass, key, 2, iv);

  //Write encrypted key
  I2E_Write(EEPROM_PASS_CIPHER_LOCATION, key, EEPROM_PASS_CIPHER_LENGTH);
 
  //Write background noise
  I2E_Write(EEPROM_PASS_BACKGROUND_LOCATION, bck, EEPROM_PASS_BACKGROUND_LENGTH); 
}

void __attribute__ ((noinline)) EncryptedStorage::putIv( byte* dst )
{
  do {
    for(uint8_t i = 0; i < EEPROM_IV_LENGTH; i++)
    {
      analogWrite(ENTROPY_PIN, 250);
      dst[i]=Entropy.random(0xff);  
      digitalWrite(ENTROPY_PIN,1);
    }

  } while( ivIsInvalid(dst) );
}

uint8_t EncryptedStorage::crc8(const uint8_t *addr, uint8_t len)
{
  uint8_t crc = 0;

  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; i--) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      inbyte >>= 1;
    }
  }
  return crc;
}

EncryptedStorage ES = EncryptedStorage();
