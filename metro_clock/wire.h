uint8_t _requested_bytes = 0;              // переменная хранит количество запрошенных и непрочитанных байт
bool _address_nack = false;         // Флаг для отслеживания ошибки при передаче адреса
bool _data_nack = false;          // Флаг для отслеживания ошибки при передаче данных
bool _stop_after_request = true;          // stop или restart после чтения последнего байта

void WireBegin(void);                    // инициализация шины
void WireSetClock(uint32_t clock);          // ручная установка частоты шины 31-900 kHz (в герцах)
void WireBeginTransmission(uint8_t address);  // открыть соединение (для записи данных)
uint8_t WireEndTransmission(bool stop);     // закрыть соединение , произвести stop или restart (по умолчанию - stop)
uint8_t WireEndTransmission(void);        // закрыть соединение , произвести stop
void WireWrite(uint8_t data);                 // отправить в шину байт данных , отправка производится сразу , формат - byte "unsigned char"
void WireRequestFrom(uint8_t address , uint8_t length , bool stop); //открыть соединение и запросить данные от устройства, отпустить или удержать шину
void WireRequestFrom(uint8_t address , uint8_t length);       //открыть соединение и запросить данные от устройства, отпустить шину
uint8_t WireRead(void);                       // прочитать байт , БУФЕРА НЕТ!!! , читайте сразу все запрошенные байты , stop или restart после чтения последнего байта, настраивается в requestFrom
uint8_t WireAvailable(void);                  // вернет количество оставшихся для чтения байт
void WireStart(void);                         // сервисная функция с нее начинается любая работа с шиной
void WireStop(void);                          // сервисная функция ей заканчивается работа с шиной


void WireBegin()
{ // Инициализация шины в роли master
  pinMode(SDA, INPUT_PULLUP);           // Подтяжка шины
  pinMode(SCL, INPUT_PULLUP);           // Подтяжка шины
  TWBR = 72;                    // Стандартная скорость - 100kHz
  TWSR = 0;                   // Делитель - /1 , статус - 0;
}

void WireSetClock(uint32_t clock)
{ // Функция установки частоты шины 31-900 kHz (в герцах)
  TWBR = (((long)F_CPU / clock) - 16) / 2;    // Расчет baudrate - регистра
}

void WireBeginTransmission(uint8_t address)
{ // Начать передачу (для записи данных)
  WireStart();                           // Старт
  WireWrite(address << 1);               // Отправка slave - устройству адреса с битом "write"
}

uint8_t WireEndTransmission(void)
{ // Завершить передачу и отпустить шину
  return WireEndTransmission(true);
}

uint8_t WireEndTransmission(bool stop)
{ // Завершить передачу (после записи данных)
  if (stop) WireStop();                      // Если задано stop или аргумент пуст - отпустить шину
  else WireStart();                          // Иначе - restart (другой master на шине не сможет влезть между сообщениями)
  if (_address_nack) {                      // Если нет ответа при передаче адреса
    _address_nack = false;                    // Обнуляем оба флага
    _data_nack = false;                       // Обнуляем оба флага
    return 2;                             // Возвращаем '2'
  } if (_data_nack) {                       // Если нет ответа при передаче данных
    _address_nack = false;                    // Обнуляем оба флага
    _data_nack = false;                     // Обнуляем оба флага
    return 3;                             // Возвращаем '2'
  } return 0;                   // Если все ОК - возвращаем '0'
}

void WireWrite(uint8_t data)
{ // Прямая отправка байта на шину
  TWDR = data;                  // Записать данные в data - регистр
  TWCR = _BV(TWEN) | _BV(TWINT);            // Запустить передачу
  while (!(TWCR & _BV(TWINT)));         // Дождаться окончания
  uint8_t _bus_status = TWSR & 0xF8;        // Чтение статуса шины
  if (_bus_status == 0x20) _address_nack = true; // SLA + W + NACK ? - нет ответа при передаче адреса
  if (_bus_status == 0x30) _data_nack = true;   // BYTE + NACK ? - нет ответа при передаче данных
}

uint8_t WireAvailable()
{ // Вернуть оставшееся количество запрошенных для чтения байт
  return _requested_bytes;            // Это содержимое этой переменной
}

uint8_t WireRead()
{ // Прямое чтение байта из шины после запроса
  if (--_requested_bytes) {             // Если байт не последний
    TWCR = _BV(TWEN) | _BV(TWINT) | _BV(TWEA);  // Запустить чтение шины (с подтверждением "ACK")
    while (!(TWCR & _BV(TWINT)));       // Дождаться окончания приема данных
    return TWDR;                // Вернуть принятые данные , это содержимое data - регистра
  }
  _requested_bytes = 0;               // Если читаем последний байт
  TWCR = _BV(TWEN) | _BV(TWINT);          // Запустить чтение шины (БЕЗ подтверждения "NACK")
  while (!(TWCR & _BV(TWINT)));         // Дождаться окончания приема данных
  if (_stop_after_request) WireStop();     // Если в requestFrom не задан аргумент stop , или stop задан как true - отпустить шину
  else WireStart();              // Иначе - restart (другой master на шине не сможет влезть между сообщениями)
  return TWDR;                  // Вернуть принятый ранее байт из data - регистра
}

void WireRequestFrom(uint8_t address , uint8_t length)
{ // Запрос n-го кол-ва байт от ведомого устройства и отпускание шины
  WireRequestFrom(address , length , true);
}

void WireRequestFrom(uint8_t address , uint8_t length , bool stop)
{ // Запрос n-го кол-ва байт от ведомого устройства (Читайте все байты сразу!!!)
  _stop_after_request = stop;           // stop или restart после чтения последнего байта
  _requested_bytes = length;            // Записать в переменную количество запрошенных байт
  WireStart();               // Начать работу на шине
  WireWrite((address << 1) | 0x1);     // Отправить устройству адрес + бит "read"
}

void WireStart()
{ // сервисная функция с нее начинается любая работа с шиной
  TWCR = _BV(TWSTA) | _BV(TWEN) | _BV(TWINT);   // start + TwoWire enable + установка флага "выполнить задачу"
  while (!(TWCR & _BV(TWINT)));         // Ожидание завершения
}

void WireStop()
{ // сервисная функция ей заканчивается работа с шиной
  TWCR = _BV(TWSTO) | _BV(TWEN) | _BV(TWINT);   // stop + TwoWire enable + установка флага "выполнить задачу"
}
