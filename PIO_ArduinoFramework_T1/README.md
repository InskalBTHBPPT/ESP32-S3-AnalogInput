# ESP32-S3 Analog Input Demo

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
  - Versi A: `analogRead()` - Nilai raw (0-4095), konversi manual ke Volt
  - Versi B: `analogReadMilliVolts()` - Nilai terkalibrasi dalam mV
- **3 Metode Averaging** untuk setiap versi:
  - Direct (tanpa averaging)
  - Averaging 5 sample terakhir
  - Averaging 10 sample terakhir
- **Format Output**: CSV untuk analisis data
- **VREF Calculation**: Menghitung VREF efektif dari perbandingan raw dan mV

---

## Spesifikasi Teknis

| Parameter | Nilai |
|-----------|-------|
| **Board** | ESP32-S3-DevKitC-1-N16R8 |
| **Framework** | Arduino-ESP32 |
| **IDE** | PlatformIO di Cursor IDE |
| **Pin ADC** | GPIO4 (ADC1_CH3) |
| **ADC Unit** | ADC1 (1 channel dari group ADC1) |
| **Resolusi ADC** | 12-bit (0-4095) |
| **Rentang Input** | 0 - 3.1V (ADC_11db attenuation) |
| **Interval Pembacaan** | 100ms |
| **Baudrate Serial** | 115200 |
| **WiFi/Bluetooth** | Tidak di-disable, tidak diaktifkan |

---

## Arsitektur Program

```
GPIO4 (ADC1_CH3)
    │
    ├──► analogRead() ──────────┬──► Direct (raw)
    │   (Versi A)               ├──► Avg 5 sample
    │                           └──► Avg 10 sample
    │
    └──► analogReadMilliVolts() ┬──► Direct (mV)
        (Versi B)               ├──► Avg 5 sample
                                └──► Avg 10 sample
```

### Struktur Data

- **Circular Buffer**: 2 buffer terpisah (raw dan mV), masing-masing 10 sample
- **Non-blocking Loop**: Menggunakan `millis()` untuk interval 100ms
- **VREF Calculation**: Dihitung setiap 1 detik menggunakan avg10

---

## Fitur Utama

### 1. Dual Reading Method

#### Versi A: `analogRead()`
- Return: 0-4095 (raw value, tidak terkalibrasi)
- Konversi ke Volt: Manual (`raw × 3.3 / 4095`)
- **Akurasi**: ~0.1-0.15V error

#### Versi B: `analogReadMilliVolts()`
- Return: 0-3100 mV (terkalibrasi)
- Menggunakan kalibrasi internal chip (eFuse + polynomial curve fitting)
- **Akurasi**: ~0.01V error (selisih dengan PSU asli)

### 2. Triple Averaging Method

| Metode | Stabilitas | Responsivitas | Penggunaan |
|--------|-----------|---------------|------------|
| **Direct** | ⭐⭐ | ⭐⭐⭐⭐⭐ | Monitoring real-time cepat |
| **Avg 5** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | Kompromi stabilitas-responsivitas |
| **Avg 10** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | Pengukuran presisi |

### 3. Effective VREF Calculation

Menghitung VREF efektif dari perbandingan raw dan mV:
```
VREF_effective = (mV × 4095) / (raw × 1000)
```

**Catatan**: VREF efektif bervariasi tergantung input voltage karena kalibrasi internal menggunakan polynomial curve fitting, bukan VREF linier tunggal.

---

## Konfigurasi Hardware

### Wiring Diagram

```
PSU DC (0-3.1V)
    │
    ├──── (+) ────► GPIO4 (ADC Input)
    │
    └──── (GND) ──► GND ESP32-S3
```

> **⚠️ Peringatan:** Jangan berikan tegangan lebih dari 3.1V ke pin ADC!

### Pin ADC1 yang Aman

ESP32-S3 memiliki 2 unit ADC:
- **ADC1** (GPIO1-10): ✅ Aman digunakan kapan saja
- **ADC2** (GPIO11-20): ⚠️ Tidak bisa digunakan saat WiFi aktif

| GPIO | Channel | Status |
|------|---------|--------|
| GPIO1 | ADC1_CH0 | ✅ Aman |
| GPIO2 | ADC1_CH1 | ✅ Aman |
| **GPIO3** | ADC1_CH2 | ⚠️ **Strapping pin** - hindari |
| **GPIO4** | **ADC1_CH3** | ✅ **Digunakan** |
| GPIO5 | ADC1_CH4 | ✅ Aman |
| GPIO6 | ADC1_CH5 | ✅ Aman |
| GPIO7 | ADC1_CH6 | ✅ Aman |
| GPIO8 | ADC1_CH7 | ✅ Aman |
| GPIO9 | ADC1_CH8 | ✅ Aman |
| GPIO10 | ADC1_CH9 | ✅ Aman |

---

## Cara Menggunakan

### 1. Setup Project

- Buka folder ini di PlatformIO (Cursor IDE)
- Pastikan board terhubung ke COM port yang benar (cek di `platformio.ini`)

### 2. Upload & Monitor

```bash
# Upload ke board
pio run -t upload

# Monitor Serial
pio device monitor
```

### 3. Analisis Data

- Copy output CSV dari Serial Monitor
- Import ke Excel, Python pandas, atau tools analisis lainnya
- Format: `Timestamp(ms),Raw_Direct,Raw_Avg5,Raw_Avg10,Volt_Direct,Volt_Avg5,Volt_Avg10,mV_Direct,mV_Avg5,mV_Avg10,Effective_VREF`

---

## Format Output

### CSV Header

```csv
Timestamp(ms),Raw_Direct,Raw_Avg5,Raw_Avg10,Volt_Direct,Volt_Avg5,Volt_Avg10,mV_Direct,mV_Avg5,mV_Avg10,Effective_VREF
```

### Contoh Output

```csv
100,2048,2048,2048,1.650,1.650,1.650,1620,1620,1620,3.285
200,2052,2050,2050,1.653,1.651,1.651,1625,1622,1622,3.285
300,2040,2047,2047,1.644,1.649,1.649,1618,1619,1619,3.285
```

### Kolom Penjelasan

| Kolom | Deskripsi | Range |
|-------|-----------|-------|
| `Timestamp(ms)` | Waktu sejak boot (ms) | 0 - ... |
| `Raw_Direct` | ADC raw langsung | 0-4095 |
| `Raw_Avg5` | Rata-rata 5 sample raw | 0-4095 |
| `Raw_Avg10` | Rata-rata 10 sample raw | 0-4095 |
| `Volt_Direct` | Konversi manual raw → Volt | 0-3.3V |
| `Volt_Avg5` | Konversi manual avg5 → Volt | 0-3.3V |
| `Volt_Avg10` | Konversi manual avg10 → Volt | 0-3.3V |
| `mV_Direct` | Nilai terkalibrasi langsung | 0-3100 mV |
| `mV_Avg5` | Rata-rata 5 sample mV | 0-3100 mV |
| `mV_Avg10` | Rata-rata 10 sample mV | 0-3100 mV |
| `Effective_VREF` | VREF efektif yang dihitung | ~3.3-3.6V |

---

## Kesimpulan & Temuan

### 1. Akurasi Pembacaan

| Metode | Error vs PSU Asli | Kesimpulan |
|--------|-------------------|------------|
| **Versi A** (raw manual) | ~0.1-0.15V | Kurang akurat |
| **Versi B** (mV calibrated) | ~0.01V | **Sangat akurat** ✅ |

**Rekomendasi**: Gunakan **Versi B** (`analogReadMilliVolts()`) untuk aplikasi yang membutuhkan akurasi tinggi.

### 2. Efektivitas Averaging

- **Direct**: Fluktuatif, sangat responsif terhadap perubahan
- **Avg 5**: Kompromi baik antara stabilitas dan responsivitas
- **Avg 10**: Paling stabil, mengurangi noise dengan baik

**Rekomendasi**: 
- Monitoring cepat → Direct
- Pengukuran presisi → Avg 10
- Kompromi → Avg 5

### 3. VREF Effective

- VREF efektif bervariasi antara **3.3V - 3.6V** (bukan tetap 3.3V)
- Variasi ini **normal** karena:
  - Kalibrasi internal menggunakan **polynomial curve fitting**
  - Bukan VREF linier tunggal
  - Kompensasi non-linearitas ADC dan variasi chip

### 4. Proses Kalibrasi Internal

`analogReadMilliVolts()` menggunakan kalibrasi internal yang meliputi:

1. **eFuse Calibration Data**
   - Disimpan di factory (one-time programmable)
   - Berisi koefisien polynomial untuk setiap attenuation
   - Spesifik per chip

2. **Polynomial Curve Fitting**
   ```
   mV = a₀ + a₁×raw + a₂×raw² + a₃×raw³ + ...
   ```
   - Kompensasi variasi VREF internal (~1.1V, range 1000-1200mV)
   - Kompensasi non-linearitas ADC
   - Akurasi tinggi (~0.01V error)

3. **VREF Internal Chip**
   - Desain: 1100 mV
   - Aktual: 1000-1200 mV (variasi manufacturing)
   - **Bukan** 3.3V (3.3V adalah tegangan supply, bukan VREF ADC)

### 5. Perbandingan Versi A vs Versi B

| Aspek | Versi A (raw) | Versi B (mV) |
|-------|---------------|--------------|
| **Akurasi** | ~0.1-0.15V error | ~0.01V error ✅ |
| **Kalibrasi** | Tidak ada | Factory calibrated ✅ |
| **VREF** | Asumsi 3.3V | VREF aktual dari eFuse ✅ |
| **Non-linearitas** | Diabaikan | Dikompensasi ✅ |
| **Kompleksitas** | Sederhana | Otomatis ✅ |

---

## Catatan Penting

### 1. ADC Channel

- ✅ Menggunakan **1 channel dari group ADC1** (GPIO4 - ADC1_CH3)
- ✅ ADC1 aman digunakan kapan saja
- ⚠️ ADC2 tidak bisa digunakan saat WiFi aktif

### 2. WiFi & Bluetooth

- ✅ **Tidak di-disable** dalam kode
- ✅ **Tidak diaktifkan** (default state)
- ✅ Tidak mengganggu pembacaan ADC1
- ⚠️ Jika WiFi/Bluetooth diaktifkan, ADC2 tidak bisa digunakan

### 3. Framework & IDE

- ✅ **Framework**: Arduino-ESP32
- ✅ **IDE**: PlatformIO di Cursor IDE
- ✅ Kompatibel dengan Arduino API standar

### 4. VREF Calculation

- VREF efektif dihitung setiap **1 detik**
- Menggunakan **avg10** untuk stabilitas
- Ditampilkan di **setiap baris CSV** (menggunakan nilai terakhir)
- **Bervariasi** tergantung input voltage (normal behavior)

### 5. Format Output

- Format: **CSV** (Comma Separated Values)
- Mudah di-import ke Excel, Python, atau tools analisis lainnya
- Header di baris pertama
- Data real-time setiap 100ms

---

## Struktur Project

```
PIO_ArduinoFramework_T1/
├── README.md                    ← File ini
├── docs/
│   └── AnalogRead_README.md    ← Dokumentasi detail (Q&A lengkap)
├── platformio.ini
└── src/
    └── main.cpp                ← Source code utama
```

---

## Referensi

### Dokumentasi Resmi

- [Arduino-ESP32 ADC API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html)
- [ESP-IDF ADC Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/adc/index.html)
- [ESP-IDF ADC Calibration](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc_calibration.html)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

### Pinout Reference

- [ESP32-S3-DevKitC-1 Pinout](https://esp32.co.uk/esp32-s3-devkitc-1-pinout-gpio-reference-safe-pins-usb-adc-touch-i%C2%B2c-spi/)

### Dokumentasi Detail

- Lihat `docs/AnalogRead_README.md` untuk penjelasan detail setiap fungsi dan Q&A lengkap

---

**Dibuat dengan PlatformIO & Cursor IDE** 🚀
