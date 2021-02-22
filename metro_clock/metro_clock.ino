#include "indiDisp.h"
#include "DS1307.h"
#include <avr/delay.h>

int atexit(void (* /*func*/ )()) { //инициализация функций
  return 0;
}

int main(void)  //инициализация
{
  indiInit(); //инициализация индикаторов
  WireBegin();

  //----------------------------------Главная-------------------------------------------------------------
  for (;;) //главная
  {
    uint8_t time[7];
    TimeGetDate(time);
    indiPrintNum(time[4], 0, 2, '0'); //вывод чисел
    indiPrintNum(time[5], 2, 2, '0'); //вывод чисел
    _delay_ms(1000);
  }
  return 0; //конец
}
