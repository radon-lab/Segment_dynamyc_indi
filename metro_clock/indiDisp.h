#include <Arduino.h>
#include "font.c"

const byte anodeMask[] = {ANODE_A, ANODE_B, ANODE_C, ANODE_D, ANODE_E, ANODE_F, ANODE_G}; //порядок и номера пинов анодов индикатора(a, b, c, d, e, f, g)
const byte cathodeMask[] = {CATHODE_1, CATHODE_2, CATHODE_3, CATHODE_4}; //порядок и номера пинов катодов индикатора(0, 1, 2, 3)

uint8_t indi_buf[4];
uint8_t indi_dimm[4];
boolean dot_state;
volatile uint8_t indi_state;

#define LEFT 0
#define RIGHT 255
#define CENTER 254

#define fontbyte(x) pgm_read_byte(&indiFont[x])

#define _INDI_ON  PRR &= ~(1 << 6); TIMSK2 = 0b00000011
#define _INDI_OFF TCNT2 = TIMSK2 = 0b00000000; PRR |= (1 << 6)

void indiInit(void);
void indiEnableSleep(void);
void indiDisableSleep(uint8_t pwm = 255);
void indiSetBright(uint8_t indi, uint8_t pwm);
void indiSetBright(uint8_t pwm);
void indiClr(void);
void indiClr(uint8_t indi);
void indiSet(uint8_t st, uint8_t indi, boolean state = 1);
void indiPrint(const char *st, uint8_t indi);
void indiPrintNum(int16_t num, uint8_t indi, uint8_t length = 0, char filler = ' ');

inline void setPin(uint8_t pin, boolean x) {
  if (pin < 8) bitWrite(PORTD, pin, x);
  else if (pin < 14) bitWrite(PORTB, (pin - 8), x);
  else if (pin < 20) bitWrite(PORTC, (pin - 14), x);
  else return;
}
void outPin(uint8_t pin) {
  if (pin < 8) bitSet(DDRD, pin);
  else if (pin < 14) bitSet(DDRB, (pin - 8));
  else if (pin < 20) bitSet(DDRC, (pin - 14));
  else return;
}

//---------------------------------Генерация символов---------------------------------------
ISR(TIMER2_OVF_vect) //генерация символов
{
  uint8_t data = indi_buf[indi_state];

  for (uint8_t c = 0; c < 7; c++)
  {
    setPin(anodeMask[c], data & 0x80);
    data = data << 1;
  }
  OCR2A = indi_dimm[indi_state];
  setPin(cathodeMask[indi_state], 0);
  if (dot_state) DOT_ON; //включаем точки
}
ISR(TIMER2_COMPA_vect) {
  DOT_OFF; //выключаем точки
  setPin(cathodeMask[indi_state], 1);
  if (++indi_state > 3) indi_state = 0;
}
//-------------------------Инициализация индикаторов----------------------------------------------------
void indiInit(void) //инициализация индикаторов
{
  for (uint8_t i = 0; i < 7; i++) {
    setPin(anodeMask[i], 0);
    outPin(anodeMask[i]);
  }

  for (uint8_t i = 0; i < 4; i++) {
    setPin(cathodeMask[i], 1);
    outPin(cathodeMask[i]);
    indi_buf[i] = 0;
    indi_dimm[i] = 255;
  }

  OCR2A = indi_dimm[indi_state];

  TCCR2A = 0b00000000; //отключаем OC2A/OC2B
#if F_CPU == 16000000UL
  TCCR2B = 0b00000100; //пределитель 64
#elif F_CPU == 8000000UL
  TCCR2B = 0b00000011; //пределитель 32
#endif
  TIMSK2 = 0b00000000; //отключаем прерывания Таймера2

  sei(); //разрешаем прерывания глобально

  _INDI_ON; //запускаем генерацию
}
//---------------------------------Включение режима сна---------------------------------------
void indiEnableSleep(void) //включение режима сна
{
  _INDI_OFF; //отключаем генирацию
  for (uint8_t i = 0; i < 4; i++) indi_buf[i] = 0; //очищаем буфер
  for (uint8_t i = 0; i < 7; i++) setPin(anodeMask[i], 0); //сбрасываем пины
  for (uint8_t i = 0; i < 4; i++) setPin(cathodeMask[i], 1); //сбрасываем пины
}
//---------------------------------Выключение режима сна---------------------------------------
void indiDisableSleep(uint8_t pwm) //выключение режима сна
{
  if (pwm < 30) pwm = 30;
  for (uint8_t i = 0; i < 4; i++) indi_dimm[i] = pwm; //устанавливаем максимальную яркость
  _INDI_ON; //запускаем генерацию
}
//---------------------------------Установка яркости индикатора---------------------------------------
void indiSetBright(uint8_t indi, uint8_t pwm) //установка яркости индикатора
{
  if (pwm < 30) pwm = 30;
  indi_dimm[indi] = pwm;
}
//---------------------------------Установка яркости индикаторов---------------------------------------
void indiSetBright(uint8_t pwm) //установка яркости индикаторов
{
  if (pwm < 30) pwm = 30;
  for (byte i = 0; i < 4; i++) {
    indi_dimm[i] = pwm;
  }
}
//-------------------------Очистка индикаторов----------------------------------------------------
void indiClr(void) //очистка индикаторов
{
  for (uint8_t i = 0; i < 4; i++) indi_buf[i] = 0;
}
//-------------------------Очистка индикатора----------------------------------------------------
void indiClr(uint8_t indi) //очистка индикатора
{
  indi_buf[indi] = 0;
}
//-------------------------Вывод символов----------------------------------------------------
void indiSet(uint8_t st, uint8_t indi, boolean state) //вывод символов
{
  bitWrite(indi_buf[indi], 7 - st, state);
}
//-------------------------Вывод текста----------------------------------------------------
void indiPrint(const char *st, uint8_t indi) //вывод текста
{
  uint8_t stl = strlen(st);

  switch (indi) {
    case RIGHT: indi = 4 - stl; break;
    case CENTER: indi = 4 - stl / 2; break;
  }

  for (int cnt = 0; cnt < stl; cnt++)
    indi_buf[indi++] = fontbyte(st[cnt] - 32);
}
//-------------------------Вывод чисел----------------------------------------------------
void indiPrintNum(int16_t num, uint8_t indi, uint8_t length, char filler) //вывод чисел
{
  char buf[4];
  char st[4];
  boolean neg = false;
  int8_t c = 0, f = 0;

  if (num == 0)
  {
    if (length != 0)
    {
      for (c = 0; c < (length - 1); c++)
        st[c] = filler;
      st[c] = 48;
      st[c + 1] = 0;
    }
    else
    {
      st[0] = 48;
      st[1] = 0;
    }
  }
  else
  {
    if (num < 0)
    {
      neg = true;
      num = -num;
    }

    while (num > 0)
    {
      buf[c] = 48 + (num % 10);
      c++;
      num = (num - (num % 10)) / 10;
    }
    buf[c] = 0;

    if (neg)
    {
      st[0] = 45;
    }

    if (length > (c + neg))
    {
      for (int i = 0; i < (length - c - neg); i++)
      {
        st[i + neg] = filler;
        f++;
      }
    }

    for (int i = 0; i < c; i++)
    {
      st[i + neg + f] = buf[c - i - 1];
    }
    st[c + neg + f] = 0;

  }

  indiPrint(st, indi);
}
