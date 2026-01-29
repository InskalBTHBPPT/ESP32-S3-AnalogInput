# ESP32-S3 Analog Input Demo

Demo pembacaan ADC pada ESP32-S3 dengan **1 GPIO** dan **3 output berbeda**:
- Output 1: Pembacaan langsung (tanpa averaging)
- Output 2: Averaging 5 sample terakhir
- Output 3: Averaging 10 sample terakhir

---

## Daftar Isi

- [Konfigurasi Hardware](#konfigurasi-hardware)
- [Pin ADC1 yang Aman](#pin-adc1-yang-aman)
- [Penjelasan Kode](#penjelasan-kode)
  - [Variabel Global (Buffer System)](#1-variabel-global-buffer-system)
  - [setup()](#2-setup---inisialisasi)
  - [loop()](#3-loop---non-blocking-interval)
  - [addSample()](#4-addsample---tambah-ke-circular-buffer)
  - [getAverage()](#5-getaverage---hitung-rata-rata)
  - [adcToVoltage()](#6-adctovoltage---konversi-ke-volt)
  - [printResults()](#7-printresults---tampilkan-output)
- [Q&A](#qa)

---

## Konfigurasi Hardware

| Parameter | Nilai |
|-----------|-------|
| Board | ESP32-S3-DevKitC-1-N16R8 |
| Framework | Arduino |
| Pin ADC | GPIO4 (ADC1_CH3) |
| Resolusi ADC | 12-bit (0-4095) |
| Rentang Input | 0 - 3.3V |
| Interval Pembacaan | 100ms |
| Baudrate Serial | 115200 |

### Wiring

```
PSU DC (0-3.3V)
    │
    ├──── (+) ────► GPIO4 (ADC Input)
    │
    └──── (GND) ──► GND ESP32-S3
```

> **Peringatan:** Jangan berikan tegangan lebih dari 3.3V ke pin ADC!

---

## Pin ADC1 yang Aman

ESP32-S3 memiliki 2 unit ADC:
- **ADC1** (GPIO1-10): Aman digunakan kapan saja
- **ADC2** (GPIO11-20): **Tidak bisa digunakan saat WiFi aktif**

| GPIO | Channel | Status |
|------|---------|--------|
| GPIO1 | ADC1_CH0 | ✅ Aman |
| GPIO2 | ADC1_CH1 | ✅ Aman |
| **GPIO3** | ADC1_CH2 | ⚠️ **Strapping pin** - hindari |
| GPIO4 | ADC1_CH3 | ✅ Aman |
| GPIO5 | ADC1_CH4 | ✅ Aman |
| GPIO6 | ADC1_CH5 | ✅ Aman |
| GPIO7 | ADC1_CH6 | ✅ Aman |
| GPIO8 | ADC1_CH7 | ✅ Aman |
| GPIO9 | ADC1_CH8 | ✅ Aman |
| GPIO10 | ADC1_CH9 | ✅ Aman |

---

## Penjelasan Kode

### Arsitektur Program

```
GPIO4 ──► Baca ADC ──┬──► Output 1: Nilai langsung (rawDirect)
                     │
                     ├──► Output 2: Rata-rata 5 sample terakhir (rawAvg5)
                     │
                     └──► Output 3: Rata-rata 10 sample terakhir (rawAvg10)
```

---

### 1. Variabel Global (Buffer System)

```cpp
// Buffer untuk menyimpan sample
int sampleBuffer[SAMPLE_COUNT_10];  // Array 10 elemen
int sampleIndex = 0;                // Posisi tulis berikutnya
bool bufferFull = false;            // Flag buffer sudah penuh
```

**Penjelasan:**
- `sampleBuffer[10]` → Array untuk menyimpan 10 nilai ADC terakhir
- `sampleIndex` → Penunjuk posisi tulis berikutnya di buffer
- `bufferFull` → Flag apakah buffer sudah terisi penuh minimal 1x

**Visualisasi Circular Buffer:**

```
Awal (kosong):
Index:    [0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9]
Data:      0    0    0    0    0    0    0    0    0    0
           ↑
     sampleIndex = 0

Setelah 3 pembacaan:
Index:    [0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9]
Data:     2048 2050 2045  0    0    0    0    0    0    0
                          ↑
                    sampleIndex = 3
```

---

### 2. setup() - Inisialisasi

```cpp
void setup() {
  Serial.begin(115200);
  
  // Set resolusi ADC (9-12 bit, default 12-bit)
  analogReadResolution(ADC_RESOLUTION);
  
  // Set attenuation untuk rentang 0-3.3V
  analogSetAttenuation(ADC_11db);
  
  // Inisialisasi buffer dengan 0
  for (int i = 0; i < SAMPLE_COUNT_10; i++) {
    sampleBuffer[i] = 0;
  }
}
```

**Konfigurasi Attenuation:**

| Attenuation | Rentang Input |
|-------------|---------------|
| ADC_0db | 0 - 1.1V |
| ADC_2_5db | 0 - 1.5V |
| ADC_6db | 0 - 2.2V |
| **ADC_11db** | **0 - 3.3V** |

---

### 3. loop() - Non-blocking Interval

```cpp
void loop() {
  unsigned long currentMillis = millis();
  
  // Non-blocking interval menggunakan millis
  if (currentMillis - previousMillis >= READ_INTERVAL_MS) {
    previousMillis = currentMillis;
    
    // Baca nilai ADC dari satu GPIO
    int rawDirect = analogRead(ADC_PIN);
    
    // Simpan ke buffer untuk averaging
    addSample(rawDirect);
    
    // Hitung averaging
    int rawAvg5 = getAverage(SAMPLE_COUNT_5);
    int rawAvg10 = getAverage(SAMPLE_COUNT_10);
    
    // Tampilkan hasil
    printResults(rawDirect, rawAvg5, rawAvg10);
  }
}
```

**Penjelasan:**
- `millis()` → Waktu sejak ESP32 menyala (dalam ms)
- `currentMillis - previousMillis >= 100` → Cek apakah sudah 100ms berlalu
- **Tidak menggunakan `delay()`** → Program tidak terblokir

**Alur Eksekusi:**

```
millis() = 0      → Tidak eksekusi (0 - 0 < 100)
millis() = 50     → Tidak eksekusi (50 - 0 < 100)
millis() = 100    → EKSEKUSI! previousMillis = 100
millis() = 150    → Tidak eksekusi (150 - 100 < 100)
millis() = 200    → EKSEKUSI! previousMillis = 200
```

---

### 4. addSample() - Tambah ke Circular Buffer

```cpp
void addSample(int value) {
  sampleBuffer[sampleIndex] = value;
  sampleIndex++;
  
  if (sampleIndex >= SAMPLE_COUNT_10) {
    sampleIndex = 0;
    bufferFull = true;
  }
}
```

**Penjelasan:**
1. Simpan nilai di posisi `sampleIndex`
2. Naikkan index
3. Jika index mencapai 10, **reset ke 0** (circular/melingkar)
4. Set `bufferFull = true` → menandakan buffer sudah pernah penuh

**Visualisasi (setelah 12 pembacaan):**

```
Pembacaan ke-11 dan ke-12 menimpa index 0 dan 1:
Index:    [0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9]
Data:     d11  d12  d3   d4   d5   d6   d7   d8   d9   d10
               ↑
          sampleIndex = 2 (next write)
```

---

### 5. getAverage() - Hitung Rata-rata

```cpp
int getAverage(int count) {
  int availableSamples = bufferFull ? SAMPLE_COUNT_10 : sampleIndex;
  
  // Jika sample belum cukup, gunakan yang tersedia
  if (availableSamples < count) {
    count = availableSamples;
  }
  
  if (count == 0) {
    return 0;
  }
  
  long sum = 0;
  int startIdx = sampleIndex - count;
  
  for (int i = 0; i < count; i++) {
    // Circular buffer: handle index negatif
    int idx = (startIdx + i + SAMPLE_COUNT_10) % SAMPLE_COUNT_10;
    sum += sampleBuffer[idx];
  }
  
  return (int)(sum / count);
}
```

**Penjelasan Ternary Operator:**

```cpp
int availableSamples = bufferFull ? SAMPLE_COUNT_10 : sampleIndex;
```

Sama dengan:

```cpp
int availableSamples;
if (bufferFull == true) {
    availableSamples = 10;        // Buffer penuh
} else {
    availableSamples = sampleIndex; // Pakai data yang ada
}
```

**Contoh Perhitungan (Avg 5, sampleIndex = 2, bufferFull = true):**

```
startIdx = 2 - 5 = -3

Loop:
  i=0: idx = (-3 + 0 + 10) % 10 = 7  → sampleBuffer[7]
  i=1: idx = (-3 + 1 + 10) % 10 = 8  → sampleBuffer[8]
  i=2: idx = (-3 + 2 + 10) % 10 = 9  → sampleBuffer[9]
  i=3: idx = (-3 + 3 + 10) % 10 = 0  → sampleBuffer[0]
  i=4: idx = (-3 + 4 + 10) % 10 = 1  → sampleBuffer[1]

Hasil = (buf[7] + buf[8] + buf[9] + buf[0] + buf[1]) / 5
```

---

### 6. adcToVoltage() - Konversi ke Volt

```cpp
float adcToVoltage(int adcValue) {
  return (adcValue * ADC_VREF) / (float)((1 << ADC_RESOLUTION) - 1);
}
```

**Penjelasan:**
- `1 << 12` = 4096 (bit shift, sama dengan 2^12)
- `4096 - 1` = 4095 (nilai maksimum ADC 12-bit)
- **Rumus:** `Voltage = (ADC_Value × 3.3V) / 4095`

**Contoh:**

| ADC Value | Perhitungan | Hasil |
|-----------|-------------|-------|
| 0 | (0 × 3.3) / 4095 | 0.000V |
| 2048 | (2048 × 3.3) / 4095 | 1.650V |
| 4095 | (4095 × 3.3) / 4095 | 3.300V |

---

### 7. printResults() - Tampilkan Output

```cpp
void printResults(int rawDirect, int rawAvg5, int rawAvg10) {
  float voltDirect = adcToVoltage(rawDirect);
  float voltAvg5 = adcToVoltage(rawAvg5);
  float voltAvg10 = adcToVoltage(rawAvg10);
  
  Serial.printf("%13lu | %4d = %6.3fV     | %4d = %6.3fV     | %4d = %6.3fV\n",
                millis(), rawDirect, voltDirect, rawAvg5, voltAvg5, rawAvg10, voltAvg10);
}
```

**Format Specifier:**

| Specifier | Arti | Contoh |
|-----------|------|--------|
| `%13lu` | unsigned long, lebar 13 karakter | `          100` |
| `%4d` | integer, lebar 4 karakter | `2048` |
| `%6.3f` | float, lebar 6, 3 desimal | ` 1.650` |

**Contoh Output Serial Monitor:**

```
================================================
   ESP32-S3 ADC Demo - 1 GPIO, 3 Output
================================================
Pin ADC1: GPIO4
Output:
  1. Langsung (tanpa averaging)
  2. Averaging 5 sample
  3. Averaging 10 sample
================================================

Timestamp(ms) | Langsung            | Avg 5 Sample        | Avg 10 Sample
--------------|---------------------|---------------------|--------------------
          100 | 2048 =  1.650V     | 2048 =  1.650V     | 2048 =  1.650V
          200 | 2052 =  1.653V     | 2050 =  1.651V     | 2050 =  1.651V
          300 | 2040 =  1.644V     | 2047 =  1.649V     | 2047 =  1.649V
          400 | 2055 =  1.656V     | 2049 =  1.651V     | 2049 =  1.651V
```

---

## Q&A

### Q1: Kenapa ADC2 tidak bisa digunakan saat WiFi aktif?

**A:** Pada ESP32-S3, ADC2 berbagi resource hardware dengan modul WiFi. Ketika WiFi diaktifkan, ADC2 di-reserve oleh WiFi driver dan tidak bisa diakses untuk pembacaan analog. Gunakan **ADC1 (GPIO1-10)** jika aplikasi membutuhkan WiFi.

---

### Q2: Apa itu Circular Buffer dan kenapa digunakan?

**A:** Circular buffer adalah struktur data array yang "melingkar" - ketika mencapai akhir, index kembali ke awal dan menimpa data lama.

**Keuntungan:**
- Ukuran memory tetap (tidak perlu alokasi dinamis)
- Efisien untuk menyimpan N data terakhir
- Operasi O(1) untuk tambah data

```
Ilustrasi:
    ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
    │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │
    └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
      ↑                                   │
      └───────────── wrap around ─────────┘
```

---

### Q3: Jelaskan baris `int availableSamples = bufferFull ? SAMPLE_COUNT_10 : sampleIndex;`

**A:** Ini adalah **ternary operator** (operator kondisional).

**Sintaks:**
```
kondisi ? nilai_jika_true : nilai_jika_false
```

**Penjelasan:**
- Jika `bufferFull == true` → `availableSamples = 10` (buffer penuh)
- Jika `bufferFull == false` → `availableSamples = sampleIndex` (pakai data yang ada)

**Tabel:**

| Kondisi | bufferFull | sampleIndex | availableSamples |
|---------|------------|-------------|------------------|
| Baru 3 pembacaan | false | 3 | **3** |
| Baru 7 pembacaan | false | 7 | **7** |
| Sudah 10 pembacaan | true | 0 | **10** |
| Sudah 15 pembacaan | true | 5 | **10** |

---

### Q4: Apakah averaging selalu mengambil dari data terakhir?

**A:** **Ya!** Averaging selalu mengambil dari data **paling baru** (terakhir), bukan data acak atau data awal.

**Ilustrasi Sliding Window:**

```
Waktu: ───────────────────────────────────────────►

       [     Avg 10 window      ]
                    [  Avg 5   ]
d1  d2  d3  d4  d5  d6  d7  d8  d9  d10  d11  d12
                                              ↑
                                         data terbaru
```

**Keuntungan menggunakan data terakhir:**
- Lebih responsif terhadap perubahan tegangan saat ini
- Merepresentasikan kondisi terkini
- Data lama yang sudah "basi" tidak mempengaruhi hasil

---

### Q5: Kenapa menggunakan `millis()` bukan `delay()`?

**A:** 

| Aspek | `delay()` | `millis()` |
|-------|-----------|------------|
| Blocking | Ya, program berhenti | Tidak, program tetap jalan |
| Multitasking | Tidak bisa | Bisa |
| Responsif | Lambat | Cepat |
| Akurasi timing | Kurang (ada overhead) | Lebih baik |

**Contoh masalah `delay()`:**
```cpp
// Dengan delay() - BLOCKING
void loop() {
  readADC();      // 1ms
  delay(100);     // Program BERHENTI 100ms
  // Tidak bisa handle button, serial, dll selama delay
}

// Dengan millis() - NON-BLOCKING
void loop() {
  if (millis() - prev >= 100) {
    readADC();
  }
  handleButton();  // Tetap bisa dijalankan
  handleSerial();  // Tetap bisa dijalankan
}
```

---

### Q6: Bagaimana cara mengubah pin ADC?

**A:** Ubah nilai `ADC_PIN` di bagian konfigurasi:

```cpp
// Ganti dari GPIO4 ke GPIO5
#define ADC_PIN  5
```

Pastikan menggunakan pin ADC1 yang aman: GPIO1, GPIO2, GPIO4-GPIO10.

---

### Q7: Bagaimana cara mengubah interval pembacaan?

**A:** Ubah nilai `READ_INTERVAL_MS`:

```cpp
// Ubah dari 100ms ke 50ms (lebih cepat)
#define READ_INTERVAL_MS  50

// Atau ke 500ms (lebih lambat)
#define READ_INTERVAL_MS  500
```

---

## Referensi

- [ESP-IDF ADC Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/adc/index.html)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [ESP32-S3-DevKitC-1 Pinout Reference](https://esp32.co.uk/esp32-s3-devkitc-1-pinout-gpio-reference-safe-pins-usb-adc-touch-i%C2%B2c-spi/)
