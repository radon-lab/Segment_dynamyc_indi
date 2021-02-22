#include "wire.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DS1307_ADDRESS 0x68

void TimeGetDate(uint8_t *values);
void TimeSetDate(uint8_t year, uint8_t month, uint8_t dayOfMonth, uint8_t dayOfWeek, uint8_t hour, uint8_t minue, uint8_t second);

uint8_t fromDecimalToBCD(uint8_t decimalValue) 
{
  return ((decimalValue / 10) * 16) + (decimalValue % 10);
}

uint8_t fromBCDToDecimal(uint8_t BCDValue) 
{
  return ((BCDValue / 16) * 10) + (BCDValue % 16);
}

void TimeSetDate(uint8_t year, uint8_t month, uint8_t dayOfMonth, uint8_t dayOfWeek, uint8_t hour, uint8_t minute, uint8_t second) 
{
  WireBeginTransmission(DS1307_ADDRESS);
  WireWrite(0); //stop oscillator

  //Start sending the new values
  WireWrite(fromDecimalToBCD(second));
  WireWrite(fromDecimalToBCD(minute));
  WireWrite(fromDecimalToBCD(hour));
  WireWrite(fromDecimalToBCD(dayOfWeek));
  WireWrite(fromDecimalToBCD(dayOfMonth));
  WireWrite(fromDecimalToBCD(month));
  WireWrite(fromDecimalToBCD(year));

  WireWrite(0); //start oscillator
  WireEndTransmission();
}

void TimeGetDate(uint8_t *values) 
{
  WireBeginTransmission(DS1307_ADDRESS);
  WireWrite(0); //stop oscillator
  WireEndTransmission();
  WireRequestFrom(DS1307_ADDRESS, 7);

  for (int i = 6; i >= 0; i--) {
    values[i] = fromBCDToDecimal(WireRead());
  }
}
