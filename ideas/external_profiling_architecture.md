# 🔬 External Machine Profiling Architecture

**Version:** 1.0
**Created:** November 12, 2025
**Purpose:** Профилирование на внешних машинах с передачей результатов

---

## 📋 Содержание

1. [Обзор архитектуры](#обзор-архитектуры)
2. [Инструменты для Linux Ubuntu 22.04](#инструменты-для-linux-ubuntu-2204)
3. [Инструменты для Windows 11 x64](#инструменты-для-windows-11-x64)
4. [Workflow профилирования](#workflow-профилирования)
5. [Форматы данных для передачи](#форматы-данных-для-передачи)
6. [Скрипты автоматизации](#скрипты-автоматизации)
7. [Интерпретация результатов](#интерпретация-результатов)

---

## 🏗️ Обзор архитектуры

### Принцип работы

```
┌─────────────────────────────────────────────────────────────┐
│                    EXTERNAL MACHINE                          │
│  (Linux Ubuntu 22.04 / Windows 11 x64)                      │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  1. Скомпилировать проект с профилированием          │  │
│  │     - Linux: perf, valgrind, gprof                   │  │
│  │     - Windows: Visual Studio Profiler, VTune         │  │
│  └──────────────────────────────────────────────────────┘  │
│                           │                                  │
│                           ▼                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  2. Запустить бенчмарки с профилированием            │  │
│  │     - Собрать данные CPU, memory, cache              │  │
│  │     - Сохранить в текстовые/JSON файлы               │  │
│  └──────────────────────────────────────────────────────┘  │
│                           │                                  │
│                           ▼                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  3. Экспортировать результаты                        │  │
│  │     - perf.txt, valgrind.txt, callgrind.out         │  │
│  │     - JSON с метриками                               │  │
│  └──────────────────────────────────────────────────────┘  │
└────────────────────────┬─────────────────────────────────────┘
                         │
                         │ Transfer results
                         │ (copy-paste / file transfer)
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                    DEVELOPMENT MACHINE                       │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  4. Анализ результатов Claude                        │  │
│  │     - Парсинг текстовых отчётов                      │  │
│  │     - Выявление hotspots                             │  │
│  │     - Рекомендации по оптимизации                    │  │
│  └──────────────────────────────────────────────────────┘  │
│                           │                                  │
│                           ▼                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  5. Реализация оптимизаций                           │  │
│  │     - SIMD, zero-copy, memory pools                  │  │
│  │     - Коммит изменений                               │  │
│  └──────────────────────────────────────────────────────┘  │
│                           │                                  │
│                           ▼                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  6. Повторное профилирование (цикл)                  │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## 🐧 Инструменты для Linux Ubuntu 22.04

### Установка всех инструментов

```bash
# Обновление системы
sudo apt-get update

# Основные инструменты профилирования
sudo apt-get install -y \
    linux-tools-common \
    linux-tools-generic \
    linux-tools-$(uname -r) \
    valgrind \
    kcachegrind \
    google-perftools \
    libgoogle-perftools-dev

# Дополнительные утилиты
sudo apt-get install -y \
    sysstat \
    htop \
    iotop \
    strace
```

### 1. perf (Linux Performance Tool) ⭐ **Приоритет: HIGH**

#### Что измеряет
- CPU hotspots (функции, занимающие больше всего времени)
- Cache miss rates (L1, L2, L3)
- Branch prediction misses
- CPU cycles, instructions

#### Использование

**A. CPU профилирование (функции, hotspots)**
```bash
# Компиляция с отладочной информацией
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build .

# Запуск профилирования
perf record -g ./tests/test_demuxer_basic

# Просмотр результатов
perf report > perf_report.txt

# Экспорт для анализа
perf report --stdio --no-children --percent-limit 1 > perf_detailed.txt
perf annotate > perf_annotate.txt
```

**B. Cache анализ**
```bash
perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses \
    ./tests/test_demuxer_basic > perf_cache.txt 2>&1
```

**C. Branch prediction**
```bash
perf stat -e branches,branch-misses,branch-loads,branch-load-misses \
    ./tests/test_demuxer_basic > perf_branches.txt 2>&1
```

**D. CPU cycles детально**
```bash
perf stat -d ./tests/test_demuxer_basic > perf_cycles.txt 2>&1
```

#### Формат вывода для передачи

**perf_report.txt** структура:
```
# Overhead  Command          Shared Object       Symbol
# ........  ...............  ..................  .....................................
  45.23%  test_demuxer_basic  libmpegts_demuxer.a  [.] mpegts::MPEGTSDemuxer::tryFindValidIteration
  12.34%  test_demuxer_basic  libmpegts_demuxer.a  [.] mpegts::TSPacket::parse
   8.91%  test_demuxer_basic  libmpegts_demuxer.a  [.] mpegts::MPEGTSDemuxer::processBuffer
```

**perf_cache.txt** структура:
```
Performance counter stats:

     1,234,567      cache-references
       123,456      cache-misses              #   10.00 % of all cache refs
    10,234,567      L1-dcache-loads
       234,567      L1-dcache-load-misses     #    2.29% of all L1-dcache hits
```

### 2. valgrind (Memory & Cache Profiler) ⭐ **Приоритет: HIGH**

#### Что измеряет
- Memory leaks
- Heap usage
- Cache misses (L1, L2)
- Function call counts

#### Использование

**A. Memory leak detection (memcheck)**
```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind_memcheck.txt \
         ./tests/test_demuxer_basic
```

**B. Heap profiling (massif)**
```bash
valgrind --tool=massif \
         --massif-out-file=massif.out \
         ./tests/test_demuxer_basic

# Конвертация в текст
ms_print massif.out > massif_report.txt
```

**C. Cache profiling (cachegrind)** ⭐ **ОЧЕНЬ ПОЛЕЗНО**
```bash
valgrind --tool=cachegrind \
         --cachegrind-out-file=cachegrind.out \
         ./tests/test_demuxer_basic

# Конвертация в текст
cg_annotate cachegrind.out > cachegrind_report.txt
```

**D. Call graph (callgrind)** ⭐ **ОЧЕНЬ ПОЛЕЗНО**
```bash
valgrind --tool=callgrind \
         --callgrind-out-file=callgrind.out \
         ./tests/test_demuxer_basic

# Конвертация в текст
callgrind_annotate callgrind.out > callgrind_report.txt
```

#### Формат вывода для передачи

**valgrind_memcheck.txt:**
```
LEAK SUMMARY:
   definitely lost: 1,234 bytes in 5 blocks
   indirectly lost: 0 bytes in 0 blocks
     possibly lost: 0 bytes in 0 blocks
   still reachable: 5,678 bytes in 20 blocks
        suppressed: 0 bytes in 0 blocks
```

**cachegrind_report.txt:**
```
Ir          I1mr  ILmr  Dr          D1mr   DLmr   Dw         D1mw   DLmw     file:function
--------------------------------------------------------------------------------
1,234,567   123   45    456,789     89     12     234,567    34     5        mpegts_demuxer.cpp:tryFindValidIteration
  567,890    45   12    123,456     23      3      89,012     8     1        mpegts_packet.cpp:parse
```

**callgrind_report.txt:**
```
Ir          Calls    file:function
--------------------------------------------------------------------------------
12,345,678   10,000   mpegts_demuxer.cpp:feedData
 8,901,234    5,000   mpegts_demuxer.cpp:processBuffer
 4,567,890   15,000   mpegts_packet.cpp:parse
```

### 3. gprof (GNU Profiler) **Приоритет: MEDIUM**

#### Что измеряет
- Function call counts
- Time spent per function
- Call graph

#### Использование

```bash
# Компиляция с -pg флагом
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-pg -g" \
      -DCMAKE_EXE_LINKER_FLAGS="-pg" ..
cmake --build .

# Запуск программы (создаст gmon.out)
./tests/test_demuxer_basic

# Генерация отчёта
gprof ./tests/test_demuxer_basic gmon.out > gprof_report.txt

# Flat profile + call graph
gprof -b ./tests/test_demuxer_basic gmon.out > gprof_full.txt
```

#### Формат вывода

**gprof_report.txt:**
```
Flat profile:

Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total
 time   seconds   seconds    calls   s/call   s/call  name
 45.23      0.45     0.45    10000     0.00     0.00  tryFindValidIteration
 12.34      0.57     0.12     5000     0.00     0.00  processBuffer
  8.91      0.66     0.09    15000     0.00     0.00  parse
```

### 4. time (Simple Benchmarking) **Приоритет: LOW**

```bash
# Детальная статистика
/usr/bin/time -v ./tests/test_demuxer_basic > time_report.txt 2>&1
```

**Формат вывода:**
```
Command being timed: "./tests/test_demuxer_basic"
User time (seconds): 1.23
System time (seconds): 0.45
Percent of CPU this job got: 98%
Elapsed (wall clock) time (h:mm:ss or m:ss): 0:01.70
Maximum resident set size (kbytes): 5678
Page faults: 234
Voluntary context switches: 12
```

### 5. strace (System Calls) **Приоритет: LOW**

```bash
# Трассировка системных вызовов
strace -c -o strace_summary.txt ./tests/test_demuxer_basic
```

**Формат вывода:**
```
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 45.23    0.012345          12      1000           read
 23.45    0.006789          23       300           write
 12.34    0.003456          11       300           mmap
```

---

## 🪟 Инструменты для Windows 11 x64

### 1. Visual Studio Profiler ⭐ **Приоритет: HIGH**

#### Установка
- Visual Studio 2022 Community (бесплатно)
- Или Visual Studio 2019 Professional

#### Использование

**A. Performance Profiler (GUI)**

```powershell
# Компиляция Release с отладкой
cd build
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build . --config RelWithDebInfo

# Запуск профайлера через GUI:
# 1. Открыть Visual Studio
# 2. Debug → Performance Profiler (Alt+F2)
# 3. Выбрать инструменты:
#    - CPU Usage (обязательно)
#    - Memory Usage (рекомендуется)
#    - Instrumentation (опционально)
# 4. Start
# 5. После завершения: File → Export → CSV
```

**B. Command Line Profiler (VSPerfCmd)**

```powershell
# Найти VSPerfCmd (обычно здесь):
# C:\Program Files\Microsoft Visual Studio\2022\Community\Team Tools\Performance Tools\x64\VSPerfCmd.exe

# Инструментация
VSInstr.exe test_demuxer_basic.exe /coverage

# Запуск профилирования
VSPerfCmd /start:trace /output:profile.vsp
test_demuxer_basic.exe
VSPerfCmd /shutdown

# Конвертация в CSV
VSPerfReport profile.vsp /summary:all /output:profile_report.csv
```

#### Формат вывода

**profile_report.csv:**
```csv
Function Name,Total CPU Time (ms),Self CPU Time (ms),Module,Call Count
mpegts::MPEGTSDemuxer::tryFindValidIteration,452.3,452.3,mpegts_demuxer.lib,10000
mpegts::TSPacket::parse,123.4,123.4,mpegts_demuxer.lib,15000
mpegts::MPEGTSDemuxer::processBuffer,89.1,89.1,mpegts_demuxer.lib,5000
```

### 2. Intel VTune Profiler **Приоритет: MEDIUM** (если есть Intel CPU)

#### Установка
```bash
# Скачать Intel oneAPI Base Toolkit
# https://software.intel.com/content/www/us/en/develop/tools/oneapi/components/vtune-profiler.html
```

#### Использование

```powershell
# Hotspots анализ
vtune -collect hotspots -result-dir vtune_results .\test_demuxer_basic.exe

# Экспорт в CSV
vtune -report hotspots -result-dir vtune_results -format csv -report-output vtune_report.csv

# Микроархитектурный анализ
vtune -collect uarch-exploration -result-dir vtune_uarch .\test_demuxer_basic.exe
vtune -report summary -result-dir vtune_uarch -format csv -report-output vtune_uarch.csv
```

#### Формат вывода

**vtune_report.csv:**
```csv
Function,Module,CPU Time,Clockticks
mpegts::MPEGTSDemuxer::tryFindValidIteration,mpegts_demuxer.lib,45.2%,1234567890
mpegts::TSPacket::parse,mpegts_demuxer.lib,12.3%,345678901
```

### 3. Windows Performance Toolkit (WPT) **Приоритет: HIGH**

#### Установка
```bash
# Входит в Windows SDK
# https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
```

#### Использование

```powershell
# Запуск трассировки
wpr -start CPU -start FileIO

# Запустить программу
.\test_demuxer_basic.exe

# Остановить трассировку
wpr -stop trace.etl

# Анализ в Windows Performance Analyzer (WPA) - GUI
# Или конвертация в текст:
xperf -i trace.etl -o trace.txt -a process -a profile
```

#### Формат вывода

**trace.txt:**
```
CPU Usage by Process:
Process,CPU Usage (%)
test_demuxer_basic.exe,98.5

Top Functions:
Function,Module,Weight (%)
mpegts::MPEGTSDemuxer::tryFindValidIteration,mpegts_demuxer.dll,45.2
mpegts::TSPacket::parse,mpegts_demuxer.dll,12.3
```

### 4. Very Sleepy (бесплатный профайлер) **Приоритет: MEDIUM**

#### Установка
```bash
# Скачать: http://www.codersnotes.com/sleepy/
# Portable версия - не требует установки
```

#### Использование
```
1. Запустить VerySleepy.exe
2. File → Launch Executable → test_demuxer_basic.exe
3. Дождаться завершения программы
4. File → Export → CSV
```

#### Формат вывода

**sleepy_export.csv:**
```csv
Function,Exclusive %,Inclusive %,Module,Samples
mpegts::MPEGTSDemuxer::tryFindValidIteration,45.23,45.23,mpegts_demuxer.lib,4523
mpegts::TSPacket::parse,12.34,12.34,mpegts_demuxer.lib,1234
```

### 5. Dr. Memory (аналог valgrind) **Приоритет: HIGH**

#### Установка
```bash
# Скачать: https://drmemory.org/
# Установщик для Windows
```

#### Использование

```powershell
# Memory leak detection
drmemory.exe -batch -logdir ./drmemory_results -- test_demuxer_basic.exe

# Результаты в: ./drmemory_results/results.txt
```

#### Формат вывода

**results.txt:**
```
Dr. Memory version 2.5.0 build 0
ERRORS FOUND:
0 unique,     0 total unaddressable access(es)
0 unique,     0 total uninitialized access(es)
0 unique,     0 total invalid heap argument(s)
0 unique,     0 total leak(s)

HEAP SUMMARY:
   total allocations:    1,234
   total frees:          1,234
   peak memory usage:    5.67 MB
```

---

## 🔄 Workflow Профилирования

### Рекомендуемая последовательность

#### Linux: 4 ключевых инструмента
```bash
# 1. perf для CPU hotspots (5 минут)
perf record -g ./test_demuxer_basic
perf report --stdio > perf_report.txt

# 2. valgrind cachegrind для cache анализа (10 минут)
valgrind --tool=cachegrind ./test_demuxer_basic
cg_annotate cachegrind.out > cachegrind_report.txt

# 3. valgrind callgrind для call graph (10 минут)
valgrind --tool=callgrind ./test_demuxer_basic
callgrind_annotate callgrind.out > callgrind_report.txt

# 4. valgrind memcheck для memory leaks (5 минут)
valgrind --leak-check=full ./test_demuxer_basic > valgrind_memcheck.txt 2>&1
```

#### Windows: 3 ключевых инструмента
```powershell
# 1. Visual Studio Profiler для CPU hotspots (GUI, 5 минут)
# Debug → Performance Profiler → CPU Usage → Start
# Export → CSV

# 2. Dr. Memory для memory leaks (10 минут)
drmemory.exe -batch -logdir ./drmemory_results -- test_demuxer_basic.exe

# 3. Very Sleepy для быстрого profiling (5 минут)
# Launch Executable → test_demuxer_basic.exe
# Export → CSV
```

### Автоматизация через скрипты

Используйте готовые скрипты из папки `scripts/`:

**Linux:**
```bash
cd /path/to/minimal_mpegts_std_impl
chmod +x scripts/profiling_runner_linux.sh
./scripts/profiling_runner_linux.sh

# Создаст архив:
tar czf profiling_results.tar.gz profiling_results/
```

**Windows:**
```powershell
cd C:\path\to\minimal_mpegts_std_impl
powershell -ExecutionPolicy Bypass -File scripts\profiling_runner_windows.ps1

# Создаст архив:
Compress-Archive -Path profiling_results -DestinationPath profiling_results.zip
```

---

## 📦 Форматы данных для передачи

### Что передавать Claude для анализа

#### Минимальный набор (Linux):
1. **perf_report.txt** - CPU hotspots
2. **cachegrind_report.txt** - Cache miss rates
3. **callgrind_report.txt** - Call graph

#### Минимальный набор (Windows):
1. **profile_report.csv** (Visual Studio) - CPU hotspots
2. **drmemory_results.txt** - Memory leaks
3. **sleepy_export.csv** (Very Sleepy) - Function timing

### Способы передачи

1. **Copy-Paste**
   ```bash
   # Вывести содержимое файла
   cat perf_report.txt
   # Скопировать и вставить в чат
   ```

2. **Архив**
   ```bash
   # Linux
   tar czf profiling_results.tar.gz profiling_results/

   # Windows
   Compress-Archive -Path profiling_results -DestinationPath profiling_results.zip

   # Передать файл через file sharing
   ```

3. **Text в чате**
   ```bash
   # Если файл небольшой
   cat perf_report.txt | head -50
   # Отправить первые 50 строк
   ```

---

## 🔍 Интерпретация результатов

### Что искать Claude при анализе

#### 1. CPU Hotspots (perf, Visual Studio)
```
✅ Смотреть на функции с Overhead > 5%
✅ Искать неожиданные hotspots (напр. strlen в цикле)
✅ Проверить, можно ли оптимизировать top-3 функции
```

**Пример:**
```
45.23%  mpegts::MPEGTSDemuxer::tryFindValidIteration
        ↑ HOTSPOT! Нужна оптимизация (SIMD для sync byte search)

12.34%  mpegts::TSPacket::parse
        ↑ Нормально, это основная работа

1.23%   std::vector::insert
        ↑ Слишком много! Возможно, zero-copy решит
```

#### 2. Cache Misses (cachegrind)
```
✅ L1 cache miss rate > 5% - плохо
✅ L2 cache miss rate > 10% - очень плохо
✅ Смотреть на data locality
```

**Пример:**
```
Ir          I1mr  D1mr   DLmr   Function
12,345,678   123   8900   1200   tryFindValidIteration
                    ↑      ↑
                  7.2%   9.7%  - ПЛОХО! Data locality issue
```

#### 3. Memory Leaks (valgrind, Dr. Memory)
```
✅ definitely lost - КРИТИЧНО
✅ possibly lost - проверить
✅ Peak memory usage - оптимизировать если > 100MB
```

#### 4. Call Graph (callgrind)
```
✅ Количество вызовов функций
✅ Глубина call stack
✅ Expensive функции внизу стека
```

### Типичные находки и решения

| Проблема | Признак | Решение |
|----------|---------|---------|
| Медленный поиск sync byte | tryFindValidIteration 40%+ CPU | SIMD (SSE4.2/AVX2) |
| Много копирований | vector::insert 5%+ CPU | Zero-copy architecture |
| Cache misses | D1mr > 5% | Improve data locality |
| Много аллокаций | malloc/free в top-10 | Memory pool |
| Branch mispredictions | branch-misses > 10% | Branch hints |

---

## 📝 Шаблон для отчёта

### Когда передаёте результаты Claude

Используйте этот шаблон:

```markdown
## Profiling Results

**Platform:** Linux Ubuntu 22.04 / Windows 11
**Date:** 2025-11-12
**Build:** RelWithDebInfo
**CPU:** Intel i7-9700K / AMD Ryzen 7

### CPU Hotspots (perf/Visual Studio)

[paste here the top 20 functions with overhead %]

### Cache Analysis (cachegrind)

[paste here cache miss rates]

### Memory Leaks (valgrind/Dr. Memory)

[paste here leak summary]

### Request

Please analyze these results and suggest optimizations for:
1. CPU hotspots
2. Cache efficiency
3. Memory usage
```

---

## ✅ Чеклист перед передачей результатов

- [ ] Скомпилировать проект с `-O2 -g` (RelWithDebInfo)
- [ ] Запустить минимум 3 инструмента:
  - [ ] perf/Visual Studio (CPU)
  - [ ] cachegrind/VTune (cache)
  - [ ] valgrind memcheck/Dr. Memory (memory)
- [ ] Экспортировать в текстовые файлы
- [ ] Создать summary.json с метаданными
- [ ] Упаковать в архив (опционально)
- [ ] Подготовить краткое описание проблемы

---

## 🚀 Быстрый старт

### Linux (5 команд)

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build .
chmod +x ../scripts/profiling_runner_linux.sh
../scripts/profiling_runner_linux.sh
```

### Windows (3 команды)

```powershell
cd build
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build . --config RelWithDebInfo
..\scripts\profiling_runner_windows.ps1
```

Результаты будут в `profiling_results/`

---

**Готово к использованию!** 🎯
