#include "wire.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DS1307_ADDRESS 0x68

void TimeGetDate(uint8_t *values);
void TimeSetDate(const uint8_t *values);

uint8_t fromDecimalToBCD(uint8_t decimalValue)
{
  return ((decimalValue / 10) * 16) + (decimalValue % 10);
}

uint8_t fromBCDToDecimal(uint8_t BCDValue)
{
  return ((BCDValue / 16) * 10) + (BCDValue % 16);
}

void TimeSetDate(const uint8_t *values) // year, month, dayOfMonth, dayOfWeek, hour, minute, second
{
  RTC_ON;
  RTC_BAT_OFF;
  
  WireBeginTransmission(DS1307_ADDRESS);
  WireWrite(0x00);

  //Start sending the new values
  WireWrite(fromDecimalToBCD(values[6]));
  WireWrite(fromDecimalToBCD(values[5]));
  WireWrite(fromDecimalToBCD(values[4]));
  WireWrite(fromDecimalToBCD(values[3]));
  WireWrite(fromDecimalToBCD(values[2]));
  WireWrite(fromDecimalToBCD(values[1]));
  WireWrite(fromDecimalToBCD(values[0]));

  WireWrite(0x00);
  WireEndTransmission();

  RTC_BAT_ON;
  RTC_OFF;
}

void TimeGetDate(uint8_t *values) // year, month, dayOfMonth, dayOfWeek, hour, minute, second
{
  RTC_ON;
  RTC_BAT_OFF;
  
  WireBeginTransmission(DS1307_ADDRESS);
  WireWrite(0x00);
  WireEndTransmission();
  WireRequestFrom(DS1307_ADDRESS, 7);

  for (int i = 6; i >= 0; i--) {
    values[i] = fromBCDToDecimal(WireRead());
  }

  RTC_BAT_ON;
  RTC_OFF;
}
