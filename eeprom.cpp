#include <Wire.h>
#include "eeprom.h"
#include "constants.h"

uint16_t EEPROM::dataOp(uint16_t eeaddress, byte* data, uint8_t len, uint8_t write)
{
  //printFreeRam("EEPROMdataOP",0);
  
  while(len)
  {
    uint8_t lenForPage;
    //lenForPage = (len > 31)?32:len;
    lenForPage = (len > 15)?16:len;
    
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
