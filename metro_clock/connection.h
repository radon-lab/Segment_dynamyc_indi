//Соединения периферии с портами МК 
//    PORTD (2 - D2 | 3 - D3 | 4 - D4 | 5 - D5 | 6 - D6 | 7 - D7) PIND
//  PORTB (0 - D8 | 1 - D9 | 2 - D10 | 3 - D11 | 4 - D12 | 5 - D13) PINB
//    PORTC (0 - A0 | 1 - A1 | 2 - A2 | 3 - A3 | 4 - A4 | 5 - A5) PINC

#define CATHODE_1 16
#define CATHODE_2 13
#define CATHODE_3 12
#define CATHODE_4 11
 
#define ANODE_A 14
#define ANODE_B 8
#define ANODE_C 7
#define ANODE_D 6
#define ANODE_E 4
#define ANODE_F 10
#define ANODE_G 3

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

//пин основного питания RTC A3
#define RTC_BIT   3 // A3
#define RTC_PORT  PORTC

#define RTC_ON    (bitSet(RTC_PORT, RTC_BIT))
#define RTC_OFF   (bitClear(RTC_PORT, RTC_BIT))
#define RTC_OUT   (bitSet((DDR_REG(RTC_PORT)), RTC_BIT))

#define RTC_INIT  RTC_ON; RTC_OUT

//пин дополнительного питания RTC A1
#define RTC_BAT_BIT   1 // A1
#define RTC_BAT_PORT  PORTC

#define RTC_BAT_ON    (bitSet(RTC_BAT_PORT, RTC_BAT_BIT))
#define RTC_BAT_OFF   (bitClear(RTC_BAT_PORT, RTC_BAT_BIT))
#define RTC_BAT_OUT   (bitSet((DDR_REG(RTC_BAT_PORT)), RTC_BAT_BIT))

#define RTC_BAT_INIT  RTC_BAT_OFF; RTC_BAT_OUT
