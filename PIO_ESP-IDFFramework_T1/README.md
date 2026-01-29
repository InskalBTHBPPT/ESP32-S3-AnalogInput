# ESP32-S3 Analog Input Demo - ESP-IDF Framework

Demo pembacaan ADC pada ESP32-S3 dengan **1 channel dari group ADC1**, menggunakan **2 versi pembacaan** dan masing-masing memiliki **3 metode averaging**.

---

## 📋 Daftar Isi

- [Overview](#overview)
- [Spesifikasi Teknis](#spesifikasi-teknis)
- [Arsitektur Program](#arsitektur-program)
- [Fitur Utama](#fitur-utama)
- [Konfigurasi Hardware](#konfigurasi-hardware)
- [Cara Menggunakan](#cara-menggunakan)
- [Format Output](#format-output)
- [Kesimpulan & Temuan](#kesimpulan--temuan)
- [Catatan Penting](#catatan-penting)
- [Referensi](#referensi)

---

## Overview

Proyek ini mendemonstrasikan pembacaan analog input pada ESP32-S3 dengan:
- **1 GPIO** (GPIO4 - ADC1_CH3) sebagai input analog
- **2 Versi Pembacaan**:
  - Versi A: `adc_oneshot_read()` - Nilai raw (0-4095), konversi manual ke Volt
  - Versi B: `adc_cali_raw_to_voltage()` - Nilai terkalibrasi dalam mV
- **3 Metode Averaging** untuk setiap versi:
  - Direct (tanpa averaging)
  - Averaging 5 sample terakhir
  - Averaging 10 sample terakhir
- **Format Output**: CSV untuk analisis data
- **Framework**: ESP-IDF dengan FreeRTOS tasks

---

## Spesifikasi Teknis

| Parameter | Nilai |
|-----------|-------|
| **Board** | ESP32-S3-DevKitC-1-N16R8 |
| **Framework** | ESP-IDF |
| **IDE** | PlatformIO di Cursor IDE |
| **Pin ADC** | GPIO4 (ADC1_CH3) |
| **ADC Unit** | ADC1 (1 channel dari group ADC1) |
| **Resolusi ADC** | 12-bit (0-4095) |
| **Attenuation** | ADC_ATTEN_DB_12 (0-3100mV untuk ESP32-S3) |
| **Rentang Input** | 0 - 3.1V |
| **Interval Pembacaan** | 100ms |
| **Baudrate Serial** | 115200 |
| **Task Stack Size** | 4096 bytes |
| **Task Priority** | 5 |

---

## Arsitektur Program

### Struktur Kode

```
main.c
├── ADC Initialization
│   ├── adc_init() - Inisialisasi ADC1 oneshot mode
│   └── adc_calibration_init() - Inisialisasi ADC calibration (Curve Fitting)
├── Circular Buffer Management
│   ├── addRawSample() - Tambah sample raw ke buffer
│   ├── addMvSample() - Tambah sample mV ke buffer
│   ├── getRawAverage() - Hitung rata-rata raw (5 atau 10 sample)
│   └── getMvAverage() - Hitung rata-rata mV (5 atau 10 sample)
├── Conversion Functions
│   └── rawToVoltage() - Konversi raw ADC ke Volt (manual)
├── Output Functions
│   ├── printHeader() - Tampilkan header CSV
│   └── printResults() - Tampilkan hasil pembacaan dalam format CSV
└── FreeRTOS Task
    └── adc_read_task() - Task utama untuk pembacaan ADC
```

### Flow Diagram

```
app_main()
  ├── Initialize Buffers
  ├── adc_init() → ADC1 oneshot mode
  ├── adc_calibration_init() → Curve Fitting calibration
  ├── printHeader() → CSV header
  └── xTaskCreate(adc_read_task)

adc_read_task() [Setiap 100ms]
  ├── adc_oneshot_read() → rawDirect
  ├── addRawSample(rawDirect)
  ├── getRawAverage(5) → rawAvg5
  ├── getRawAverage(10) → rawAvg10
  ├── rawToVoltage() → voltDirect, voltAvg5, voltAvg10
  ├── adc_cali_raw_to_voltage() → mVDirect
  ├── addMvSample(mVDirect)
  ├── getMvAverage(5) → mVAvg5
  ├── getMvAverage(10) → mVAvg10
  └── printResults() → Output CSV
```

---

## Fitur Utama

### 1. **Dua Versi Pembacaan ADC**

#### Versi A: `adc_oneshot_read()` - Raw Value
- Mengembalikan nilai raw ADC (0-4095 untuk 12-bit)
- Tidak terkalibrasi
- Konversi ke Volt dilakukan secara manual: `Volt = (raw * 3.3) / 4095`
- Cocok untuk aplikasi yang membutuhkan kontrol penuh atas konversi

#### Versi B: `adc_cali_raw_to_voltage()` - Calibrated Value
- Mengembalikan nilai terkalibrasi dalam mV (0-3100mV)
- Menggunakan **Curve Fitting Scheme** untuk kalibrasi
- Memanfaatkan data eFuse internal chip untuk akurasi tinggi
- Lebih akurat karena memperhitungkan karakteristik non-linear ADC

### 2. **Tiga Metode Averaging**

Setiap versi memiliki 3 output:

1. **Direct** - Pembacaan langsung tanpa averaging
2. **Avg5** - Rata-rata dari 5 sample terakhir (sliding window)
3. **Avg10** - Rata-rata dari 10 sample terakhir (sliding window)

**Implementasi Circular Buffer:**
- Buffer berukuran 10 elemen untuk menyimpan history sample
- Menggunakan indeks circular untuk efisiensi memori
- Sliding window: selalu mengambil N sample terakhir

### 3. **FreeRTOS Task Management**

- Menggunakan `vTaskDelayUntil()` untuk timing yang akurat
- Mencegah task watchdog timeout dengan memberikan waktu CPU untuk IDLE task
- Non-blocking timing dengan interval 100ms yang konsisten

### 4. **ADC Calibration**

- Menggunakan **Curve Fitting Scheme** untuk ESP32-S3
- Otomatis fallback ke konversi manual jika kalibrasi tidak tersedia
- Kalibrasi menggunakan data eFuse internal chip

---

## Konfigurasi Hardware

### Pin Configuration

| Pin | Fungsi | Keterangan |
|-----|--------|------------|
| GPIO4 | ADC1_CH3 | Input analog (0-3.1V) |

### Pin ADC1 yang Aman Digunakan

- ✅ **GPIO1, GPIO2, GPIO4-GPIO10** - Aman digunakan
- ❌ **GPIO3** - Hindari (strapping pin)
- ⚠️ **GPIO11-20 (ADC2)** - Tidak bisa digunakan saat WiFi aktif

### Koneksi Hardware

```
PSU DC (0-3.1V) ──┬── GPIO4 (ADC1_CH3)
                 │
                 └── GND ── ESP32-S3 GND
```

**Catatan:** Pastikan tegangan input tidak melebihi 3.1V untuk menghindari kerusakan ADC.

---

## Cara Menggunakan

### 1. Prasyarat

- PlatformIO terinstall
- ESP-IDF framework terinstall
- ESP32-S3 board terhubung ke komputer via USB
- Serial monitor siap untuk melihat output

### 2. Build & Upload

```bash
# Build project
pio run

# Upload ke board
pio run --target upload

# Monitor serial output
pio device monitor
```

### 3. Konfigurasi Port (jika diperlukan)

Edit `platformio.ini`:
```ini
upload_port = COM22  # Sesuaikan dengan port Anda
monitor_speed = 115200
```

### 4. Output Serial

Setelah upload, serial monitor akan menampilkan:

1. **Header informasi** tentang konfigurasi ADC
2. **CSV header** dengan nama kolom
3. **Data CSV** setiap 100ms dengan format:
   ```
   Timestamp(ms),Raw_Direct,Raw_Avg5,Raw_Avg10,Volt_Direct,Volt_Avg5,Volt_Avg10,mV_Direct,mV_Avg5,mV_Avg10
   ```

---

## Format Output

### CSV Format

Output menggunakan format CSV dengan **10 kolom**:

| Kolom | Tipe | Deskripsi |
|-------|------|-----------|
| `Timestamp(ms)` | uint32_t | Timestamp dalam milidetik sejak boot |
| `Raw_Direct` | int | Nilai ADC raw langsung (0-4095) |
| `Raw_Avg5` | int | Rata-rata 5 sample terakhir dari raw |
| `Raw_Avg10` | int | Rata-rata 10 sample terakhir dari raw |
| `Volt_Direct` | float | Konversi raw langsung ke Volt (3 desimal) |
| `Volt_Avg5` | float | Konversi raw avg5 ke Volt (3 desimal) |
| `Volt_Avg10` | float | Konversi raw avg10 ke Volt (3 desimal) |
| `mV_Direct` | uint32_t | Nilai terkalibrasi langsung dalam mV |
| `mV_Avg5` | uint32_t | Rata-rata 5 sample terakhir dari mV |
| `mV_Avg10` | uint32_t | Rata-rata 10 sample terakhir dari mV |

### Contoh Output

```
Timestamp(ms),Raw_Direct,Raw_Avg5,Raw_Avg10,Volt_Direct,Volt_Avg5,Volt_Avg10,mV_Direct,mV_Avg5,mV_Avg10
100,2048,2048,2048,1.650,1.650,1.650,1650,1650,1650
200,2049,2048,2048,1.653,1.651,1.651,1653,1651,1651
300,2050,2049,2049,1.655,1.653,1.653,1655,1653,1653
```

---

## Kesimpulan & Temuan

### Perbandingan Versi A vs Versi B

1. **Akurasi:**
   - **Versi B (Calibrated)** lebih akurat karena menggunakan kalibrasi internal chip
   - Selisih dengan nilai PSU aktual biasanya < 0.01V
   - **Versi A (Manual)** menggunakan asumsi VREF = 3.3V yang mungkin tidak akurat

2. **Stabilitas:**
   - Averaging mengurangi noise dan fluktuasi
   - Avg10 lebih stabil daripada Avg5
   - Direct reading lebih responsif terhadap perubahan cepat

3. **Penggunaan:**
   - **Versi A**: Cocok untuk aplikasi yang membutuhkan kontrol penuh atau kalibrasi custom
   - **Versi B**: Cocok untuk aplikasi yang membutuhkan akurasi tinggi tanpa kalibrasi manual

### Temuan Teknis

1. **ADC Calibration:**
   - ESP32-S3 menggunakan Curve Fitting Scheme untuk kalibrasi
   - Kalibrasi menggunakan data eFuse internal chip
   - Kalibrasi memperhitungkan karakteristik non-linear ADC

2. **Task Management:**
   - Menggunakan `vTaskDelayUntil()` memberikan timing yang lebih akurat
   - Mencegah task watchdog timeout dengan memberikan waktu CPU untuk IDLE task
   - Timing konsisten tanpa drift

3. **Circular Buffer:**
   - Efisien dalam penggunaan memori
   - Sliding window selalu mengambil N sample terakhir
   - Tidak perlu memindahkan data, hanya update indeks

---

## Catatan Penting

### 1. **ADC Channel Configuration**
- Menggunakan **1 channel dari group ADC1** (GPIO4 = ADC1_CH3)
- ADC1 aman digunakan karena tidak bersinggungan dengan WiFi
- ADC2 tidak digunakan karena bisa konflik dengan WiFi

### 2. **WiFi dan Bluetooth**
- **WiFi dan Bluetooth tidak di-disable** tapi **tidak diaktifkan**
- Framework ESP-IDF tidak mengaktifkan WiFi/Bluetooth secara default
- Jika WiFi/Bluetooth diaktifkan, hindari penggunaan ADC2

### 3. **Framework yang Digunakan**
- Framework: **ESP-IDF** (bukan Arduino)
- Build system: **PlatformIO** dengan ESP-IDF framework
- IDE: **Cursor IDE** dengan PlatformIO extension

### 4. **Task Watchdog**
- Menggunakan `vTaskDelayUntil()` untuk mencegah watchdog timeout
- Memberikan waktu CPU yang cukup untuk IDLE task
- Timing akurat tanpa blocking CPU terlalu lama

### 5. **ADC Calibration**
- Kalibrasi menggunakan Curve Fitting Scheme
- Otomatis fallback ke konversi manual jika kalibrasi tidak tersedia
- Kalibrasi memerlukan data eFuse yang sudah di-program di factory

### 6. **Input Voltage Range**
- Rentang input: **0 - 3.1V** (dengan ADC_ATTEN_DB_12)
- Jangan melebihi 3.1V untuk menghindari kerusakan ADC
- Pastikan sumber tegangan stabil dan tidak berisik

---

## Referensi

### Dokumentasi Resmi

- [ESP-IDF ADC API Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/adc/index.html)
- [ESP-IDF ADC Calibration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/adc_calibration.html)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [FreeRTOS API Reference](https://www.freertos.org/a00106.html)

### PlatformIO

- [PlatformIO ESP-IDF Documentation](https://docs.platformio.org/en/latest/frameworks/espidf.html)
- [PlatformIO Project Configuration](https://docs.platformio.org/page/projectconf.html)

### ADC Pin Mapping ESP32-S3

- ADC1 Channels: GPIO1, GPIO2, GPIO4-GPIO10
- ADC2 Channels: GPIO11-GPIO20 (tidak bisa digunakan saat WiFi aktif)
- Hindari GPIO3 (strapping pin)

---

## Lisensi

Proyek ini dibuat untuk tujuan edukasi dan demonstrasi. Silakan gunakan dan modifikasi sesuai kebutuhan.

---

**Dibuat dengan PlatformIO + ESP-IDF Framework**
