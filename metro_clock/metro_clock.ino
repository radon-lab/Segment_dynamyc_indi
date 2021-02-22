#include "indiDisp.h"
#include "DS1307.h"
#include <avr/delay.h>

#define BTN_GIST_TICK    2   //количество циклов для защиты от дребезга(0..255)(1 цикл -> +-17.5мс)
#define BTN_HOLD_TICK    30  //количество циклов после которого считается что кнопка зажата(0..255)(1 цикл -> +-17.5мс)

//переменные обработки кнопок
uint8_t btn_tmr; //таймер тиков обработки
boolean btn_check; //флаг разрешения опроса кнопки
boolean btn_state; //флаг текущего состояния кнопки

uint8_t mode = 0;
boolean scr = 0;

volatile uint8_t tick_wdt; //счетчик тиков для обработки данных
uint32_t timer_millis; //таймер отсчета миллисекунд

int atexit(void (* /*func*/ )()) { //инициализация функций
  return 0;
}

#define DDR_REG(portx)  (*(&portx-1))

//назначаем кнопки//
//пин кнопки RIGHT D2
#define RIGHT_BIT   2 // D2
#define RIGHT_PORT  PORTD
#define RIGHT_PIN   PIND

#define RIGHT_OUT   (bitRead(RIGHT_PIN, RIGHT_BIT))
#define RIGHT_SET   (bitSet(RIGHT_PORT, RIGHT_BIT))
#define RIGHT_INP   (bitClear((DDR_REG(RIGHT_PORT)), RIGHT_BIT))

#define RIGHT_INIT  RIGHT_SET; RIGHT_INP

//пин кнопки LEFT D0
#define LEFT_BIT   0 // D0
#define LEFT_PORT  PORTD
#define LEFT_PIN   PIND

#define LEFT_OUT   (bitRead(LEFT_PIN, LEFT_BIT))
#define LEFT_SET   (bitSet(LEFT_PORT, LEFT_BIT))
#define LEFT_INP   (bitClear((DDR_REG(LEFT_PORT)), LEFT_BIT))

#define LEFT_INIT  LEFT_SET; LEFT_INP

//пин точек D5
#define DOT_BIT   5 // D5
#define DOT_PORT  PORTD

#define DOT_INV   (DOT_PORT ^= (1 << DOT_BIT))
#define DOT_ON    (bitSet(DOT_PORT, DOT_BIT))
#define DOT_OFF   (bitClear(DOT_PORT, DOT_BIT))
#define DOT_OUT   (bitSet((DDR_REG(DOT_PORT)), DOT_BIT))

#define DOT_INIT  DOT_OFF; DOT_OUT

//пин колбы D9
#define FLASK_BIT   1 // D9
#define FLASK_PORT  PORTB

#define is_FLASK_ON   (bitRead(FLASK_PORT, FLASK_BIT))
#define FLASK_ON      (bitSet(FLASK_PORT, FLASK_BIT))
#define FLASK_OFF     (bitClear(FLASK_PORT, FLASK_BIT))
#define FLASK_OUT     (bitSet((DDR_REG(FLASK_PORT)), FLASK_BIT))

#define FLASK_INIT  FLASK_OFF; FLASK_OUT

int main(void)  //инициализация
{
  LEFT_INIT; //инициализация левой кнопки
  RIGHT_INIT; //инициализация правой кнопки
  DOT_INIT; //инициализация точек
  FLASK_INIT; //инициализация колбы

  indiInit(); //инициализация индикаторов
  WireBegin(); //инициализация шины i2c
  WDT_enable(); //запускаем WatchDog с пределителем 2

  //----------------------------------Главная-------------------------------------------------------------
  for (;;) //главная
  {
    data_convert(); //преобразование данных

    if (!scr) {
      scr = 1;
      
      uint8_t time[7];
      switch (mode) {
        case 0:
          TimeGetDate(time);
          indiPrintNum(time[4], 0, 2, '0'); //вывод чисел
          indiPrintNum(time[5], 2, 2, '0'); //вывод чисел
          break;
        case 1:
          TimeGetDate(time);
          indiPrintNum(time[1], 0, 2, '0'); //вывод чисел
          indiPrintNum(time[2], 2, 2, '0'); //вывод чисел
          DOT_ON;
          break;
        case 2:
          uint16_t bat = _convert_vcc_bat(Read_VCC());
          indiPrintNum(bat / 100, 0, 1, '0'); //вывод чисел
          indiPrintNum(bat % 100, 1, 2, '0'); //вывод чисел
          indiPrint("V", 3);
          DOT_OFF;
          break;
      }
    }

    if (!mode && !timer_millis) {
      DOT_INV;
      timer_millis = 500;
    }

    switch (check_keys()) {
      case 1: //left key press
        if (++mode < 3) mode = 0;
        break;

      case 2: //right key press
        if (++mode < 3) mode = 0;
        break;

      case 3: //left key hold
        break;

      case 4: //right key hold
        break;
    }
  }
  return 0; //конец
}
//-------------------------Прерывание по переполнению wdt - 17.5мс------------------------------------
ISR(WDT_vect) //прерывание по переполнению wdt - 17.5мс
{
  tick_wdt++; //прибавляем тик
}
//----------------------------------Преобразование данных---------------------------------------------------------
void data_convert(void) //преобразование данных
{
  static uint8_t wdt_time;

  for (; tick_wdt > 0; tick_wdt--) { //если был тик, обрабатываем данные

    if (++wdt_time >= 57) {
      scr = wdt_time = 0;
    }

    switch (btn_state) { //таймер опроса кнопок
      case 0: if (btn_check) btn_tmr++; break; //считаем циклы
      case 1: if (btn_tmr > 0) btn_tmr--; break; //убираем дребезг
    }

    if (timer_millis > 17) timer_millis -= 17; //если таймер больше 17мс
    else if (timer_millis) timer_millis = 0; //иначе сбрасываем таймер
  }
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
//-------------------------------Включение WDT----------------------------------------------------
void WDT_enable(void) //включение WDT
{
  uint8_t sregCopy = SREG; //Сохраняем глобальные прерывания
  cli(); //Запрещаем глобальные прерывания
  WDTCSR = ((1 << WDCE) | (1 << WDE)); //Сбрасываем собаку
  WDTCSR = 0x40; //Устанавливаем пределитель 2(режим прерываний)
  SREG = sregCopy; //Восстанавливаем глобальные прерывания
}
//-------------------------------Выключение WDT---------------------------------------------------
void WDT_disable(void) //выключение WDT
{
  uint8_t sregCopy = SREG; //Сохраняем глобальные прерывания
  cli(); //Запрещаем глобальные прерывания
  WDTCSR = ((1 << WDCE) | (1 << WDE)); //Сбрасываем собаку
  WDTCSR = 0x00; //Выключаем собаку
  SREG = sregCopy; //Восстанавливаем глобальные прерывания
}
//-------------------------------Включение ADC----------------------------------------------------
void ADC_enable(void) //включение ADC
{
  PRR &= ~ (1 << 0); //включаем питание АЦП
  ADCSRA |= (1 << ADEN); //включаем ацп
}
//-------------------------------Выключение ADC---------------------------------------------------
void ADC_disable(void) //выключение ADC
{
  ADCSRA &= ~ (1 << ADEN); //выключаем ацп
  PRR |= (1 << 0); //выключаем питание ацп
}
//-----------------------------Проверка кнопок----------------------------------------------------
uint8_t check_keys(void) //проверка кнопок
{
  static uint8_t btn_set; //флаг признака действия
  static uint8_t btn_switch; //флаг мультиопроса кнопок

  switch (btn_switch) { //переключаемся в зависимости от состояния мультиопроса
    case 0:
      if (!LEFT_OUT) { //если нажата кл. ок
        btn_switch = 1; //выбираем клавишу опроса
        btn_state = 0; //обновляем текущее состояние кнопки
      }
      else if (!RIGHT_OUT) { //если нажата кл. вниз
        btn_switch = 2; //выбираем клавишу опроса
        btn_state = 0; //обновляем текущее состояние кнопки
      }
      else btn_state = 1; //обновляем текущее состояние кнопки
      break;
    case 1: btn_state = LEFT_OUT; break; //опрашиваем клавишу ок
    case 2: btn_state = RIGHT_OUT; break; //опрашиваем клавишу вниз
  }

  switch (btn_state) { //переключаемся в зависимости от состояния клавиши
    case 0:
      if (btn_check) { //если разрешена провекрка кнопки
        if (btn_tmr > BTN_HOLD_TICK) { //если таймер больше длительности удержания кнопки
          btn_tmr = BTN_GIST_TICK; //сбрасываем таймер на антидребезг
          //if (!light && !sleep) { //если не спим и если подсветка включена
          btn_set = 2; //поднимаем признак удержания
          //cnt_pwr = 0; //сбрасываем таймер сна
          //}
          btn_check = 0; //запрещем проврку кнопки
        }
      }
      break;

    case 1:
      if (btn_tmr > BTN_GIST_TICK) { //если таймер больше времени антидребезга
        btn_tmr = BTN_GIST_TICK; //сбрасываем таймер на антидребезг
        //if (!light && !sleep)
        btn_set = 1; //если не спим и если подсветка включена, поднимаем признак нажатия
        btn_check = 0; //запрещем проврку кнопки
        //cnt_pwr = 0; //сбрасываем таймер сна
      }
      else if (!btn_tmr) {
        btn_check = 1; //разрешаем проврку кнопки
        btn_switch = 0; //сбрасываем мультиопрос кнопок
      }
      break;
  }

  switch (btn_set) { //переключаемся в зависимости от признака нажатия
    case 0: return 0; //клавиша не нажата, возвращаем 0
    case 1:
      btn_set = 0; //сбрасываем признак нажатия
      switch (btn_switch) { //переключаемся в зависимости от состояния мультиопроса
        case 1: return 1; //left press, возвращаем 1
        case 2: return 2; //right press, возвращаем 2
      }
      break;

    case 2:
      btn_set = 0; //сбрасываем признак нажатия
      switch (btn_switch) { //переключаемся в зависимости от состояния мультиопроса
        case 1: return 3; //left hold, возвращаем 3
        case 2: return 4; //right hold, возвращаем 4
      }
      break;
  }
}
