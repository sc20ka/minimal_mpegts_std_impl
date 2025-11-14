# MPEG-TS Demuxer - Полная Техническая Спецификация

**Версия:** 1.0  
**Язык:** C++17/20  
**Стандарт:** ISO/IEC 13818-1 (MPEG-TS)  
**Фокус:** H.264/H.265 с поддержкой приватных данных

---

## Содержание

1. [Обзор проекта](#обзор-проекта)
2. [Архитектура буферизации](#архитектура-буферизации)
3. [Алгоритм синхронизации](#алгоритм-синхронизации)
4. [Структура данных и хранилище](#структура-данных-и-хранилище)
5. [Система управления PID и программами](#система-управления-pid-и-программами)
6. [Обработка payload'ов](#обработка-payloadов)
7. [API и интерфейсы](#api-и-интерфейсы)
8. [Режимы работы](#режимы-работы)
9. [Примеры использования](#примеры-использования)
10. [Детали реализации](#детали-реализации)

---

## Обзор проекта

### Назначение

Создание высокопроизводительного адаптивно-восстановительного MPEG-TS демультиплексора для:

- **Потоковой обработки** MPEG-TS данных с высокой надёжностью
- **Адаптивной синхронизации** в условиях мусора и помех
- **Восстановления** валидных пакетов из произвольных данных
- **Разделения** основной полезной нагрузки и приватных данных
- **Управления** несколькими программами и потоками одновременно

### Ключевые характеристики

| Характеристика            | Значение                                       |
| ------------------------- | ---------------------------------------------- |
| Размер MPEG-TS пакета     | 188 байт (стандартный, без prefix)             |
| Максимум пакетов в буфере | 100 (18.8 КБ)                                  |
| Валидация синхронизации   | 3-итеративная (trinary validation)             |
| Поддержка потоков         | Динамическое обнаружение, до 63 программ       |
| Устойчивость к мусору     | Адаптивный поиск с фильтрацией                 |
| Поддержка видеокодеков    | H.264, H.265 (любые в MPEG-TS)                 |
| Приватные данные          | Полная поддержка (transport_private_data_flag) |
| Scrambled контент         | **НЕ поддерживается**                          |
| Профили                   | Без привязки к DVB/ATSC/ISDB                   |

---

## Архитектура буферизации

### 1.1 Адаптивно-восстановительная буферизация

#### Проблема

MPEG-TS поток может содержать:

- **Мусор (garbage)** - произвольные байты, которые могут случайно содержать 0x47 (sync byte)
- **Валидные пакеты** - 188-байтовые структурированные данные
- **Разрывы** - потери данных, нарушения continuity counter

Необходимо **стабильно накапливать, находить и отсеивать** валидные пакеты несмотря на помехи.

#### Решение: Трёхитеративная валидация

**Концепция:**

```
Буфер: [n байт мусора][b₁ валидный пакет][a байт мусора][b₂ валидный пакет][g мусор][b₃ валидный пакет]

Поиск:
  1. Сканируем буфер на наличие 0x47 (sync byte)
  2. Для каждого найденного потенциального пакета:
     - Извлекаем характеристики (PID, CC, flags)
     - Добавляем в candidates[]
  3. Если найдено 3+ кандидата с ОДИНАКОВОЙ итерацией → СИНХРОНИЗАЦИЯ НАЙДЕНА
  4. Если найдено 3+ кандидата с РАЗНЫМИ итерациями → Несколько программ/потоков
```

**Варианты комбинаций:**

```
Вариант 1: Валидный набор (ПРИНИМАЕМ)
  [мусор][pkt₁][мусор][pkt₂][мусор][pkt₃]
  
  Все три пакета:
    ✓ Имеют одинаковый PID (или неизвестен из-за payload_length=0)
    ✓ Имеют последовательный continuity counter
    ✓ Имеют согласованные adaptation field флаги
  
  РЕЗУЛЬТАТ: ✅ ВАЛИДНАЯ ИТЕРАЦИЯ

─────────────────────────────────────────────

Вариант 2: Разные итерации (РАЗДЕЛЯЕМ)
  [pkt₁: PID=0x100, CC=5]
  [пакет с разными параметрами: PID=0x101, CC=2]
  [pkt₃: PID=0x100, CC=6]
  
  Вывод:
    ✓ Группа 1: pkt₁, pkt₃ - может быть часть валидной итерации
    ✓ Группа 2: средний пакет - начало другой итерации
  
  РЕЗУЛЬТАТ: ✅ ДВА НАБОРА (с флагом discontinuity на втором)

─────────────────────────────────────────────

Вариант 3: Недостаточно данных (ОТБРАСЫВАЕМ)
  [pkt₁: CC=5]
  [пакет с CC=10, но нет пакетов 6,7,8,9]
  
  Вывод: ❌ Только 1-2 пакета не могут быть валидной итерацией
```

#### Размер буфера

```
Максимум: 100 MPEG-TS пакетов
         = 100 × 188 байт
         = 18,800 байт
         ≈ 18.8 КБ

Структура:
  circular_buffer<uint8_t> raw_buffer;  // SIZE = 18800 байт

Поведение при переполнении:
  - После синхронизации старые данные очищаются по мере обработки
  - Пользователь должен регулярно вызывать feedData() и clearIteration()
  - Если буфер переполнится → необработанные данные теряются
```

### 1.2 Захват полезных нагрузок каналов

#### Извлечение данных

```
Для каждого найденного валидного пакета:

  1. Идентификация потока
     └─ PID: 0x0000...0x1FFF (13 бит)

  2. Основная полезная нагрузка
     └─ Данные после заголовка и adaptation field
     └─ Маркируется как PAYLOAD_NORMAL

  3. Приватные данные (если присутствуют)
     └─ Извлекаются из adaptation field
     └─ Только если transport_private_data_flag = 1
     └─ Маркируются как PAYLOAD_PRIVATE

  4. Аккумулирование по ID программы/потока
     └─ Группируются в один буфер для каждого (program, stream)
```

#### Структура накопления

```cpp
// Пример накопления для потока PID=0x100

IterationData {
    payloads: [
        {type: PAYLOAD_PRIVATE, data: [...], len: 16 bytes},
        {type: PAYLOAD_NORMAL,  data: [...], len: 1172 bytes},
    ],
    first_cc: 5,
    last_cc: 7,
    packet_count: 3,
    discontinuity_detected: false,
};

// Потом эта итерация добавляется в вектор:
// streams[0x100].iterations.push_back({iterID: 42, data: IterationData});
```

### 1.3 Полная обработка MPEG-TS

#### Что обрабатываем

- ✅ **Основные потоки:** H.264, H.265, AAC, AC3 (все MPEG-TS типы)
- ✅ **Приватные данные:** Из adaptation field через transport_private_data_flag
- ✅ **Метаметадаты:** Continuity counter, adaptation field, flags
- ✅ **Программы:** Несколько независимых потоков одновременно
- ✅ **Разрывы:** Discontinuity indicators

#### Что **НЕ** обрабатываем

- ❌ **Scrambled контент:** Нет расшифровки
- ❌ **PCR/PTS/DTS:** Только парсируем, не используем для синхронизации
- ❌ **Специфичные профили:** DVB, ATSC, ISDB обработка
- ❌ **Таблицы PSI:** PAT/PMT парсируем только если предоставлены явно

### 1.4 Видеокодеки и данные

#### Основные потоки

```
Тип потока                   Обработка
──────────────────────────────────────────
H.264 (AVC)                 ✅ Полная поддержка
H.265 (HEVC)                ✅ Полная поддержка
AAC-LC, AAC-HE              ✅ Полная поддержка
AC3, E-AC3                  ✅ Полная поддержка
Другие MPEG-TS типы         ✅ Raw extraction
```

#### Приватные полезные данные

```
Источник: Adaptation Field → transport_private_data_flag

Структура:
  [adaptation_field_length: 1 байт]
  [flags: 1 байт]           ← bit 1: transport_private_data_flag
  [optional PCR: 6 байт]
  [optional OPCR: 6 байт]
  [optional splice: 1 байт]
  [private_data_length: 1 байт]  ← ВОТ ОТСЮДА
  [private_data[]: N байт]        ← ДО КОНЦА

Могут быть фрагментированы:
  - Пакет 1: [начало приватных данных]
  - Пакет 2: [продолжение приватных данных]
  - Пакет 3: [конец приватных данных]

Хранятся ОТДЕЛЬНО от основного payload!
```

### 1.5 Без фокуса на профилях

```
Подход: профайл-агностичный (profile-agnostic)

Особенность:
  - Работаем с RAW MPEG-TS пакетами
  - Не интерпретируем специфичный контент DVB/ATSC
  - Просто извлекаем потоки по их структуре
  - Пользователь решает, что делать с extracted payload'ом

Примечание:
  Если впоследствии понадобится поддержка конкретного профиля
  (например, DVB subtitle extraction) - добавляется как дополнение,
  но ядро остаётся профайл-агностичным.
```

---

## Алгоритм синхронизации

### 2.1 Быстрое сканирование с адаптивным пропуском мусора

#### Шаг 1: Инициальное сканирование

```
INPUT: Буфер данных size=N

FOR offset = 0 TO N-1:
    IF buffer[offset] == 0x47:  // Найден потенциальный sync byte
        
        // Начинаем попытку синхронизации с этой позиции
        result = TryFindValidIteration(offset)
        
        IF result.found == TRUE:
            SYNC_ALIGNMENT = offset
            SYNC_CONFIDENCE = HIGH
            return result
```

#### Шаг 2: Адаптивный поиск трёх валидных пакетов

```
FUNCTION TryFindValidIteration(start_offset):
    candidates = []
    search_pos = start_offset
    
    // Первый кандидат
    IF IsValidPacket(buffer[start_offset]):
        candidates.append(ParsePacket(start_offset))
    ELSE:
        return {found: false}
    
    // Ищем второй и третий кандидаты АДАПТИВНО
    search_pos = start_offset + 1
    
    WHILE candidates.size() < 3 AND search_pos < N - 188:
        
        IF buffer[search_pos] == 0x47:
            packet_candidate = ParsePacket(search_pos)
            
            IF IsValidPacket(packet_candidate):
                
                // Проверяем принадлежность к одной итерации
                IF BelongsToSameIteration(candidates[-1], packet_candidate):
                    candidates.append(packet_candidate)
                    
                    // После нахождения валидного пакета,
                    // предполагаем что следующий на расстоянии 188
                    search_pos += 188
                    continue
        
        search_pos++  // Адаптивный скип мусора по одному байту
    
    IF candidates.size() >= 3:
        return {found: true, offset: start_offset, candidates: candidates}
    ELSE:
        return {found: false}
```

#### Шаг 3: Проверка валидности пакета

```
FUNCTION IsValidPacket(data):
    // Базовые проверки
    IF data[0] != 0x47:
        return false
    
    // Парсим заголовок
    transport_error_indicator = (data[1] >> 7) & 1
    IF transport_error_indicator == 1:
        return false
    
    // Проверяем что структура парсируется
    adaptation_field_control = (data[3] >> 4) & 0x3
    
    IF adaptation_field_control == 0x0:
        // Reserved, не валидно
        return false
    
    IF adaptation_field_control & 0x2:  // Adaptation field present
        IF NOT IsValidAdaptationField(data):
            return false
    
    return true
```

### 2.2 Определение "одной итерации"

#### Условие принадлежности

```
FUNCTION BelongsToSameIteration(prev_packet, curr_packet):
    
    // ┌─────────────────────────────────┐
    // │ ОБЯЗАТЕЛЬНАЯ ПРОВЕРКА 1:         │
    // │ Continuity Counter (CC)          │
    // └─────────────────────────────────┘
    
    expected_cc = (prev_packet.CC + 1) % 16
    actual_cc = curr_packet.CC
    
    IF actual_cc == expected_cc:
        // ✓ CC последовательный, всё хорошо
        pass
    ELSE:
        // Проверяем есть ли флаг разрыва
        IF curr_packet.discontinuity_indicator == 1:
            // ✓ Разрыв допускается
            pass
        ELSE:
            // ❌ Неожиданный разрыв, разные потоки
            return false
    
    
    // ┌─────────────────────────────────┐
    // │ УСЛОВНАЯ ПРОВЕРКА 2:             │
    // │ Adaptation Field                 │
    // └─────────────────────────────────┘
    
    IF prev_packet.adaptation_field_control & 0x2:
        // Если adaptation field есть, проверяем его
        
        IF curr_packet.adaptation_field_control != prev_packet.adaptation_field_control:
            // Различие в наличии adaptation field
            // Это может означать разные потоки
            
            // ⚠️ Адаптивный выбор:
            // - Если оба имеют payload → разные структуры
            // - Если разные комбинации → скорее всего разные
            
            IF prev_packet.adaptation_field_control != 0x3:
                return false  // Было без payload, теперь с payload → разные
    
    
    // ┌─────────────────────────────────┐
    // │ УСЛОВНАЯ ПРОВЕРКА 3:             │
    // │ PID (при известной длине)        │
    // └─────────────────────────────────┘
    
    IF prev_packet.payload_length != 0:
        // Длина известна → можем ожидать следующих пакетов того же потока
        
        IF prev_packet.PID != curr_packet.PID:
            // ❌ Разные PID при известной длине → разные потоки
            return false
    
    // ELSE: payload_length == 0
    // Длина неизвестна → не можем использовать PID как критерий
    // (может быть как одна итерация, так и разные)
    
    
    return true  // ✅ Одна итерация!
```

#### Логичные указатели валидности

| Критерий                    | Обязателен? | Описание                                                     |
| --------------------------- | ----------- | ------------------------------------------------------------ |
| **sync_byte (0x47)**        | ✅ ДА        | Обязателен на позиции [0]                                    |
| **Continuity Counter (CC)** | ✅ ДА        | Должен быть последовательным (0-15) или с discontinuity_indicator |
| **Adaptation Field**        | ⚠️ УСЛОВНО   | Проверяем ТОЛЬКО если ожидаем (adaptation_field_control & 0x2) |
| **PID**                     | ⚠️ УСЛОВНО   | Используем ТОЛЬКО если payload_length ≠ 0                    |
| **PCR**                     | ❌ НЕТ       | Полностью игнорируем (не используется для валидации)         |

### 2.3 Обработка нескольких итераций

#### Сценарий: Две группы валидных пакетов

```
Буфер: [Группа 1: pkt1-3] [мусор] [Группа 2: pkt4-6]

ГРУППА 1:
  pkt1: PID=0x100, CC=5,  adapt=0x3, private_len=0
  pkt2: PID=0x100, CC=6,  adapt=0x3, private_len=0
  pkt3: PID=0x100, CC=7,  adapt=0x3, private_len=0
  
  Результат: ✅ ВАЛИДНАЯ ИТЕРАЦИЯ 1
  IterID: 1

[Разрыв в CC: ожидалось 8, найдено другой поток]

ГРУППА 2:
  pkt4: PID=0x101, CC=1,  adapt=0x1, private_len=5
  pkt5: PID=0x101, CC=2,  adapt=0x1, private_len=0
  pkt6: PID=0x101, CC=3,  adapt=0x1, private_len=0
  
  Результат: ✅ ВАЛИДНАЯ ИТЕРАЦИЯ 2
  IterID: 2
  discontinuity_detected: TRUE  ← флаг!

ДОБАВЛЕНИЯ В ХРАНИЛИЩЕ:
  streams[0x100].iterations.push_back({1, IterData1})
  streams[0x101].iterations.push_back({2, IterData2})
```

#### Специальный случай: Разрыв в одном потоке

```
Буфер: [pkt1: 0x100, CC=5] [pkt2: 0x100, CC=1] [pkt3: 0x100, CC=2]

pkt1 → Может быть конец группы 1
pkt2 → Начало группы 2 (разрыв CC: 5→1)
pkt3 → Продолжение группы 2

РЕЗУЛЬТАТ: ДВА НАБОРА
  Итерация 1: [pkt1]                 (неполная группа)
  Итерация 2: [pkt2, pkt3]           (с discontinuity_indicator)
  
ЛОГИКА:
  IF curr_cc < prev_cc AND discontinuity_indicator != 1:
    → Возможен разрыв, но не явный
    → Рассматриваем как начало новой группы
```

---

## Структура данных и хранилище

### 3.1 Типы данных

#### PayloadType - Тип полезной нагрузки

```cpp
enum class PayloadType {
    PAYLOAD_NORMAL   = 0,   // Основные данные потока
    PAYLOAD_PRIVATE  = 1    // Приватные данные (из adaptation field)
};
```

#### PayloadSegment - Сегмент данных

```cpp
struct PayloadSegment {
    PayloadType  type;                // NORMAL или PRIVATE
    uint8_t*     data;                // Указатель на данные
    size_t       length;              // Размер в байтах
    size_t       offset_in_stream;    // Позиция в общем потоке
};

Пример:
  segment1: {type: PRIVATE, data: 0x12345678, len: 16}
  segment2: {type: NORMAL,  data: 0x12345690, len: 1024}
```

#### IterationData - Данные одной итерации

```cpp
struct IterationData {
    // Полезные нагрузки (может быть PRIVATE + NORMAL)
    vector<PayloadSegment>  payloads;
    
    // Информационные флаги
    bool         discontinuity_detected;      // Был ли разрыв CC?
    bool         payload_unit_start_seen;     // Начало PES фрейма?
    bool         is_complete;                 // Фрейм завершён?
    
    // Метаданные последовательности
    uint8_t      first_cc;                    // CC первого пакета итерации
    uint8_t      last_cc;                     // CC последнего пакета итерации
    size_t       packet_count;                // Количество пакетов
    
    // Временные метаданные
    timestamp_t  first_packet_time;           // Время получения
    size_t       buffer_position;             // Позиция в буфере
};

Пример:
  IterationData {
      payloads: [
          {type: PRIVATE, data: [...16 байт...], len: 16},
          {type: NORMAL,  data: [...548 байт...], len: 548},
      ],
      discontinuity_detected: false,
      first_cc: 5,
      last_cc: 7,
      packet_count: 3,
  }
```

#### StreamIterations - Вектор итераций для одного PID

```cpp
struct StreamIterations {
    uint16_t  PID;                    // Идентификатор потока
    
    // Вектор пар: (IterationID, IterationData)
    vector<pair<uint32_t, IterationData>>  iterations;
    
    // Текущая аккумулирующаяся итерация
    uint32_t          current_iter_id;
    IterationData*    current_iter;
    
    // Метаданные потока
    set<uint8_t>      observed_cc_values;
    bool              has_discontinuity;
};

Пример структуры:
  StreamIterations {
      PID: 0x100,
      iterations: [
          {1, IterData{...}},
          {2, IterData{...}},
          {3, IterData{...}},
      ],
      current_iter_id: 3,
  }
```

### 3.2 Главное хранилище: DemuxerStreamStorage

```cpp
class DemuxerStreamStorage {
private:
    // KEY: uint16_t PID (или можно как "0x100" string)
    // VALUE: StreamIterations для этого PID
    map<uint16_t, StreamIterations>  streams;
    
    // Счётчик для генерации уникальных IterationID
    uint32_t  next_iteration_id;
    
public:
    // Получить или создать поток
    StreamIterations& getOrCreateStream(uint16_t pid);
    
    // Получить существующий поток
    const StreamIterations& getStream(uint16_t pid) const;
    
    // Все потоки
    const map<uint16_t, StreamIterations>& getAllStreams() const;
    
    // Генерация новых IterationID
    uint32_t generateIterationID();
    
    // Очистка
    void clear();
    void clearStream(uint16_t pid);
};
```

### 3.3 Визуализация иерархии

```
DemuxerStreamStorage (главное хранилище)
│
├─ streams[ 0x100 ]
│  └─ StreamIterations {
│      PID: 0x100,
│      iterations: [
│          {1, IterationData{payloads: [...], ...}},
│          {2, IterationData{payloads: [...], ...}},
│          {3, IterationData{payloads: [...], ...}},
│      ]
│  }
│
├─ streams[ 0x101 ]
│  └─ StreamIterations {
│      PID: 0x101,
│      iterations: [
│          {4, IterationData{payloads: [...], ...}},
│      ]
│  }
│
└─ streams[ 0x200 ]
   └─ StreamIterations {
       PID: 0x200,
       iterations: [
           {5, IterationData{payloads: [...], ...}},
           {6, IterationData{payloads: [...], ...}},  ← discontinuity_detected: TRUE
       ]
   }
```

---

## Система управления PID и программами

### 4.1 Фильтрация системных PID

#### Системные PID (игнорируем)

```cpp
const set<uint16_t> SYSTEM_PIDS = {
    0x0000,  // PAT (Program Association Table)
    0x0001,  // CAT (Conditional Access Table)
};

class SystemPIDFilter {
public:
    static bool isSystemPID(uint16_t pid) {
        return SYSTEM_PIDS.count(pid) > 0;
    }
    
    static bool isProgramStream(uint16_t pid) {
        return !isSystemPID(pid);
    }
};
```

#### Логика фильтрации

```
КОГДА добавляем пакет в storage:

IF SystemPIDFilter::isProgramStream(packet.PID):
    ✓ Добавляем в хранилище
ELSE:
    ✗ Игнорируем (это системный поток)
```

### 4.2 Режим без таблицы программ

#### Состояние

```
programs_table_available: FALSE
```

#### Поведение

```
Действие:
  ✓ Накапливаем ВСЕ найденные потоки
  ✓ Фильтруем ТОЛЬКО явные системные (PAT=0x0000, CAT=0x0001)
  ✓ Остальное считаем программными потоками
  
streams = {
  0x100: StreamIterations{...},
  0x101: StreamIterations{...},
  0x102: StreamIterations{...},
  0x200: StreamIterations{...},
  0x201: StreamIterations{...},
  ...
};

Проблема:
  - Не знаем, к какой программе принадлежит каждый поток
  - Не знаем типы потоков (H264, AAC, и т.д.)
  
Решение:
  - Накапливаем всё
  - Пользователь может позже предоставить таблицу
```

### 4.3 Режим с таблицей программ

#### Состояние

```
programs_table_available: TRUE
programs_table: {
    program_1: {
        program_number: 1,
        pids: [0x100, 0x101, 0x102]
    },
    program_2: {
        program_number: 2,
        pids: [0x200, 0x201]
    },
    ...
}
```

#### Поведение

```
Действие:
  ✓ Работаем ТОЛЬКО с PID из programs_table
  ✓ Новые неизвестные PID:
    - ОТБРАСЫВАЕМ (не добавляем в storage)
    - Игнорируем их
  
streams = {
  0x100: StreamIterations{...},  ← Из программы 1
  0x101: StreamIterations{...},  ← Из программы 1
  0x102: StreamIterations{...},  ← Из программы 1
  0x200: StreamIterations{...},  ← Из программы 2
  0x201: StreamIterations{...},  ← Из программы 2
  // 0x103, 0x202, 0x300, etc. → ИГНОРИРУЕМ
};

Переход:
  IF programs_table_available == FALSE:
      Накапливаем всё
  
  IF programs_table_available == TRUE:
      // Старые данные отбрасываются (или фильтруются)
      streams.clear()  // или переинициализация
      only_known_pids = true
```

### 4.4 Информация о программах

#### Структура ProgramInfo

```cpp
struct ProgramInfo {
    uint16_t program_number;           // Номер программы
    vector<uint16_t> stream_pids;      // Список PID потоков
    size_t total_payload_size;         // Общий размер полезной нагрузки
    size_t iteration_count;            // Количество итераций
    bool has_discontinuity;            // Есть ли разрывы?
};
```

#### Получение информации о программах

```cpp
vector<ProgramInfo> getPrograms() const {
    vector<ProgramInfo> result;
    
    for (auto& [pid, stream] : storage.getAllStreams()) {
        if (SystemPIDFilter::isProgramStream(pid)) {
            ProgramInfo info;
            info.stream_pids.push_back(pid);
            
            for (auto& [iter_id, iter_data] : stream.iterations) {
                info.iteration_count++;
                info.total_payload_size += GetPayloadSize(iter_data);
                
                if (iter_data.discontinuity_detected) {
                    info.has_discontinuity = true;
                }
            }
            
            result.push_back(info);
        }
    }
    
    return result;
}
```

---

## Обработка payload'ов

### 5.1 Структура MPEG-TS пакета в контексте payload'ов

#### Пакет с обеими типами данных

```
[4 байта: Header]
    [1: sync_byte=0x47]
    [1: error, PUSI, priority, PID[12:8]]
    [1: PID[7:0]]
    [1: scrambling, adaptation_control, CC]

[variable: Adaptation Field] (если adaptation_field_control & 0x2)
    [1: adaptation_field_length]
    [1: flags]
        [bit 7: discontinuity_indicator]
        [bit 6: random_access_indicator]
        [bit 5: ES_priority_indicator]
        [bit 4: PCR_flag]
        [bit 3: OPCR_flag]
        [bit 2: splicing_point_flag]
        [bit 1: transport_private_data_flag]  ◄── КЛЮЧ
        [bit 0: adaptation_field_extension_flag]
    
    [если transport_private_data_flag == 1]:
        [1: private_data_length]
        [N: private_data[private_data_length]]  ◄── PAYLOAD_PRIVATE
    
    [остальные optional поля...]

[variable: Payload] (если adaptation_field_control & 0x1)
    [до конца пакета: основная полезная нагрузка]  ◄── PAYLOAD_NORMAL
```

### 5.2 Извлечение приватных данных

#### Алгоритм парсирования

```
INPUT: MPEG-TS пакет (188 байт)

STEP 1: Извлекаем основной payload
────────────────────────────────────
payload_offset = 4  // После header'а
payload_length = 188 - 4

IF adaptation_field_control & 0x2:
    // Adaptation field есть
    adaptation_field_length = packet[payload_offset]
    payload_offset += 1 + adaptation_field_length
    payload_length -= (1 + adaptation_field_length)

IF adaptation_field_control & 0x1:
    // Payload есть
    EXTRACT_PAYLOAD(payload_offset, payload_length)
    AddSegment({type: PAYLOAD_NORMAL, ...})


STEP 2: Извлекаем приватные данные
────────────────────────────────────
IF adaptation_field_control & 0x2:
    
    adaptation_field_data_offset = 5  // После sync+error+PUSI+PID+scrambling
    flags = packet[adaptation_field_data_offset + 1]
    
    IF (flags & 0x02) == 0x02:  // transport_private_data_flag
        
        // Шагаем через optional поля до private_data
        field_offset = adaptation_field_data_offset + 2
        
        IF (flags & 0x10):  // PCR flag
            field_offset += 6
        
        IF (flags & 0x08):  // OPCR flag
            field_offset += 6
        
        IF (flags & 0x04):  // Splicing point flag
            field_offset += 1
        
        // Теперь находимся на private_data_length
        private_data_length = packet[field_offset]
        private_data_start = field_offset + 1
        
        IF private_data_length > 0:
            EXTRACT_PRIVATE_DATA(private_data_start, private_data_length)
            AddSegment({type: PAYLOAD_PRIVATE, ...})
```

### 5.3 Фрагментация данных

#### Случай 1: Данные в одном пакете

```
Пакет 1:
  [Header]
  [Adaptation Field with private_data: 16 байт]
  [Payload: 150 байт]

Результат IterationData:
  payloads: [
      {type: PRIVATE, data: [...16 байт...], len: 16},
      {type: NORMAL,  data: [...150 байт...], len: 150},
  ]
```

#### Случай 2: Фрагментация основного payload'а

```
Пакет 1 (payload_unit_start=1):
  [Header]
  [Payload START: 180 байт]

Пакет 2 (payload_unit_start=0):
  [Header]
  [Payload CONTINUE: 188 байт]

Пакет 3 (payload_unit_start=0):
  [Header]
  [Payload END: 188 байт]

Результат в одной IterationData:
  payloads: [
      {type: NORMAL, data: [180+188+188=556 байт], ...}
  ]
```

#### Случай 3: Фрагментация приватных данных

```
Пакет 1 (с приватными данными):
  [Header]
  [Adaptation Field with private_data: 8 байт]
  [Payload: 150 байт]
  
  ПРИМЕЧАНИЕ: Приватные данные могут быть фрагментированы
  если следующий пакет того же stream'а содержит продолжение
```

### 5.4 Синхронизация payload'ов

#### По payload_unit_start_indicator (PUSI)

```
PUSI = 1: Начало нового PES фрейма (новая группа)
PUSI = 0: Продолжение текущего PES фрейма

Логика:
  IF PUSI == 1:
      // Закрываем текущую итерацию (если есть)
      IF current_iter not empty:
          AddIterationToStorage()
      
      // Начинаем новую
      current_iter = IterationData()
  
  // Добавляем payload'ы в текущую итерацию
  current_iter.payloads.push_back(...)
```

---

## API и интерфейсы

### 6.1 Главный класс MPEGTSDemuxer

```cpp
class MPEGTSDemuxer {
private:
    // === ВНУТРЕННЕЕ СОСТОЯНИЕ ===
    
    // Хранилище потоков
    DemuxerStreamStorage        storage;
    
    // Циркулярный буфер для сырых данных
    circular_buffer<uint8_t>    raw_buffer;         // MAX 100 packets
    
    // Состояние синхронизации
    bool                        is_synchronized;
    size_t                      sync_offset;
    uint8_t                     sync_validation_depth;
    
    // Информация о программах
    bool                        programs_table_available;
    set<uint16_t>               known_program_pids;
    
    // === ВНУТРЕННИЕ МЕТОДЫ ===
    
    bool validatePacket(const uint8_t* data);
    bool belongsToSameIteration(const TSPacket& p1, const TSPacket& p2);
    void addPacketToStorage(const TSPacket& packet);
    void handleDiscontinuity(uint16_t pid);
    
public:
    // === ОСНОВНОЙ API ===
    
    // Подача данных в демультиплексор
    void feedData(const uint8_t* data, size_t len);
    
    // === ПОЛУЧЕНИЕ ИНФОРМАЦИИ О ПРОГРАММАХ ===
    
    vector<ProgramInfo> getPrograms() const;
    set<uint16_t> getDiscoveredPIDs() const;
    
    // === ПОЛУЧЕНИЕ ДАННЫХ ИТЕРАЦИЙ ===
    
    // Информация об итерациях потока
    vector<IterationInfo> getIterationsSummary(uint16_t pid) const;
    
    // Структура IterationInfo
    struct IterationInfo {
        uint32_t iteration_id;
        size_t payload_normal_size;
        size_t payload_private_size;
        bool has_discontinuity;
        uint8_t cc_start;
        uint8_t cc_end;
    };
    
    // === ПОЛУЧЕНИЕ PAYLOAD'ОВ ===
    
    // Один payload определённого типа
    struct PayloadBuffer {
        uint8_t*    data;
        size_t      length;
        PayloadType type;
    };
    
    PayloadBuffer getPayload(uint16_t pid, uint32_t iter_id,
                            PayloadType type = PayloadType::PAYLOAD_NORMAL);
    
    // ВСЕ payload'ы итерации (normal + private)
    vector<PayloadBuffer> getAllPayloads(uint16_t pid, uint32_t iter_id);
    
    // === УПРАВЛЕНИЕ ДАННЫМИ ===
    
    // Очистить конкретную итерацию
    void clearIteration(uint16_t pid, uint32_t iter_id);
    
    // Очистить весь поток
    void clearStream(uint16_t pid);
    
    // Очистить всё хранилище
    void clearAll();
    
    // === ИНФОРМАЦИЯ О СОСТОЯНИИ ===
    
    bool isSynchronized() const;
    size_t getBufferOccupancy() const;
    size_t getPacketCount() const;
    
    // === УПРАВЛЕНИЕ ТАБЛИЦАМИ ПРОГРАММ ===
    
    struct ProgramTable {
        map<uint16_t, vector<uint16_t>> programs;  // program_id → [PIDs]
    };
    
    void setProgramsTable(const ProgramTable& table);
    bool hasProgramsTable() const;
};
```

### 6.2 Структуры данных API

#### IterationInfo - Информация об итерации

```cpp
struct IterationInfo {
    uint32_t iteration_id;           // Уникальный ID
    size_t payload_normal_size;      // Размер основного payload'а
    size_t payload_private_size;     // Размер приватного payload'а
    bool has_discontinuity;          // Флаг разрыва CC
    uint8_t cc_start;                // CC первого пакета
    uint8_t cc_end;                  // CC последнего пакета
    size_t packet_count;             // Кол-во пакетов в итерации
};
```

#### PayloadBuffer - Буфер данных

```cpp
struct PayloadBuffer {
    uint8_t*    data;                // Указатель на данные
    size_t      length;              // Размер в байтах
    PayloadType type;                // NORMAL или PRIVATE
};

Особенность:
  - data указывает на память внутри storage
  - Валидна до вызова clearIteration() или clearStream()
  - НЕ требует освобождения (managed by storage)
```

#### ProgramInfo - Информация о программе

```cpp
struct ProgramInfo {
    uint16_t program_number;         // Номер программы (если известен)
    vector<uint16_t> stream_pids;    // Список PID потоков в программе
    size_t total_payload_size;       // Общий размер полезной нагрузки
    size_t iteration_count;          // Количество итераций
    bool has_discontinuity;          // Есть ли разрывы?
};
```

### 6.3 Базовые операции

#### Операция 1: Подача данных и синхронизация

```cpp
// Инициализация
MPEGTSDemuxer demuxer;

// Подача данных (может быть вызовано многократно)
uint8_t buffer[1024];
size_t bytes_read = read_from_stream(buffer, sizeof(buffer));
demuxer.feedData(buffer, bytes_read);

// Проверка синхронизации
if (demuxer.isSynchronized()) {
    cout << "Synced! Buffer occupancy: " 
         << demuxer.getBufferOccupancy() << " packets\n";
}
```

#### Операция 2: Получение информации о программах

```cpp
// Получить все найденные потоки
auto programs = demuxer.getPrograms();

for (auto& prog : programs) {
    cout << "Program #" << prog.program_number 
         << " has " << prog.stream_pids.size() << " streams\n";
    
    for (auto pid : prog.stream_pids) {
        cout << "  Stream PID: 0x" << hex << pid << "\n";
    }
}
```

#### Операция 3: Получение итераций потока

```cpp
// Получить итерации для конкретного PID
auto iterations = demuxer.getIterationsSummary(0x100);

for (auto& iter : iterations) {
    cout << "Iteration #" << iter.iteration_id << ":\n"
         << "  Normal payload: " << iter.payload_normal_size << " bytes\n"
         << "  Private payload: " << iter.payload_private_size << " bytes\n"
         << "  Discontinuity: " << (iter.has_discontinuity ? "YES" : "NO") << "\n";
}
```

#### Операция 4: Извлечение payload'ов

```cpp
// Получить только основной payload
auto normal = demuxer.getPayload(0x100, 1, PayloadType::PAYLOAD_NORMAL);
process_h264_data(normal.data, normal.length);

// Получить только приватный payload
auto private_data = demuxer.getPayload(0x100, 1, PayloadType::PAYLOAD_PRIVATE);
handle_private_data(private_data.data, private_data.length);

// Получить оба payload'а
auto all = demuxer.getAllPayloads(0x100, 1);
for (auto& payload : all) {
    if (payload.type == PayloadType::PAYLOAD_NORMAL) {
        cout << "Normal: " << payload.length << " bytes\n";
    } else {
        cout << "Private: " << payload.length << " bytes\n";
    }
}
```

#### Операция 5: Очистка и управление памятью

```cpp
// Очистить конкретную итерацию после обработки
demuxer.clearIteration(0x100, 1);

// Или очистить весь поток
demuxer.clearStream(0x100);

// Или всё сразу
demuxer.clearAll();
```

### 6.4 Работа с таблицей программ

#### Установка таблицы

```cpp
MPEGTSDemuxer::ProgramTable table;

table.programs[1] = {0x100, 0x101, 0x102};  // Программа 1
table.programs[2] = {0x200, 0x201};         // Программа 2

demuxer.setProgramsTable(table);

// После установки таблицы:
// - Знаем структуру программ
// - Игнорируем неизвестные PID
// - Старые данные переинициализируются
```

#### Проверка наличия таблицы

```cpp
if (demuxer.hasProgramsTable()) {
    cout << "Working with explicit program table\n";
} else {
    cout << "Accumulating all streams (no table)\n";
}
```

---

## Режимы работы

### 7.1 Режим 1: Без таблицы программ

#### Сценарий

```
┌─────────────────────────────────────────┐
│ Статус: programs_table = NOT AVAILABLE  │
│                                          │
│ Действие: Накопление всех потоков      │
└─────────────────────────────────────────┘

Поток MPEG-TS:
  [Пакет PID=0x100] → Добавляем в storage
  [Пакет PID=0x101] → Добавляем в storage
  [Пакет PID=0x200] → Добавляем в storage
  [Пакет PID=0x0000] → ПРОПУСКАЕМ (системный PAT)
  [Пакет PID=0x0001] → ПРОПУСКАЕМ (системный CAT)
  
Результат:
  streams {
    0x100: StreamIterations{...},
    0x101: StreamIterations{...},
    0x200: StreamIterations{...},
    // Неизвестно, к каким программам они относятся
  }
```

#### Характеристики

```
✓ Преимущества:
  - Работает с любым потоком сразу
  - Накапливаем максимум данных
  - Гибко
  
✗ Недостатки:
  - Не знаем структуру программ
  - Может быть "мусор" в виде неизвестных потоков
  - Сложнее организовать обработку
```

### 7.2 Режим 2: С таблицей программ

#### Сценарий

```
┌─────────────────────────────────────────┐
│ Статус: programs_table = AVAILABLE      │
│ Таблица:                                │
│   Program 1: [0x100, 0x101, 0x102]     │
│   Program 2: [0x200, 0x201]            │
└─────────────────────────────────────────┘

Поток MPEG-TS:
  [Пакет PID=0x100] → ✓ В программе 1, добавляем
  [Пакет PID=0x101] → ✓ В программе 1, добавляем
  [Пакет PID=0x102] → ✓ В программе 1, добавляем
  [Пакет PID=0x103] → ✗ НЕ в таблице, ПРОПУСКАЕМ
  [Пакет PID=0x200] → ✓ В программе 2, добавляем
  [Пакет PID=0x201] → ✓ В программе 2, добавляем
  
Результат:
  streams {
    0x100: StreamIterations{...},  ← Program 1
    0x101: StreamIterations{...},  ← Program 1
    0x102: StreamIterations{...},  ← Program 1
    0x200: StreamIterations{...},  ← Program 2
    0x201: StreamIterations{...},  ← Program 2
    // PID 0x103 и выше НЕ в потоке
  }
```

#### Характеристики

```
✓ Преимущества:
  - Знаем точную структуру программ
  - Фильтруем "мусор" (неизвестные PID)
  - Организованная структура
  - Можем связать с метаданными программ
  
✗ Недостатки:
  - Нужна таблица программ (откуда её получить?)
  - Менее гибко
  - Если таблица неполна → потеряем данные
```

### 7.3 Переход между режимами

#### Сценарий: Получили таблицу в процессе

```
Шаг 1: Начало работы (таблицы нет)
  ├─ Накапливаем потоки: 0x100, 0x101, 0x102, 0x200, 0x201, 0x50

Шаг 2: В процессе получена таблица
  ├─ Вызываем setProgramsTable(table)
  ├─ table содержит: Program1=[0x100,0x101], Program2=[0x200,0x201]

Шаг 3: Переинициализация хранилища
  ├─ Старые данные (потокиф 0x102, 0x50) → ОТБРАСЫВАЮТСЯ
  ├─ Новое хранилище теперь знает структуру
  └─ Начинаем с чистого листа (или filtrуем старые данные)

Шаг 4: Дальнейшая работа
  └─ Работаем ТОЛЬКО с PID из таблицы
```

---

## Примеры использования

### 8.1 Базовый пример

```cpp
#include "mpegts_demuxer.hpp"

int main() {
    // Создаём демультиплексор
    MPEGTSDemuxer demuxer;
    
    // Открываем MPEG-TS файл
    std::ifstream file("stream.ts", std::ios::binary);
    
    const size_t CHUNK_SIZE = 4096;
    uint8_t buffer[CHUNK_SIZE];
    
    // Подаём данные порциями
    while (file.read(reinterpret_cast<char*>(buffer), CHUNK_SIZE)) {
        size_t bytes_read = file.gcount();
        demuxer.feedData(buffer, bytes_read);
        
        // Проверяем синхронизацию
        if (demuxer.isSynchronized()) {
            auto programs = demuxer.getPrograms();
            std::cout << "Found " << programs.size() << " streams\n";
        }
    }
    
    // Обрабатываем накопленные данные
    auto programs = demuxer.getPrograms();
    
    for (auto& prog : programs) {
        for (auto pid : prog.stream_pids) {
            auto iterations = demuxer.getIterationsSummary(pid);
            std::cout << "Stream 0x" << std::hex << pid << " has "
                      << iterations.size() << " iterations\n";
            
            for (auto& iter : iterations) {
                auto payload = demuxer.getPayload(pid, iter.iteration_id);
                
                // Обработка payload'а
                std::cout << "Iteration " << iter.iteration_id
                          << ": " << payload.length << " bytes\n";
                
                // Очистка
                demuxer.clearIteration(pid, iter.iteration_id);
            }
        }
    }
    
    return 0;
}
```

### 8.2 Пример с отдельной обработкой private payload'ов

```cpp
void ProcessStream(MPEGTSDemuxer& demuxer, uint16_t pid) {
    auto iterations = demuxer.getIterationsSummary(pid);
    
    for (auto& iter : iterations) {
        // Получить оба типа payload'ов
        auto all_payloads = demuxer.getAllPayloads(pid, iter.iteration_id);
        
        for (auto& payload : all_payloads) {
            if (payload.type == PayloadType::PAYLOAD_NORMAL) {
                // Обработка основного payload'а (H.264/H.265)
                ProcessVideoData(payload.data, payload.length);
            } 
            else if (payload.type == PayloadType::PAYLOAD_PRIVATE) {
                // Обработка приватных данных
                ProcessPrivateMetadata(payload.data, payload.length);
            }
        }
        
        // Очищаем после обработки
        demuxer.clearIteration(pid, iter.iteration_id);
    }
}

int main() {
    MPEGTSDemuxer demuxer;
    
    // ... подача данных ...
    
    // Обработка каждого найденного потока
    auto pids = demuxer.getDiscoveredPIDs();
    
    for (auto pid : pids) {
        ProcessStream(demuxer, pid);
    }
    
    return 0;
}
```

### 8.3 Пример с таблицей программ

```cpp
int main() {
    MPEGTSDemuxer demuxer;
    
    // Парсируем PAT/PMT и получаем таблицу
    // (в реальном коде это происходит отдельно)
    MPEGTSDemuxer::ProgramTable table;
    table.programs[1] = {0x100, 0x101};  // Program 1
    table.programs[2] = {0x200, 0x201};  // Program 2
    
    // Устанавливаем таблицу
    demuxer.setProgramsTable(table);
    
    // Подаём данные
    uint8_t buffer[4096];
    while (ReadStream(buffer, sizeof(buffer))) {
        demuxer.feedData(buffer, sizeof(buffer));
    }
    
    // Теперь работаем с конкретными программами
    auto programs = demuxer.getPrograms();
    
    for (auto& prog : programs) {
        std::cout << "Program " << prog.program_number 
                  << " with " << prog.stream_pids.size() 
                  << " streams:\n";
        
        for (auto pid : prog.stream_pids) {
            auto iters = demuxer.getIterationsSummary(pid);
            std::cout << "  PID 0x" << std::hex << pid 
                      << ": " << iters.size() << " iterations\n";
        }
    }
    
    return 0;
}
```

### 8.4 Пример с обработкой разрывов

```cpp
void AnalyzeStream(MPEGTSDemuxer& demuxer, uint16_t pid) {
    auto iterations = demuxer.getIterationsSummary(pid);
    
    uint8_t expected_cc = 0;
    
    for (auto& iter : iterations) {
        std::cout << "Iteration " << iter.iteration_id << ":\n";
        
        if (iter.has_discontinuity) {
            std::cout << "  ⚠️ DISCONTINUITY DETECTED!\n"
                      << "  CC jump from " << int(expected_cc) 
                      << " to " << int(iter.cc_start) << "\n";
        }
        
        std::cout << "  CC sequence: " << int(iter.cc_start) 
                  << " -> " << int(iter.cc_end) << "\n"
                  << "  Packets: " << iter.packet_count << "\n"
                  << "  Size: " << iter.payload_normal_size << " bytes\n";
        
        expected_cc = (iter.cc_end + 1) % 16;
    }
}
```

---

## Детали реализации

### 9.1 Обязательные требования к реализации

#### Буферизация

- [ ] Циркулярный буфер на 100 пакетов (18.8 КБ)
- [ ] Эффективное управление памятью (переиспользование буфера)
- [ ] Не превышать лимит в 18.8 КБ

#### Синхронизация

- [ ] Быстрый скан на 0x47
- [ ] Адаптивный поиск с пропуском мусора
- [ ] Трёхитеративная валидация (3 пакета с одной итерацией)
- [ ] Обнаружение и разделение разных итераций
- [ ] Continuity counter валидация

#### Хранилище

- [ ] Map<PID, StreamIterations>
- [ ] Вектор пар (IterationID, IterationData)
- [ ] Раздельное хранение NORMAL и PRIVATE payload'ов
- [ ] Автоинкрементирующиеся IterationID

#### API

- [ ] feedData() - основной метод подачи
- [ ] getPrograms() - получить список потоков
- [ ] getIterationsSummary() - итерации потока
- [ ] getPayload() / getAllPayloads() - извлечение данных
- [ ] clearIteration() / clearStream() - управление памятью

### 9.2 Оптимизации

#### Производительность сканирования

```cpp
// Быстрый скан на 0x47
while (pos < buffer.size()) {
    if (buffer[pos] == 0x47) {
        // Попытка валидации
    }
    pos++;
}
```

#### Минимизация копирований

```cpp
// Payload'ы хранятся как указатели на storage
// Не копируются, пока не требуется явно

struct PayloadSegment {
    uint8_t* data;  // Указатель на память в storage
    size_t length;
};
```

#### Кэширование результатов

```cpp
// Результаты парсирования сохраняются в IterationData
// Повторный парсинг не требуется
```

### 9.3 Обработка ошибок

#### Типичные ошибочные ситуации

| Ситуация            | Обработка                                   |
| ------------------- | ------------------------------------------- |
| Весь буфер - мусор  | Продолжаем сканировать, не синхронизируемся |
| Потеря синхр        | Ищем 3 валидных пакета заново               |
| Переполнение буфера | Данные теряются, сканируем дальше           |
| Поврежденный пакет  | Пропускаем, ищем следующий                  |
| Неполные данные     | Ждём feedData() с дополнительными           |

### 9.4 Потокобезопасность

#### Текущая архитектура

```cpp
// На данный момент НЕ thread-safe
// Предполагается однопоточное использование

// Если требуется многопоточность:
// - Добавить mutex для storage
// - Добавить thread-safe очередь для feedData()
// - Синхронизировать доступ к буферам
```

---

## Заключение

### Ключевые принципы

1. **Адаптивность** - автоматическое восстановление в условиях помех
2. **Надёжность** - трёхитеративная валидация предотвращает false positives
3. **Эффективность** - минимум копирований, быстрое сканирование
4. **Гибкость** - работает с таблицей и без
5. **Простота** - понятный API, чётко разделённые обязанности

### Возможные расширения

```
Будущие версии могут включать:

  • Поддержка PAT/PMT парсирования
  • Extraction таблиц (PAT, PMT, NIT, и т.д.)
  • PCR-based синхронизация
  • DVB subtitle extraction
  • Teletext data extraction
  • Multi-threading support
  • Real-time streaming optimizations
```

---

**Версия документа:** 1.0  
**Последнее обновление:** November 2025  
**Статус:** Готово к реализации
