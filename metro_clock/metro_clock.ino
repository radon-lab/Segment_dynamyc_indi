//----------------Библиотеки----------------
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <util/delay.h>

//---------------Конфигурации---------------
#include "config.h"
#include "connection.h"
#include "indiDisp.h"
#include "DS1307.h"

//переменные обработки кнопок
uint8_t btn_tmr; //таймер тиков обработки
boolean btn_check; //флаг разрешения опроса кнопки
boolean btn_state; //флаг текущего состояния кнопки

uint8_t _mode = 0; //текущий основной режим
boolean scr = 0; //флаг обновления экрана
boolean disableSleep = 0; //флаг запрета сна

uint8_t bat = 100; //заряд акб
uint8_t tmr_sleep = 0; //счетчик ухода в сон
boolean _sleep = 0; //влаг активного сна

uint8_t time[7]; //массив времени(год, месяц, день, день_недели, часы, минуты, секунды)

volatile boolean power_off = 0; //флаг отключения питания
volatile uint8_t tick_wdt; //счетчик тиков для обработки данных
uint32_t timer_millis; //таймер отсчета миллисекунд
uint32_t timer_dot; //таймер отсчета миллисекунд для точек

int atexit(void (* /*func*/ )()) { //инициализация функций
  return 0;
}

int main(void)  //инициализация
{
  LEFT_INIT; //инициализация левой кнопки
  RIGHT_INIT; //инициализация правой кнопки
  DOT_INIT; //инициализация точек
  FLASK_INIT; //инициализация колбы
  RTC_BAT_INIT; //инициализация дополнительного питания RTC
  RTC_INIT; //инициализация питания RTC

  EICRA = 0b00000010; //настраиваем внешнее прерывание по спаду импульса на INT0
  PRR = 0b10101111; //отключаем все лишнее (I2C | TIMER0 | TIMER1 | SPI | UART | ADC)
  ACSR |= 1 << ACD; //отключаем компаратор

  indiInit(); //инициализация индикаторов

  _batCheck(); //проверяем заряд акб
  if (bat < LOW_BAT_P) { //если батарея разряжена
    indiPrint("LO", 1); //отрисовка сообщения разряженной батареи
    _delay_ms(2000); //ждём
    _PowerDown(); //выключаем питание
  }
  else {
    TWI_enable(); //включение TWI
    WDT_enable(); //запускаем WatchDog с пределителем 2
    indiPrint("####", 1); //отрисовка сообщения
  }

  if (eeprom_read_byte((uint8_t*)100) != 100) { //если первый запуск, восстанавливаем из переменных
    eeprom_update_byte((uint8_t*)100, 100); //делаем метку
    eeprom_update_block((void*)&timeDefault, 0, sizeof(timeDefault)); //записываем дату по умолчанию в память
  }

  TimeGetDate(time); //синхронизация времени
  if (time[0] < 21 || time[0] > 50) { //если пропадало питание
    indiPrint("INIT", 0);
    eeprom_read_block((void*)&time, 0, sizeof(time)); //считываем дату из памяти
    TimeSetDate(time); //устанавливаем новое время
  }
  for (timer_millis = 2000; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных

  //----------------------------------Главная-------------------------------------------------------------
  for (;;) //главная
  {
    data_convert(); //преобразование данных
    main_screen(); //главный экран
  }
  return 0; //конец
}
//------------------------------------Включение питания----------------------------------------------
ISR(INT0_vect) //внешнее прерывание на пине INT0 - включение питания
{
  _delay_ms(2000); //ждем 2 секунды
  if (!RIGHT_OUT) { //если кнопка не отжата
    _batCheck(); //проверяем заряд акб
    if (bat > PWR_BAT_P) { //если батарея не разряжена
      indiDisableSleep(255); //включаем дисплей
      indiPrint("####", 1); //отрисовка сообщения

      while (!RIGHT_OUT); //ждем пока отпустят кнопу

      TWI_enable(); //включение TWI
      WDT_enable(); //запускаем WatchDog с пределителем 2
      EIMSK = 0b00000000; //запрещаем внешнее прерывание INT0

      power_off = 0; //флаг выключения питания
    }
    else { //иначе выводим предупреждение об разряженной батарее
      indiDisableSleep(255); //включаем дисплей
      indiPrint("LO", 1); //отрисовка сообщения разряженной батареи
      _delay_ms(2000); //ждём
      indiEnableSleep(); //выключаем дисплей
    }
  }
}
//-------------------------Прерывание по переполнению wdt - 17.5мс------------------------------------
ISR(WDT_vect) //прерывание по переполнению wdt - 17.5мс
{
  tick_wdt++; //прибавляем тик
}
//----------------------------------Преобразование данных---------------------------------------------------------
void data_convert(void) //преобразование данных
{
  static uint8_t wdt_time; //счетчик времени
  static uint8_t tmr_bat; //таймер опроса батареи

  sleepMode(); //режим сна

  for (; tick_wdt > 0; tick_wdt--) { //если был тик, обрабатываем данные

    switch (btn_state) { //таймер опроса кнопок
      case 0: if (btn_check) btn_tmr++; break; //считаем циклы
      case 1: if (btn_tmr > 0) btn_tmr--; break; //убираем дребезг
    }

    //счет времени
    if (++wdt_time >= 57) {
      wdt_time = 0;

      if (!_sleep) {
        if (++time[6] > 59) {
          time[6] = 0;
          if (++time[5] > 59) {
            time[5] = 0;
            if (++time[4] > 23)
              time[4] = 0;
          }
          TimeGetDate(time); //синхронизируем время
        }
        scr = 0;
      }
      //опрос акб
      if (tmr_bat >= BAT_TIME) { //если пришло время опросит акб
        tmr_bat = 0; //сбрасываем таймер
        _batCheck(); //проверяем заряд акб
        if (bat < LOW_BAT_P) { //если батарея разряжена
          indiPrint("LO", 1); //отрисовка сообщения разряженной батареи
          _delay_ms(2000); //ждём
          DOT_OFF; //выключаем точку
          _PowerDown(); //выключаем питание
        }
      }
      else tmr_bat++;
      //сон
      if (!disableSleep && tmr_sleep <= 5) tmr_sleep++; //таймер ухода в сон
    }

    if (timer_millis > 17) timer_millis -= 17; //если таймер больше 17мс
    else if (timer_millis) timer_millis = 0; //иначе сбрасываем таймер

    if (timer_dot > 17) timer_dot -= 17; //если таймер больше 17мс
    else if (timer_dot) timer_dot = 0; //иначе сбрасываем таймер
  }
}
//-------------------------------Режим сна----------------------------------------------------
void sleepMode(void) //режим сна
{
  if (!_sleep) save_pwr(); //энергосбережение
  else sleep_pwr(); //иначе

  switch (tmr_sleep) {
    case 0:
      if (_sleep) {
        _sleep = 0;
        _mode = 0;
        TimeGetDate(time); //синхронизируем время
        indiDisableSleep(255); //включаем дисплей
      }
      break;

    case SLEEP_TIME:
      if (!_sleep) {
        _sleep = 1;
        DOT_OFF;
        indiEnableSleep(); //выключаем дисплей
      }
      break;
  }
}
//-------------------------------------Ожидание--------------------------------------------------------
void waint_pwr(void) //ожидание
{
  SMCR = (0x0 << 1) | (1 << SE);  //устанавливаем режим сна idle

  MCUCR = (0x03 << 5); //выкл bod
  MCUCR = (0x02 << 5);

  asm ("sleep");  //с этого момента спим.
}
//-------------------------------------Энергосбережение--------------------------------------------------------
void save_pwr(void) //энергосбережение
{
  SMCR = (0x3 << 1) | (1 << SE);  //устанавливаем режим сна powersave

  MCUCR = (0x03 << 5); //выкл bod
  MCUCR = (0x02 << 5);

  asm ("sleep");  //с этого момента спим.
}
//-------------------------------------Глубокий сон--------------------------------------------------------
void sleep_pwr(void) //uлубокий сон
{
  SMCR = (0x2 << 1) | (1 << SE);  //устанавливаем режим сна powerdown

  MCUCR = (0x03 << 5); //выкл bod
  MCUCR = (0x02 << 5);

  asm ("sleep");  //с этого момента спим.
}
//--------------------------------Чтение напряжения батареи-------------------------------------
uint8_t Read_VCC(void)  //чтение напряжения батареи
{
  ADC_enable(); //включение ADC
  ADMUX = 0b01101110; //выбор внешнего опорного+BG
  ADCSRA = 0b11100111; //настройка АЦП
  _delay_ms(5);
  while ((ADCSRA & 0x10) == 0); //ждем флага прерывания АЦП
  ADCSRA |= 0x10; //сбрасываем флаг прерывания
  uint8_t resu = ADCH; //результат опроса АЦП
  ADC_disable(); //выключение ADC
  return resu; //возвращаем результат опроса АЦП
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
//-------------------------------Включение TWI----------------------------------------------------
void TWI_enable(void) //включение TWI
{
  PRR &= ~ (1 << 7); //включаем питание i2c
  WireBegin(); //инициализация шины i2c
}
//-------------------------------Выключение TWI---------------------------------------------------
void TWI_disable(void) //выключение TWI
{
  PRR |= (1 << 7); //выключаем питание i2c
}
//----------------------------------------------------------------------------------
void _PowerDown(void)
{
  EIMSK = 0b00000001; //разрешаем внешнее прерывание INT0

  indiEnableSleep(); //выключаем дисплей
  TWI_disable(); //выключение TWI
  WDT_disable(); //выключение WDT
  power_off = 1; //устанавливаем флаг
  while (power_off) sleep_pwr();
}
//----------------------------------------------------------------------------------
void _batCheck(void)
{
  uint16_t vcc = (1.10 * 255.0) / Read_VCC() * 100;
  bat = map(constrain(vcc, BAT_MIN_V, BAT_MAX_V), BAT_MIN_V, BAT_MAX_V, 0, 100); //состояние батареи
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
          if (!_sleep) { //если не спим
            btn_set = 2; //поднимаем признак удержания
            tmr_sleep = 0; //сбрасываем таймер сна
          }
          btn_check = 0; //запрещем проврку кнопки
        }
      }
      break;

    case 1:
      if (btn_tmr > BTN_GIST_TICK) { //если таймер больше времени антидребезга
        btn_tmr = BTN_GIST_TICK; //сбрасываем таймер на антидребезг
        if (!_sleep) btn_set = 1; //если не спим, поднимаем признак нажатия
        btn_check = 0; //запрещем проврку кнопки
        tmr_sleep = 0; //сбрасываем таймер сна
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
//----------------------------------------------------------------------------------
void settings_time(void)
{
  uint8_t cur_mode = 0;
  boolean blink_data = 0;

  disableSleep = 1; //запрещаем сон

  DOT_ON; //включаем точку
  indiClr();
  indiPrint("SET", 0);
  for (timer_millis = 1000; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных

  //настройки
  while (1) {
    data_convert(); //преобразование данных

    if (!scr) {
      scr = 1; //сбрасываем флаг
      indiClr(); //очистка индикаторов
      switch (cur_mode) {
        case 0:
        case 1:
          if (!blink_data || cur_mode == 1) indiPrintNum(time[4], 0, 2, '0'); //вывод чисел
          if (!blink_data || cur_mode == 0) indiPrintNum(time[5], 2, 2, '0'); //вывод чисел
          break;
        case 2:
        case 3:
          if (!blink_data || cur_mode == 3) indiPrintNum(time[1], 0, 2, '0'); //вывод чисел
          if (!blink_data || cur_mode == 2) indiPrintNum(time[2], 2, 2, '0'); //вывод чисел
          break;
        case 4:
          indiPrint("20", 0); //вывод чисел
          if (!blink_data) indiPrintNum(time[0], 2, 2, '0'); //вывод чисел
          break;
      }
      blink_data = !blink_data;
    }

    //+++++++++++++++++++++  опрос кнопок  +++++++++++++++++++++++++++
    switch (check_keys()) {
      case 1: //left click
        switch (cur_mode) {
          //настройка времени
          case 0: if (time[4] > 0) time[4]--; else time[4] = 23; break; //часы
          case 1: if (time[5] > 0) time[5]--; else time[5] = 59; break; //минуты

          //настройка даты
          case 2: if (time[1] > 1) time[1]--; else time[1] = 12; time[2] = 1; break; //месяц
          case 3: if (time[2] > 1 ) time[2]--; else time[2] = (time[1] == 2 && !(time[0] % 4)) ? 1 : 0 + pgm_read_byte(&daysInMonth[time[1] - 1]); break; //день

          //настройка года
          case 4: if (time[0] > 20) time[0]--; else time[0] = 50; break; //год
        }
        scr = blink_data = time[6] = 0; //сбрасываем флаги
        break;

      case 2: //right click
        switch (cur_mode) {
          //настройка времени
          case 0: if (time[4] < 23) time[4]++; else time[4] = 0; break; //часы
          case 1: if (time[5] < 59) time[5]++; else time[5] = 0; break; //минуты

          //настройка даты
          case 2: if (time[1] < 12) time[1]++; else time[1] = 1; time[2] = 1; break; //месяц
          case 3: if (time[2] < pgm_read_byte(&daysInMonth[time[1] - 1]) + (time[1] == 2 && !(time[0] % 4)) ? 1 : 0) time[2]++; else time[2] = 1; break; //день

          //настройка года
          case 4: if (time[0] < 50) time[0]++; else time[0] = 21; break; //год
        }
        scr = blink_data = time[6] = 0; //сбрасываем флаги
        break;

      case 3: //left hold
        if (cur_mode < 4) cur_mode++; else cur_mode = 0;
        switch (cur_mode) {
          case 0:
            indiClr(); //очистка индикаторов
            indiPrint("T", 0);
            for (timer_millis = 500; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
            break;

          case 2:
            indiClr(); //очистка индикаторов
            indiPrint("D", 0);
            for (timer_millis = 500; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
            break;

          case 4:
            indiClr(); //очистка индикаторов
            indiPrint("Y", 0);
            for (timer_millis = 500; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
            break;
        }
        scr = blink_data = time[6] = 0; //сбрасываем флаги
        break;

      case 4: //right hold
        eeprom_update_block((void*)&time, 0, sizeof(time)); //записываем дату в память
        TimeSetDate(time); //обновляем время
        DOT_OFF; //выключаем точку
        indiClr(); //очистка индикаторов
        indiPrint("OUT", 0);
        for (timer_millis = 1000; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
        disableSleep = 0; //разрешаем сон
        scr = 0; //обновляем экран
        return;
    }
  }
}
//-----------------------------Главный экран------------------------------------------------
void main_screen(void) //главный экран
{
  if (!scr) {
    scr = 1; //сбрасываем флаг
    switch (_mode) {
      case 0:
        indiPrintNum(time[4], 0, 2, '0'); //вывод чисел
        indiPrintNum(time[5], 2, 2, '0'); //вывод чисел
        break;
      case 1:
        indiPrintNum(time[1], 0, 2, '0'); //вывод чисел
        indiPrintNum(time[2], 2, 2, '0'); //вывод чисел
        DOT_ON;
        break;
      case 2:
        indiPrint("B", 0);
        indiPrintNum(bat, 1, 3, ' '); //вывод чисел
        DOT_OFF;
        break;
    }
  }

  if (!_sleep && !_mode && !timer_dot) {
    DOT_INV; //инвертируем точки
    timer_dot = 500;
  }

  switch (check_keys()) {
    case 1: //left key press
      if (_mode > 0) _mode--;
      else _mode = 2;
      scr = 0;
      break;

    case 2: //right key press
      if (_mode < 3) _mode++;
      else _mode = 0;
      scr = 0;
      break;

    case 3: //left key hold
      settings_time(); //настройки
      break;

    case 4: //right key hold
      break;
  }
}
