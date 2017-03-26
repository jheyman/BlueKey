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

#define ENTRY_SIZE sizeof(entry_t)

//#define EEPROM_ENTRY_DISTANCE 240 //EntrySize + 16 for iv
#define EEPROM_ENTRY_DISTANCE 128 //EntrySize + 16 for iv

//#define ENTRY_FULL_CBC_BLOCKS 14 //Blocksize / 16 for encryption
#define ENTRY_FULL_CBC_BLOCKS 7//Blocksize / 16 for encryption

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

#define EEPROM_KEYBOARD_LAYOUT_LOCATION	(EEPROM_PASS_BACKGROUND_LOCATION + EEPROM_PASS_BACKGROUND_LENGTH)
#define EEPROM_KEYBOARD_LAYOUT_LENGTH 1

//Read the IV which comes after 12 + 32 bytes and is 16 bytes long. (Identifier + Unit name)
#define headerIdentifierOffsetAndIv(iv) I2E_Read( EEPROM_IV_LOCATION, iv, EEPROM_IV_LENGTH )
#define entryOffset( entryNum ) ((EEPROM_ENTRY_START_ADDR)+(EEPROM_ENTRY_DISTANCE*entryNum))

bool __attribute__ ((noinline)) EncryptedStorage::readHeader(char* deviceName)
{
  byte buf[HEADER_EEPROM_IDENTIFIER_LEN];
  bool ret = TRUE;
  
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

void __attribute__ ((noinline)) EncryptedStorage::setBanner(char* banner)
{
  I2E_Write( EEPROM_DEVICENAME_LOCATION,(byte*)banner, EEPROM_DEVICENAME_LENGTH );
}

bool __attribute__ ((noinline)) EncryptedStorage::unlock( byte* k )
{
  byte key[EEPROM_PASS_CIPHER_LENGTH];
  byte bck[EEPROM_PASS_BACKGROUND_LENGTH];
  byte iv[EEPROM_IV_LENGTH];
  bool success = FALSE;

  //printHexBuff(key, "k", 32);

  uint16_t offset = headerIdentifierOffsetAndIv(iv);

  //Read the encrypted password which comes after the IV
  offset = I2E_Read( offset, key, EEPROM_PASS_CIPHER_LENGTH );

  //printHexBuff(key, "enc_key", 32);
  
  //Read the background noise for the password
  I2E_Read( offset, bck, EEPROM_PASS_BACKGROUND_LENGTH );

  //printHexBuff(key, "bck", 32);
  
  //xor it with zero padded key
  for(uint8_t i = 0 ; i < EEPROM_PASS_CIPHER_LENGTH; i++ )
  {
    k[i] ^= bck[i];
  }

  //printHexBuff(k, "xor_k", 32);
  
  //Set key
  aes.set_key(k, 256);  

  //Decrypt
  if( aes.cbc_decrypt (key, key, 2, iv) == SUCCESS )
  {  
    //printHexBuff(key, "dec_key", 32);
      
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
  
  //Read first 32 bytes of entry.
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

void __attribute__ ((noinline)) EncryptedStorage::putEntry( uint8_t entryNum, entry_t* entry )
{
  uint16_t offset = entryOffset(entryNum);
  //Create IV
  byte iv[EEPROM_IV_LENGTH];

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
  //entry_t dat2;
  
  memset(&dat,0,EEPROM_IV_LENGTH); //Zero out first 16 bytes of entry so we can write an all zero iv.  
  //Write an all zero iv to indicate it's empty
 // Serial.print("\r\nEntry num: "); Serial.print(entryNum);Serial.print(" iv offset before ");Serial.print(offset);
  offset = I2E_Write( offset, (byte*)&dat, EEPROM_IV_LENGTH );
  
  //Fill entry with random numbers
  for(uint8_t i=0; i < ENTRY_SIZE; i++)
  {
    ((byte*)(&dat))[i] = random(255);
  }
  
  //Write random data
  I2E_Write( offset, (byte*)&dat, ENTRY_SIZE );
  //Read it back
  //I2E_Read( offset, (byte*)&dat2, ENTRY_SIZE );
  
  //Compare, to see that we read what we wrote
  //if( memcmp( &dat, &dat2, ENTRY_SIZE ) !=  0 )
  //{
   //  strcpy( dat.title, "[Bad]" );
  //   dat.passwordOffset=0;
  //   putEntry( entryNum, &dat );
  //}  
}

void __attribute__ ((noinline)) EncryptedStorage::changePass( byte* newPass, byte* oldPass )
{
  byte obck[32];
  entry_t entry;

  //Note: oldPass was or'ed with background noise by unlock
//  debugPrint((char*)F("Changing password..."));
  //putPass will xor the background noise into our newPass, so it's ready to use for encryption.
  putPass(newPass);

  // For each non-empty entry
  for(uint16_t i = 0; i < NUM_ENTRIES; i++ )
  {
   //Serial.write('\r');txt(i);Serial.write('/');txt(255);
    if( getTitle( (uint8_t)i, (char*)obck ) )
    {
      //Set old key
      aes.set_key( oldPass, 256 );
      
      //Read entry
      getEntry( (uint8_t)i, &entry );

      //Set new key
      aes.set_key( newPass, 256 );

      //Write entry
      putEntry( (uint8_t)i, &entry );
    }
  }

  //Clean up a bit
  memset( newPass, 0, 32 );
  memset( oldPass, 0, 32 );
  memset( &entry, 0, sizeof(entry) ); 
}

void __attribute__ ((noinline)) EncryptedStorage::format( byte* pass, char* name )
{
  byte identifier[HEADER_EEPROM_IDENTIFIER_LEN];

  for(uint16_t i=0; i < NUM_ENTRIES; i++ )
  {
    //Serial.write('\n');txt(i);Serial.write('/');txt(NUM_ENTRIES);
    char tmp[24];
    delEntry((uint8_t)i);
    sprintf(tmp, "Formatting: %d/%d", i+1, NUM_ENTRIES);
    displayCenteredMessage(tmp);    
  }

  //printHexBuff(pass, "UserPass", 32);
  
  putPass(pass);
    
  //Copy Identifier to memory
  for(uint8_t i=0; i < HEADER_EEPROM_IDENTIFIER_LEN; i++)
    identifier[i]=pgm_read_byte (& eepromIdentifierTxt[i]);
  
  //Write the cleartext stuff. Identifier and Name.
  I2E_Write( EEPROM_IDENTIFIER_LOCATION, identifier, HEADER_EEPROM_IDENTIFIER_LEN );
  I2E_Write( EEPROM_DEVICENAME_LOCATION,(byte*)name, EEPROM_DEVICENAME_LENGTH );

  Serial.print("device name written:"); Serial.println(name);
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

  //printHexBuff(pass, "PASS", 32);
 
  //Generate IV
  putIv( iv );
  
  //Write the IV before it's changed by the encryption.
  I2E_Write( EEPROM_IV_LOCATION, iv, EEPROM_IV_LENGTH );

  //printHexBuff(iv, "iv", 16);

  //Encrypt the password.
  aes.set_key(pass, 256);  
  aes.cbc_encrypt(pass, key, 2, iv);

  //Write encrypted key
  I2E_Write(EEPROM_PASS_CIPHER_LOCATION, key, EEPROM_PASS_CIPHER_LENGTH);

  //printHexBuff(key, "EEkey", 32);
  
  //Write background noise
  I2E_Write(EEPROM_PASS_BACKGROUND_LOCATION, bck, EEPROM_PASS_BACKGROUND_LENGTH); 

  //printHexBuff(bck, "EEbck", 32);
}

void __attribute__ ((noinline)) EncryptedStorage::putIv( byte* dst )
{
  do {
    for(uint8_t i = 0; i < EEPROM_IV_LENGTH; i++)
    {
      analogWrite(10, 250);
      dst[i]=Entropy.random(0xff);  
      digitalWrite(10,1);
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

/*
//R = Ready, W = Wait, C =CRC error, O = OK, F = Failure, E=End
void __attribute__ ((noinline)) EncryptedStorage::importData()
{
  char buf[32];
  uint16_t i=0;
  uint8_t crca, crcb;
  Serial.setTimeout(30000);
  while( i < 64000 )
  {
    txt("R");
    if( Serial.readBytes(buf,32) == 32 )
    {
      crcb = Serial.read();
      txt("W");
      crca = crc8((uint8_t*)buf,32);
      if( crca != crcb )
      {
	I2E_Write( 0, (byte*)"C", 1 );
	txt("C");
	while(1) {};
      } else {
	txt("O");
      }
      I2E_Write(i, (byte*)buf, 32);
      i+=32;
    } else {
      I2E_Write( 0, (byte*)"F", 1 );
      txt("F");
      break;
    }
  }
  txt("E");
}

void __attribute__ ((noinline)) EncryptedStorage::exportData()
{
  byte buf[32];
  uint8_t crc;
 // debugPrint((char*)"exportData...");
  for(uint16_t i=0; i < 64000; i+=32)
  {
    I2E_Read( i, buf, 32 );
    for( int ii=0; ii < 32; ii++)
    {
      Serial.write((char)buf[ii]);
    }
    crc = crc8( (uint8_t*)buf, 32 );
    Serial.write( (char)crc );
  }
//  debugPrint((char*)"exportData DONE");
}
*/
uint16_t __attribute__ ((noinline)) EncryptedStorage::getNextEmpty()
{
  uint8_t iv[EEPROM_IV_LENGTH];
  uint16_t idx;
  uint8_t taken;

  for(idx=0; idx < NUM_ENTRIES; idx++)
  {
    //Read iv
    getIVandStartAddressForEntry(idx, iv);
    
    taken=0;
    for(uint8_t i = 0; i < EEPROM_IV_LENGTH; i++)
    {
      taken |= iv[i];
    }
    
    if( !taken )
    {
      break;
    }
  }
  
  return( idx );
}

void __attribute__ ((noinline)) EncryptedStorage::setKeyboardLayout(uint8_t lang)
{
  byte buf = (byte)lang;
  I2E_Write(EEPROM_KEYBOARD_LAYOUT_LOCATION, &buf, EEPROM_KEYBOARD_LAYOUT_LENGTH);  
}

uint8_t __attribute__ ((noinline)) EncryptedStorage::getKeyboardLayout()
{
  byte buf;
  I2E_Read(EEPROM_KEYBOARD_LAYOUT_LOCATION, &buf, EEPROM_KEYBOARD_LAYOUT_LENGTH); 
  return( (uint8_t)buf);
}

EncryptedStorage ES = EncryptedStorage();
