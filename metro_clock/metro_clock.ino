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
    uint16_t bat = _convert_vcc_bat(Read_VCC());
    indiPrintNum(bat / 100, 0, 1, '0'); //вывод чисел
    indiPrintNum(bat % 100, 1, 2, '0'); //вывод чисел
    indiPrint("V", 3);
    _delay_ms(1000);
  }
  return 0; //конец
}

//--------------------------------Чтение напряжения батареи-------------------------------------
uint8_t Read_VCC(void)  //чтение напряжения батареи
{
  ADMUX = 0b01101110; //выбор внешнего опорного+BG
  ADCSRA = 0b11100111; //настройка АЦП
  _delay_ms(5);
  while ((ADCSRA & 0x10) == 0); //ждем флага прерывания АЦП
  ADCSRA |= 0x10; //сбрасываем флаг прерывания
  uint8_t resu = ADCH; //результат опроса АЦП
  return resu; //возвращаем результат опроса АЦП
}
//-----------------------------------Параметры-----------------------------------------
uint16_t _convert_vcc_bat(uint8_t adc) //параметры
{
  return (1.10 * 255.0) / adc * 100; //состояние батареи
}
