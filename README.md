# План работы:

1. **Изучение структуры ETL**  
   - Формат данных и особенности хранения в Windows.

2. **Реализация чтения данных из ETL-файлов**  
   - Обработка событий (парсинг, валидация, преобразование).

3. **Оптимизация структуры для вывода**  
   - Поддержка удобных форматов (CSV или JSON).

4. **Добавление выбора формата через CLI**  
   - Реализация аргументов командной строки для указания формата вывода.

---

**Таблица описания структуры ETL файла**

| **Колонка**             | **Тип данных**              | **Описание**                                                                 |
|-------------------------|-----------------------------|----------------------------------------------------------------------------- |
| **Event Name**          | `std::string`               | Имя события (обычно `<ProviderName>`)                                        |
| **Type**                | `std::string` **или** `uint32_t` | Тип события/канала (строка или числовой идентификатор)                       |
| **Event ID**            | `uint32_t`                  | Числовой идентификатор события                                               |
| **Version**             | `uint8_t`                   | Версия определения события                                                   |
| **Channel**             | `uint8_t`                   | Канал: 0 = None, 1 = Application, 2 = System, 16 = Security и т.д.           |
| **Level**               | `uint8_t`                   | Уровень: 0 = Log Always, 1 = Critical, 2 = Error, 3 = Warning и т.д.         |
| **Opcode**              | `uint8_t`                   | Код операции внутри события (обычно 0)                                       |
| **Task**                | `uint16_t`                  | Идентификатор задачи (обычно 0)                                              |
| **Keyword**             | `uint64_t`                  | Битовая маска ключевых слов (представлена в hex)                             |
| **PID**                 | `uint32_t`                  | Process ID — ID процесса, записавшего событие                                |
| **TID**                 | `uint32_t`                  | Thread ID — ID потока                                                        |
| **Processor Number**    | `uint32_t`                  | Логический номер процессора                                                  |
| **Instance ID**         | `uint32_t`                  | Идентификатор экземпляра события при множественных сессиях                   |
| **Parent Instance ID**  | `uint32_t`                  | Идентификатор родительского экземпляра (если есть)                           |
| **Activity ID**         | `std::array<uint8_t, 16>`                      | GUID активности, позволяет коррелировать цепочку событий                     |
| **Related Activity ID** | `std::array<uint8_t, 16>`                      | Связанный Activity ID (если есть)                                            |
| **Clock-Time**          | `uint64_t`                  | Метка времени в формате Windows FILETIME (100-нс тики от 1 января 1601)      |
| **Kernel(ms)**          | `uint64_t`                  | Время в ядре до момента события (при включённой трассировке ядра)            |
| **User(ms)**            | `uint64_t`                  | Время в режиме пользователя                                                  |
| **User Data**           | `std::vector<uint8_t>`      | Полезная нагрузка события: текст (если читаемо) или HEX-строка               |

## Библиотеки

| Библиотека           | Назначение                                                                                     |
|-----------------------|-----------------------------------------------------------------------------------------------|
| `<windows.h>`         | Работа с Windows API (функции времени, GUID, обработка файлов).                               |
| `<evntrace.h>`        | Event Tracing for Windows (ETW) — сбор и обработка событий.                                   |
| `<fstream>`           | Работа с файлами (чтение/запись).                                                             |
| `<vector>`            | Хранение данных в динамических массивах.                                                      |
| `<iostream>`          | Ввод/вывод в консоль.                                                                         |
| `<sstream>`           | Форматирование строковых потоков.                                                             |
| `<iomanip>`           | Управление выводом (например, форматирование даты).                                           |
| `<tdh.h>`             | Trace Data Helper (TDH) — разбор данных событий.                                               |
| `advapi32.lib`        | Для работы с ETW (линковка).                                                                  |
| `tdh.lib`             | Для функций TDH (линковка).                                                                   |
| `ole32.lib`           | Работа с GUID (линковка).                                                                     |

---

## Структура данных в коде

### `EventData`
Хранит информацию о событии из ETL-файла:
- `EventID` — идентификатор события.
- `ProviderID` — GUID провайдера.
- `ProcessID`, `ThreadID` — идентификаторы процесса и потока.
- `Timestamp` — метка времени в формате `ULONGLONG`.
- `KernelTime`, `UserTime` — время выполнения в ядре и пользовательском режиме.
- `EventVersion`, `EventOpcode`, `EventLevel`, `EventKeyword` — параметры события.
- `TimeLocal` — время в формате строки (`YYYY-MM-DD HH:MM:SS.MMM`).

---

Использование `ULONG`, `UCHAR` и `GUID` вместо стандартных типов C++ обусловлено:  
- Совместимостью с Windows API (ETW, обработка событий);  
- Точным соответствием системным структурам данных (например, `EVENT_RECORD`);  
- Упрощением кода за счёт исключения преобразований и снижения риска ошибок.  

## Функции

### Вспомогательные функции
1. **`EscapeForCSV`**  
   Экранирует спецсимволы (кавычки, запятые) для корректного форматирования CSV.

2. **`ConvertFileTimeToLocalTime`**  
   Конвертирует `FILETIME` в локализованную строку времени.

3. **`WideToUTF8`**  
   Преобразует строку `wstring` в UTF-8.

4. **`FindEtlFiles`**  
   Ищет все файлы с расширением `.etl` в указанной директории.

5. **`ProcessSystemDirectory`**  
   Обрабатывает все ETL-файлы в директории и возвращает данные в заданном формате.

6. **`ConvertUtf8ToWide`**  
   Конвертирует строку UTF-8 в `wstring`.

---

## Классы

### `ETLParser`
Парсит ETL-файлы и сохраняет события в формате `EventData`.

#### Методы:
1. **`Parse`**  
   Открывает ETL-файл и обрабатывает события через `ProcessTrace`.  
   Использует `EventCallback` для передачи данных в `HandleEvent`.

2. **Функция `ToCSV`**
**Назначение**:  
Преобразует данные о событиях из вектора `events_` в строку формата CSV

**Шаги работы**:
1. **Инициализация строкового потока**:  
   Создаётся `std::stringstream` для формирования итоговой строки.
   ```cpp
   std::stringstream ss;
   ```

2. **Добавление заголовков CSV**:  
   Записывается строка с названиями колонок:
   ```cpp
   ss << "EventID,ProviderID,ProcessID,ThreadID,Timestamp,KernelTime,UserTime,EventVersion,EventOpcode,EventLevel,EventKeyword,TimeLocal\n";
   ```

3. **Обработка каждого события**:  
   Для каждого элемента `EventData` в векторе `events_`:
   - **Преобразование GUID в строку**:  
     Используется `StringFromGUID2` для конвертации `GUID` провайдера в строку вида `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`.
     ```cpp
     wchar_t guidStr[40];
     StringFromGUID2(event.ProviderID, guidStr, 40);
     std::string providerUTF8 = WideToUTF8(guidStr);
     ```
   - **Экранирование данных**:  
     Для полей `providerUTF8` и `timeLocalUTF8` применяется функция `EscapeForCSV`, чтобы:
     - Заменить двойные кавычки `"` на `""`.
     - Обрамить строку кавычками, если она содержит запятые, кавычки или переносы строк.
     ```cpp
     std::string providerEscaped = EscapeForCSV(providerUTF8);
     std::string timeLocalEscaped = EscapeForCSV(timeLocalUTF8);
     ```

4. **Формирование строки события**:  
   Данные записываются в поток в формате CSV. Числовые значения выводятся напрямую, строки — с экранированием.
   ```cpp
   ss << event.EventID << ","
      << providerEscaped << ","
      << event.ProcessID << ","
      << event.ThreadID << ","
      << event.Timestamp << ","
      << event.KernelTime << ","
      << event.UserTime << ","
      << static_cast<int>(event.EventVersion) << ","
      << static_cast<int>(event.EventOpcode) << ","
      << static_cast<int>(event.EventLevel) << ","
      << event.EventKeyword << ","
      << "\"" << timeLocalEscaped << "\"\n";
   ```

**Итог**:  
Возвращается строка вида:
```csv
EventID,ProviderID,...,TimeLocal
123,"{A0B1C2D3...}",...,"2023-10-01 12:34:56.789"
```

---

3. **Функция `ToJSON`**
**Назначение**:  
Преобразует данные о событиях в строку формата JSON (JavaScript Object Notation).

**Шаги работы**:
1. **Инициализация строкового потока**:  
   Создаётся `std::stringstream`, и записывается открывающая скобка массива:
   ```cpp
   std::stringstream ss;
   ss << "[\n";
   ```

2. **Обработка каждого события**:  
   Для каждого элемента `EventData` в векторе `events_`:
   - **Формирование объекта JSON**:  
     - Поля `EventID`, `ProcessID`, `ThreadID` и другие числовые значения выводятся напрямую.
     - `GUID` и `TimeLocal` конвертируются в UTF-8 и экранируются.
     ```cpp
     ss << "  {\n"
        << "    \"EventID\": " << event.EventID << ",\n"
        << "    \"ProviderID\": \"" << providerUTF8 << "\",\n"
        << "    \"ProcessID\": " << event.ProcessID << ",\n"
        << "    \"ThreadID\": " << event.ThreadID << ",\n"
        << "    \"Timestamp\": " << event.Timestamp << ",\n"
        << "    \"KernelTime\": " << event.KernelTime << ",\n"
        << "    \"UserTime\": " << event.UserTime << ",\n"
        << "    \"EventVersion\": " << static_cast<int>(event.EventVersion) << ",\n"
        << "    \"EventOpcode\": " << static_cast<int>(event.EventOpcode) << ",\n"
        << "    \"EventLevel\": " << static_cast<int>(event.EventLevel) << ",\n"
        << "    \"EventKeyword\": " << event.EventKeyword << ",\n"
        << "    \"TimeLocal\": \"" << timeLocalUTF8 << "\"\n"
        << "  }";
     ```

3. **Управление запятыми**:  
   Для всех элементов, кроме последнего, после закрывающей скобки `}` добавляется запятая и перенос строки:
   ```cpp
   if (!first) {
       ss << ",\n";
   }
   first = false;
   ```

4. **Завершение JSON**:  
   После обработки всех событий добавляется закрывающая скобка массива:
   ```cpp
   ss << "\n]";
   ```

**Итог**:  
Возвращается строка вида:
```json
[
  {
    "EventID": 123,
    "ProviderID": "{A0B1C2D3...}",
    "ProcessID": 456,
    ...
  }
]
```
---

## Компиляция

### Требования
- Компилятор: MSVC (из-за ETW и WinAPI).
- Операционная система: Windows (из-за ETW и WinAPI).

### Команда компиляции
```bash
cl /EHsc /W4 /DUNICODE /D_UNICODE main.cpp advapi32.lib tdh.lib ole32.lib
```

#### Флаги:
- `/EHsc` — включение обработки исключений.
- `/W4` — уровень предупреждений.
- `/DUNICODE`, `/D_UNICODE` — поддержка Unicode.
- Линковка с `advapi32.lib`, `tdh.lib`, `ole32.lib`.

---

## Запуск

### Команды для CSV/JSON
```bash
./main.exe "X:\" csv > output.csv
./main.exe "X:\" json > output.json
```

### Параметры:
1. Путь к директории — где находятся ETL-файлы.
2. Формат — `csv` или `json`.
3. Результат перенаправляется в файл через `>`.

---

## Ссылки на документацию
1. [Event Tracing for Windows (ETW)](https://docs.microsoft.com/en-us/windows/win32/etw/event-tracing-portal)
2. [Типы данных Windows](https://learn.microsoft.com/ru-ru/windows/win32/winprog/windows-data-types)
3. [Trace Data Helper (TDH)](https://docs.microsoft.com/en-us/windows/win32/api/tdh/)
4. [Работа с GUID](https://docs.microsoft.com/en-us/windows/win32/api/guiddef/)
5. [GUID в Windows API](https://learn.microsoft.com/ru-ru/windows/win32/api/guiddef/ns-guiddef-guid)
6. [Функции времени Windows](https://docs.microsoft.com/en-us/windows/win32/sysinfo/time-functions)
7. [Заголовок evntrace](https://learn.microsoft.com/ru-ru/windows/win32/api/evntrace/)
