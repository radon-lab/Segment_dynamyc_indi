#include "indiDisp.h"
#include <avr/delay.h>

int atexit(void (* /*func*/ )()) { //инициализация функций
  return 0;
}

int main(void)  //инициализация
{
  indiInit(); //инициализация индикаторов

  //----------------------------------Главная-------------------------------------------------------------
  for (;;) //главная
  {
    indiPrint("ABCD", 0); //вывод текста
    _delay_ms(1000);
    indiPrintNum(1234, 0); //вывод чисел
    _delay_ms(1000);
    indiClr(); //очистка индикаторов
    indiSet(3, 0, 1); //вывод символов
    indiSet(1, 1, 1); //вывод символов
    indiSet(5, 2, 1); //вывод символов
    indiSet(6, 3, 1); //вывод символов
    _delay_ms(1000);
    indiPrint("8888", 0); //вывод текста
    indiSetBright(0, 30); //установка яркости индикатора
    indiSetBright(1, 80); //установка яркости индикатора
    indiSetBright(2, 128); //установка яркости индикатора
    indiSetBright(3, 255); //установка яркости индикатора
    _delay_ms(1000);
    for (uint8_t i = 0; i < 4; i++) indiSetBright(i, 255); //установка яркости индикатора
  }
  return 0; //конец
}
