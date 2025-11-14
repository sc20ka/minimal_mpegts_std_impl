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
    gprof \
    google-perftools \
    libgoogle-perftools-dev

# Дополнительные утилиты
sudo apt-get install -y \
    sysstat \
    htop \
    iotop \
    strace
```

### 1. perf (Linux Performance Tool)

#### Установка
```bash
sudo apt-get install linux-tools-common linux-tools-$(uname -r)
```

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
perf report --stdio > perf_detailed.txt
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

```bash
# Создать полный отчёт
perf record -g ./benchmark
perf report --stdio --no-children -n --percent-limit 1 > perf_full_report.txt

# Структура файла perf_full_report.txt:
# - Overhead % (процент времени в функции)
# - Samples (количество семплов)
# - Command (программа)
# - Shared Object (библиотека)
# - Symbol (имя функции)
```

### 2. valgrind (Memory Profiler)

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

**C. Cache profiling (cachegrind)**
```bash
valgrind --tool=cachegrind \
         --cachegrind-out-file=cachegrind.out \
         ./tests/test_demuxer_basic

# Конвертация в текст
cg_annotate cachegrind.out > cachegrind_report.txt
```

**D. Call graph (callgrind)**
```bash
valgrind --tool=callgrind \
         --callgrind-out-file=callgrind.out \
         ./tests/test_demuxer_basic

# Конвертация в текст
callgrind_annotate callgrind.out > callgrind_report.txt
```

#### Формат вывода для передачи

```bash
# memcheck
# - Leak summary (lost bytes, blocks)
# - Stack traces для каждой утечки

# massif
# - Heap profile over time
# - Peak memory usage
# - Allocation call stacks

# cachegrind
# - Cache miss rates (L1, L2, LL)
# - Per-function cache statistics

# callgrind
# - Call counts
# - Instruction counts per function
# - Call graph
```

### 3. gprof (GNU Profiler)

#### Использование

```bash
# Компиляция с -pg флагом
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-pg" \
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

```
Flat profile:
  %time - процент времени
  cumulative seconds - накопительное время
  self seconds - время только в функции
  calls - количество вызовов

Call graph:
  - Кто вызывал функцию
  - Кого вызывала функция
  - Время в каждом вызове
```

### 4. time (Simple Benchmarking)

```bash
# Детальная статистика
/usr/bin/time -v ./tests/test_demuxer_basic > time_report.txt 2>&1

# Включает:
# - User time, System time, Wall time
# - Maximum resident set size (memory)
# - Page faults
# - Context switches
```

### 5. strace (System Calls)

```bash
# Трассировка системных вызовов
strace -c -o strace_summary.txt ./tests/test_demuxer_basic

# Детальная трассировка
strace -tt -T -o strace_detailed.txt ./tests/test_demuxer_basic
```

---

## 🪟 Инструменты для Windows 11 x64

### 1. Visual Studio Profiler

#### Установка
- Visual Studio 2022 Community (бесплатно)
- Или Visual Studio 2019 Professional

#### Использование

**A. Performance Profiler (GUI)**

```powershell
# Компиляция Release с отладкой
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config RelWithDebInfo

# Запуск профайлера
# Visual Studio → Debug → Performance Profiler
# Выбрать:
# - CPU Usage
# - Memory Usage
# - .NET Object Allocation

# Экспорт результатов
# File → Export → Export as .diagsession or .csv
```

**B. Command Line Profiler (VSPerfCmd)**

```powershell
# Инструментация
VSInstr.exe test_demuxer_basic.exe

# Запуск профилирования
VSPerfCmd /start:trace /output:profile.vsp
test_demuxer_basic.exe
VSPerfCmd /shutdown

# Конвертация в XML/CSV
VSPerfReport profile.vsp /summary:all /output:profile_report.csv
```

#### Формат вывода

```
CSV файл с колонками:
- Function Name
- Total CPU Time
- Self CPU Time
- Module
- Call Count
```

### 2. Intel VTune Profiler (опционально)

```bash
# Для Intel процессоров
# Скачать: https://software.intel.com/content/www/us/en/develop/tools/oneapi/components/vtune-profiler.html

# Использование
vtune -collect hotspots -result-dir vtune_results ./test_demuxer_basic.exe
vtune -report hotspots -result-dir vtune_results -format csv -report-output vtune_report.csv
```

### 3. Windows Performance Toolkit (WPT)

```powershell
# Установка (входит в Windows SDK)
# Скачать: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/

# Запуск трассировки
wpr -start CPU -start FileIO

# Запустить программу
.\test_demuxer_basic.exe

# Остановить трассировку
wpr -stop trace.etl

# Анализ в Windows Performance Analyzer (WPA)
# Или конвертация в текст:
xperf -i trace.etl -o trace.txt
```

### 4. Very Sleepy (бесплатный профайлер)

```bash
# Скачать: http://www.codersnotes.com/sleepy/

# Использование:
# 1. Запустить Very Sleepy
# 2. File → Launch Executable → test_demuxer_basic.exe
# 3. Дождаться завершения
# 4. File → Export → CSV

# Формат: Function Name, Exclusive %, Inclusive %, Module
```

### 5. Dr. Memory (аналог valgrind для Windows)

```powershell
# Установка
# Скачать: https://drmemory.org/

# Использование
drmemory.exe -batch -no_summary -- test_demuxer_basic.exe

# Результаты в: results.txt
# Включает:
# - Memory leaks
# - Uninitialized reads
# - Invalid heap arguments
```

---

## 🔄 Workflow Профилирования

### Шаг 1: Подготовка на внешней машине

```bash
# Linux
cd /path/to/minimal_mpegts_std_impl
mkdir -p profiling_results

# Компиляция для профилирования
cd build
rm -rf *
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_CXX_FLAGS="-g -O2 -fno-omit-frame-pointer" \
      -DBUILD_TESTS=ON ..
cmake --build . -j$(nproc)
```

```powershell
# Windows
cd C:\path\to\minimal_mpegts_std_impl
mkdir profiling_results

# Компиляция
cd build
cmake -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=ON ..
cmake --build . --config RelWithDebInfo
```

### Шаг 2: Создание benchmark runner

<function_calls>
<invoke name="Write">
<parameter name="content">#!/bin/bash
# profiling_runner_linux.sh
# Автоматический запуск всех профайлеров

BENCHMARK_BIN="./tests/test_demuxer_basic"
OUTPUT_DIR="./profiling_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

mkdir -p "$OUTPUT_DIR"

echo "=== Starting profiling session: $TIMESTAMP ==="

# 1. perf CPU profiling
echo "[1/7] Running perf CPU profiling..."
perf record -g -o "$OUTPUT_DIR/perf_${TIMESTAMP}.data" $BENCHMARK_BIN
perf report --stdio -i "$OUTPUT_DIR/perf_${TIMESTAMP}.data" > "$OUTPUT_DIR/perf_report_${TIMESTAMP}.txt"

# 2. perf cache analysis
echo "[2/7] Running perf cache analysis..."
perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses \
    $BENCHMARK_BIN > "$OUTPUT_DIR/perf_cache_${TIMESTAMP}.txt" 2>&1

# 3. valgrind memcheck
echo "[3/7] Running valgrind memcheck..."
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --log-file="$OUTPUT_DIR/valgrind_memcheck_${TIMESTAMP}.txt" \
         $BENCHMARK_BIN

# 4. valgrind cachegrind
echo "[4/7] Running valgrind cachegrind..."
valgrind --tool=cachegrind \
         --cachegrind-out-file="$OUTPUT_DIR/cachegrind_${TIMESTAMP}.out" \
         $BENCHMARK_BIN
cg_annotate "$OUTPUT_DIR/cachegrind_${TIMESTAMP}.out" > "$OUTPUT_DIR/cachegrind_report_${TIMESTAMP}.txt"

# 5. valgrind callgrind
echo "[5/7] Running valgrind callgrind..."
valgrind --tool=callgrind \
         --callgrind-out-file="$OUTPUT_DIR/callgrind_${TIMESTAMP}.out" \
         $BENCHMARK_BIN
callgrind_annotate "$OUTPUT_DIR/callgrind_${TIMESTAMP}.out" > "$OUTPUT_DIR/callgrind_report_${TIMESTAMP}.txt"

# 6. time statistics
echo "[6/7] Running time statistics..."
/usr/bin/time -v $BENCHMARK_BIN > "$OUTPUT_DIR/time_${TIMESTAMP}.txt" 2>&1

# 7. Create summary JSON
echo "[7/7] Creating summary..."
cat > "$OUTPUT_DIR/summary_${TIMESTAMP}.json" << EOF
{
  "timestamp": "$TIMESTAMP",
  "platform": "linux",
  "hostname": "$(hostname)",
  "cpu": "$(lscpu | grep 'Model name' | cut -d: -f2 | xargs)",
  "files": [
    "perf_report_${TIMESTAMP}.txt",
    "perf_cache_${TIMESTAMP}.txt",
    "valgrind_memcheck_${TIMESTAMP}.txt",
    "cachegrind_report_${TIMESTAMP}.txt",
    "callgrind_report_${TIMESTAMP}.txt",
    "time_${TIMESTAMP}.txt"
  ]
}
EOF

echo "=== Profiling complete! ==="
echo "Results saved to: $OUTPUT_DIR"
echo ""
echo "To transfer results:"
echo "  tar czf profiling_${TIMESTAMP}.tar.gz $OUTPUT_DIR"
echo ""
echo "Files to copy:"
ls -lh "$OUTPUT_DIR"/*_${TIMESTAMP}.txt
