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
uint8_t _timer_mode = 0; //текущий режим таймера(0-выкл | 1-настройка | 2-вкл | 3-пауза)
uint8_t _timer_preset = 0; //текущий номер выбранного пресета таймера
uint16_t _timer_secs = timerDefault[_timer_preset] * 60; //установленное время таймера

uint8_t _flask_mode = 2; //текущий режим свечения колбы
uint8_t _bright_mode = 0; //текущий режим подсветки
uint8_t _bright_levle = 0; //текущая яркость подсветки

uint8_t timeBright[] = { 23, 8 }; //массив времени 0 - ночь, 1 - день
uint8_t indiBright[] = { 0, 4 }; //массив подсветки 0 - ночь, 1 - день

const uint8_t allModes[3] = {2, 3, 5}; //всего режимов

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

  dot_state = 1; //включаем точки
  flask_state = 1; //включаем колбу

  if (eeprom_read_byte((uint8_t*)100) != 100) { //если первый запуск, восстанавливаем из переменных
    eeprom_update_byte((uint8_t*)100, 100); //делаем метку
    eeprom_update_block((void*)&timeDefault, 0, sizeof(timeDefault)); //записываем дату по умолчанию в память
    eeprom_update_block((void*)&timeBright, 7, sizeof(timeBright)); //записываем время в память
    eeprom_update_block((void*)&indiBright, 9, sizeof(indiBright)); //записываем яркость в память
    eeprom_update_byte((uint8_t*)11, _flask_mode); //записываем в память режим колбы
    eeprom_update_byte((uint8_t*)12, _bright_mode); //записываем в память режим подсветки
    eeprom_update_byte((uint8_t*)13, _bright_levle); //записываем в память уровень подсветки
  }
  else {
    eeprom_read_block((void*)&timeBright, 7, sizeof(timeBright)); //считываем время из памяти
    eeprom_read_block((void*)&indiBright, 9, sizeof(indiBright)); //считываем яркость из памяти
    _flask_mode = eeprom_read_byte((uint8_t*)11); //считываем режим колбы из памяти
    _bright_mode = eeprom_read_byte((uint8_t*)12); //считываем режим подсветки из памяти
    _bright_levle = eeprom_read_byte((uint8_t*)13); //считываем уровень подсветки из памяти
  }

  TimeGetDate(time); //синхронизация времени
  if (time[0] < 21 || time[0] > 50) { //если пропадало питание
    indiPrint("INIT", 0);
    eeprom_read_block((void*)&time, 0, sizeof(time)); //считываем дату из памяти
    TimeSetDate(time); //устанавливаем новое время
  }
  for (timer_millis = 2000; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных

  flask_state = _flask_mode; //обновление стотояния колбы

  switch (_bright_mode) {
    case 0: indiSetBright(brightDefault[_bright_levle]); break; //установка яркости индикаторов
    case 1: indiSetBright(brightDefault[indiBright[readLightSens()]]); break; //установка яркости индикаторов
    case 2: indiSetBright(brightDefault[indiBright[changeBright()]]); break; //установка яркости индикаторов
  }

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
      TimeGetDate(time); //синхронизируем время

      switch (_bright_mode) {
        case 0: indiSetBright(brightDefault[_bright_levle]); break; //установка яркости индикаторов
        case 1: indiSetBright(brightDefault[indiBright[readLightSens()]]); break; //установка яркости индикаторов
        case 2: indiSetBright(brightDefault[indiBright[changeBright()]]); break; //установка яркости индикаторов
      }

      indiDisableSleep(); //включаем дисплей
      indiPrint("####", 1); //отрисовка сообщения

      dot_state = 1; //включаем точки
      flask_state = 1; //включаем колбу

      while (!RIGHT_OUT); //ждем пока отпустят кнопу

      TWI_enable(); //включение TWI
      WDT_enable(); //запускаем WatchDog с пределителем 2
      EIMSK = 0b00000000; //запрещаем внешнее прерывание INT0

      power_off = 0; //флаг выключения питания
    }
    else { //иначе выводим предупреждение об разряженной батарее
      indiSetBright(127); //устанавливаем максимальную яркость
      indiDisableSleep(); //включаем дисплей
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

    if (++wdt_time >= 57) {
      wdt_time = 0;

      if (!_sleep) {
        //счет времени
        if (++time[6] > 59) { //секунды
          time[6] = 0;
          if (++time[5] > 59) { //минуты
            time[5] = 0;
            if (++time[4] > 23) { //часы
              time[4] = 0;
            }
            if (!disableSleep && _bright_mode == 2) indiSetBright(brightDefault[indiBright[changeBright()]]); //установка яркости индикаторов
          }
          TimeGetDate(time); //синхронизируем время
        }
        if (_flask_mode == 2) flask_state = readLightSens(); //автоматическое включение колбы
        if (!disableSleep && _bright_mode == 1) indiSetBright(brightDefault[indiBright[readLightSens()]]); //установка яркости индикаторов
        scr = 0;
      }
      //таймер часов
      if (_timer_mode == 2 && _timer_secs) {
        _timer_secs--;
        //оповещение окончания таймера
        if (!_timer_secs) {
          dot_state = 0; //выключаем точки
          tmr_sleep = 0; //сбрасываем таймер сна
          disableSleep = 1; //запрещаем сон
          sleepOut(); //выход из сна
          for (timer_millis = 10000; timer_millis && !check_keys();) {
            data_convert(); // ждем, преобразование данных
            if (!timer_dot) {
              indiClr();
              if (!flask_state) {
                indiPrint("TOUT", 0);
              }
              flask_state = !flask_state; //инвертируем колбу
              timer_dot = 500;
            }
          }
          _mode = 0; //переходим в режим часов
          _timer_mode = 0; //сбрасываем режим таймера
          disableSleep = 0; //разрешаем сон
        }
      }
      //опрос акб
      if (tmr_bat >= BAT_TIME) { //если пришло время опросит акб
        tmr_bat = 0; //сбрасываем таймер
        _batCheck(); //проверяем заряд акб
        if (bat < LOW_BAT_P) { //если батарея разряжена
          indiPrint("LO", 1); //отрисовка сообщения разряженной батареи
          _delay_ms(2000); //ждём
          dot_state = 0; //выключаем точку
          _PowerDown(); //выключаем питание
        }
      }
      else tmr_bat++; //иначе прибавляем время
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
  else sleep_pwr(); //иначе сон

  switch (tmr_sleep) {
    case 0:
      if (_sleep) sleepOut(); //выход из сна
      break;

    case SLEEP_TIME:
      if (!_sleep) {
        _sleep = 1; //устанавливаем флаг активного сна
        TWI_disable(); //выключение TWI
        indiEnableSleep(); //выключаем дисплей
      }
      break;
  }
}
//-------------------------------------Выход из сна--------------------------------------------------------
void sleepOut(void) //выход из сна
{
  _sleep = 0; //сбрасываем флаг активного сна
  if (!_timer_mode) _mode = 0; //если таймер не работает, переходим в режим часов
  TWI_enable(); //включение TWI
  TimeGetDate(time); //синхронизируем время
  switch (_bright_mode) {
    case 1: indiSetBright(brightDefault[indiBright[readLightSens()]]); break; //установка яркости индикаторов
    case 2: indiSetBright(brightDefault[indiBright[changeBright()]]); break; //установка яркости индикаторов
  }
  indiDisableSleep(); //включаем дисплей
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
  SMCR = (0x7 << 1) | (1 << SE);  //устанавливаем режим сна extstandby

  MCUCR = (0x03 << 5); //выкл bod
  MCUCR = (0x02 << 5);

  asm ("sleep");  //с этого момента спим.
}
//-------------------------------------Глубокий сон-----------------------------------------------------------
void sleep_pwr(void) //uлубокий сон
{
  SMCR = (0x2 << 1) | (1 << SE);  //устанавливаем режим сна powerdown

  MCUCR = (0x03 << 5); //выкл bod
  MCUCR = (0x02 << 5);

  asm ("sleep");  //с этого момента спим.
}
//-------------------------------Чтение датчика освещённости--------------------------------------------------
boolean readLightSens(void) //чтение датчика освещённости
{
  uint16_t result = 0; //результат опроса АЦП внутреннего опорного напряжения
  ADC_enable(); //включение ADC
  ADMUX = 0b01100110; //выбор внешнего опорного и А6
  ADCSRA = 0b11100111; //настройка АЦП

  for (uint8_t i = 0; i < 10; i++) { //делаем 10 замеров
    while ((ADCSRA & 0x10) == 0); //ждем флага прерывания АЦП
    ADCSRA |= 0x10; //сбрасываем флаг прерывания
    result += ADCH; //прибавляем замер в буфер
  }
  result /= 10; //находим среднее значение
  ADC_disable(); //выключение ADC
  return (result > 170) ? 1 : 0; //возвращаем результат
}
//----------------------------------Чтение напряжения батареи-------------------------------------------------
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
  dot_state = 0; //выключаем точки
  flask_state = 0; //выключаем колбу
  power_off = 1; //устанавливаем флаг
  while (power_off) sleep_pwr();
}
//----------------------------------------------------------------------------------
void _batCheck(void)
{
  uint16_t vcc = (1.10 * 255.0) / Read_VCC() * 100; //рассчитываем напряжение
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
  uint8_t cur_mode = 0; //текущий режим
  boolean blink_data = 0; //мигание сигментами

  disableSleep = 1; //запрещаем сон

  dot_state = 1; //включаем точку
  indiClr(); //очищаем индикаторы
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
          if (!blink_data || cur_mode == 1) indiPrintNum(time[4], 0, 2, '0'); //вывод часов
          if (!blink_data || cur_mode == 0) indiPrintNum(time[5], 2, 2, '0'); //вывод минут
          break;
        case 2:
        case 3:
          if (!blink_data || cur_mode == 3) indiPrintNum(time[1], 0, 2, '0'); //вывод месяца
          if (!blink_data || cur_mode == 2) indiPrintNum(time[2], 2, 2, '0'); //вывод даты
          break;
        case 4:
          indiPrint("20", 0); //вывод 2000
          if (!blink_data) indiPrintNum(time[0], 2, 2, '0'); //вывод года
          break;
      }
      blink_data = !blink_data; //мигание сигментами
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
        if (_bright_mode == 2) indiSetBright(brightDefault[indiBright[changeBright()]]); //установка яркости индикаторов
        TimeSetDate(time); //обновляем время
        dot_state = 0; //выключаем точку
        indiClr(); //очистка индикаторов
        indiPrint("OUT", 0);
        for (timer_millis = 1000; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
        disableSleep = 0; //разрешаем сон
        _mode = 0; //переходим в режим часов
        scr = 0; //обновляем экран
        return;
    }
  }
}
//----------------------------------------------------------------------------------
void settings_bright(void)
{
  uint8_t cur_mode = 0; //текущий режим
  boolean blink_data = 0; //мигание сигментами

  disableSleep = 1; //запрещаем сон

  dot_state = 0; //выключаем точку
  indiClr(); //очищаем индикаторы
  indiPrint("BRI", 0);
  for (timer_millis = 1000; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
  if (_bright_mode) indiSetBright(brightDefault[indiBright[1]]); //установка яркости индикаторов

  //настройки
  while (1) {
    data_convert(); //преобразование данных

    if (!scr) {
      scr = 1; //сбрасываем флаг
      indiClr(); //очистка индикаторов
      switch (cur_mode) {
        case 0:
          indiPrint("FL", 0); //вывод 2000
          if (!blink_data) indiPrintNum(_flask_mode, 3); //режим колбы
          break;
        case 1:
          indiPrint("BR", 0);
          if (!blink_data) indiPrintNum(_bright_mode, 3); //режим подсветки
          break;
        case 2:
          switch (_bright_mode) {
            case 0: //ручная подсветка
              indiPrint("L", 0);
              if (!blink_data) indiPrintNum(_bright_levle + 1, 2, 2, '0'); //вывод яркости
              break;

            case 1: //авто-подсветка
              indiPrint("N", 0);
              if (!blink_data) indiPrintNum(indiBright[0] + 1, 2, 2, '0'); //вывод яркости ночь
              break;

            case 2: //подсветка день/ночь
              indiPrint("N", 0);
              if (!blink_data) indiPrintNum(timeBright[0], 2, 2, '0'); //вывод время включения ночной подсветки
              break;
          }
          break;
        case 3:
          switch (_bright_mode) {
            case 1:
              indiPrint("D", 0);
              if (!blink_data) indiPrintNum(indiBright[1] + 1, 2, 2, '0'); //вывод яркости день
              break;

            case 2:
              indiPrint("L", 0); //вывод 2000
              if (!blink_data) indiPrintNum(indiBright[0] + 1, 3); //вывод яркости ночь
              break;
          }
          break;
        case 4:
          indiPrint("D", 0);
          if (!blink_data) indiPrintNum(timeBright[1], 2, 2, '0'); //вывод время включения дневной подсветки
          break;
        case 5:
          indiPrint("L", 0); //вывод 2000
          if (!blink_data) indiPrintNum(indiBright[1] + 1, 3); //вывод яркости день
          break;
      }
      blink_data = !blink_data; //мигание сигментами
    }

    //+++++++++++++++++++++  опрос кнопок  +++++++++++++++++++++++++++
    switch (check_keys()) {
      case 1: //left click
        switch (cur_mode) {
          //настройка режима подсветки и колбы
          case 0:
            if (_flask_mode > 0) _flask_mode--; else _flask_mode = 2;
            flask_state = _flask_mode; //обновление стотояния колбы
            break;
          case 1:
            if (_bright_mode > 0) _bright_mode--; else _bright_mode = 2;
            switch (_bright_mode) {
              case 0: indiSetBright(brightDefault[_bright_levle]); break; //установка яркости индикаторов
              case 1:
              case 2:
                indiSetBright(brightDefault[indiBright[0]]); //установка яркости индикаторов
                break;
            }
            break;

          //настройка ночной подсветки
          case 2:
            switch (_bright_mode) {
              case 0: //ручная подсветка
                if (_bright_levle > 0) _bright_levle--; else _bright_levle = 4;
                indiSetBright(brightDefault[_bright_levle]); //установка яркости индикаторов
                break;

              case 1: //авто-подсветка
                if (indiBright[0] > 0) indiBright[0]--; else indiBright[0] = 4;
                indiSetBright(brightDefault[indiBright[0]]); //установка яркости индикаторов
                break;

              case 2: //часы
                if (timeBright[0] > 0) timeBright[0]--; else timeBright[0] = 23; //часы
                break;
            }
            break;
          case 3:
            switch (_bright_mode) {
              case 1:
                if (indiBright[1] > 0) indiBright[1]--; else indiBright[1] = 4;
                indiSetBright(brightDefault[indiBright[1]]); //установка яркости индикаторов
                break;

              case 2:
                if (indiBright[0] > 0) indiBright[0]--; else indiBright[0] = 4;
                indiSetBright(brightDefault[indiBright[0]]); //установка яркости индикаторов
                break;
            }
            break;

          //настройка дневной подсветки
          case 4: if (timeBright[1] > 0) timeBright[1]--; else timeBright[1] = 23; break; //часы
          case 5:
            if (indiBright[1] > 0) indiBright[1]--; else indiBright[1] = 4;
            indiSetBright(brightDefault[indiBright[1]]); //установка яркости индикаторов
            break;
        }
        scr = blink_data = 0; //сбрасываем флаги
        break;

      case 2: //right click
        switch (cur_mode) {
          //настройка режима подсветки и колбы
          case 0:
            if (_flask_mode < 2) _flask_mode++; else _flask_mode = 0;
            flask_state = _flask_mode; //обновление стотояния колбы
            break;
          case 1:
            if (_bright_mode < 2) _bright_mode++; else _bright_mode = 0;
            switch (_bright_mode) {
              case 0: indiSetBright(brightDefault[_bright_levle]); break; //установка яркости индикаторов
              case 1:
              case 2:
                indiSetBright(brightDefault[indiBright[0]]); //установка яркости индикаторов
                break;
            }
            break;

          //настройка ночной подсветки
          case 2:
            switch (_bright_mode) {
              case 0: //ручная подсветка
                if (_bright_levle < 4) _bright_levle++; else _bright_levle = 0;
                indiSetBright(brightDefault[_bright_levle]); //установка яркости индикаторов
                break;

              case 1: //авто-подсветка
                if (indiBright[0] < 4) indiBright[0]++; else indiBright[0] = 0;
                indiSetBright(brightDefault[indiBright[0]]); //установка яркости индикаторов
                break;

              case 2: //часы
                if (timeBright[0] < 23) timeBright[0]++; else timeBright[0] = 0; //часы
                break;
            }
            break;
          case 3:
            switch (_bright_mode) {
              case 1:
                if (indiBright[1] < 4) indiBright[1]++; else indiBright[1] = 0;
                indiSetBright(brightDefault[indiBright[1]]); //установка яркости индикаторов
                break;

              case 2:
                if (indiBright[0] < 4) indiBright[0]++; else indiBright[0] = 0;
                indiSetBright(brightDefault[indiBright[0]]); //установка яркости индикаторов
                break;
            }
            break;

          //настройка дневной подсветки
          case 4: if (timeBright[1] < 23) timeBright[1]++; else timeBright[1] = 0; break; //часы
          case 5:
            if (indiBright[1] < 4) indiBright[1]++; else indiBright[1] = 0;
            indiSetBright(brightDefault[indiBright[1]]); //установка яркости индикаторов
            break;
        }
        scr = blink_data = 0; //сбрасываем флаги
        break;

      case 3: //left hold
        if (cur_mode < allModes[_bright_mode]) cur_mode++; else cur_mode = 0;
        switch (cur_mode) {
          case 0:
            indiClr(); //очистка индикаторов
            indiPrint("F", 0);
            for (timer_millis = 500; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
            if (_bright_mode) indiSetBright(brightDefault[indiBright[1]]); //установка яркости индикаторов
            break;

          case 1:
            indiClr(); //очистка индикаторов
            indiPrint("B", 0);
            for (timer_millis = 500; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
            break;

          case 2:
            indiClr(); //очистка индикаторов
            if (_bright_mode) indiPrint("N", 0);
            else indiPrint("L", 0);
            for (timer_millis = 500; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
            if (_bright_mode) indiSetBright(brightDefault[indiBright[0]]); //установка яркости индикаторов
            else indiSetBright(brightDefault[_bright_levle]); //установка яркости индикаторов
            break;

          case 3:
            if (_bright_mode == 1) {
              indiClr(); //очистка индикаторов
              indiPrint("D", 0);
              for (timer_millis = 500; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
              indiSetBright(brightDefault[indiBright[1]]); //установка яркости индикаторов
            }
            break;

          case 4:
            indiClr(); //очистка индикаторов
            indiPrint("D", 0);
            for (timer_millis = 500; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
            indiSetBright(brightDefault[indiBright[1]]); //установка яркости индикаторов
            break;
        }
        scr = blink_data = 0; //сбрасываем флаги
        break;

      case 4: //right hold
        eeprom_update_block((void*)&timeBright, 7, sizeof(timeBright)); //записываем время в память
        eeprom_update_block((void*)&indiBright, 9, sizeof(indiBright)); //записываем яркость в память
        eeprom_update_byte((uint8_t*)11, _flask_mode); //записываем в память режим колбы
        eeprom_update_byte((uint8_t*)12, _bright_mode); //записываем в память режим подсветки
        eeprom_update_byte((uint8_t*)13, _bright_levle); //записываем в память уровень подсветки

        switch (_bright_mode) {
          case 0: indiSetBright(brightDefault[_bright_levle]); break; //установка яркости индикаторов
          case 1: indiSetBright(brightDefault[indiBright[readLightSens()]]); break; //установка яркости индикаторов
          case 2: indiSetBright(brightDefault[indiBright[changeBright()]]); break; //установка яркости индикаторов
        }

        dot_state = 0; //выключаем точку
        indiClr(); //очистка индикаторов
        indiPrint("OUT", 0);
        for (timer_millis = 1000; timer_millis && !check_keys();) data_convert(); // ждем, преобразование данных
        disableSleep = 0; //разрешаем сон
        _mode = 0; //переходим в режим часов
        scr = 0; //обновляем экран
        return;
    }
  }
}
//----------------------------------------------------------------------------------
boolean changeBright(void) {
  // установка яркости всех светилок от времени суток
  if ((timeBright[0] > timeBright[1] && (time[4] >= timeBright[0] || time[4] < timeBright[1])) ||
      (timeBright[0] < timeBright[1] && time[4] >= timeBright[0] && time[4] < timeBright[1])) {
    return 0;
  } else {
    return 1;
  }
}
//-----------------------------Главный экран------------------------------------------------
void main_screen(void) //главный экран
{
  if (!scr) {
    scr = 1; //сбрасываем флаг
    switch (_mode) {
      case 0:
        indiPrintNum(time[4], 0, 2, '0'); //вывод часов
        indiPrintNum(time[5], 2, 2, '0'); //вывод минут
        break;
      case 1:
        indiPrint("B", 0);
        indiPrintNum(bat, 1, 3, ' '); //вывод заряда акб
        dot_state = 0; //выключаем точки
        break;
      case 2:
        if (!_timer_mode) {
          indiPrint("T", 0);
          indiPrintNum(timerDefault[_timer_preset], 2, 2, '0'); //вывод времени таймера
        }
        else {
          indiPrintNum(_timer_secs / 60, 0, 2, '0'); //вывод минут
          indiPrintNum(_timer_secs % 60, 2, 2, '0'); //вывод секунд
        }
        switch (_timer_mode) {
          case 0: dot_state = 0; break; //включаем точки
          case 1: dot_state = 1; break; //выключаем точки
        }
        break;
      case 3:
        indiPrintNum(time[1], 0, 2, '0'); //вывод месяца
        indiPrintNum(time[2], 2, 2, '0'); //вывод даты
        dot_state = 1; //включаем точки
        break;
    }
  }

  if (!_sleep && (!_mode || _timer_mode == 2) && !timer_dot) {
    dot_state = !dot_state; //инвертируем точки
    timer_dot = 500;
  }

  switch (check_keys()) {
    case 1: //left key press
      switch (_timer_mode) {
        case 0: if (_mode < 2) _mode++; else _mode = 0; break;
        case 1:
          if (_timer_preset > 0) _timer_preset--; else _timer_preset = 6;
          _timer_secs = timerDefault[_timer_preset] * 60;
          break;
        case 2: _timer_mode = 1; break;
      }
      scr = 0;
      break;

    case 2: //right key press
      switch (_timer_mode) {
        case 0: _mode = (!_mode) ? 3 : 0; break;
        case 1:
          if (_timer_preset < 6) _timer_preset++; else _timer_preset = 0;
          _timer_secs = timerDefault[_timer_preset] * 60;
          break;
        case 2: _timer_mode = 1; break;
      }
      scr = 0;
      break;

    case 3: //left key hold
      if (_mode != 2) settings_time(); //настройки времени
      else {
        if (_timer_mode == 2) _timer_secs = timerDefault[_timer_preset] * 60;
        _timer_mode = (_timer_mode != 2) ? 2 : 0;
      }
      scr = 0;
      break;

    case 4: //right key hold
      if (_mode != 2) settings_bright(); //настройки яркости
      else _timer_mode = (!_timer_mode) ? 1 : 0;
      scr = 0;
      break;
  }
}
