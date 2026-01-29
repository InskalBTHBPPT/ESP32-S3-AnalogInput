/**
 * ESP32-S3 Analog Input Demo
 * 1 GPIO dengan 2 Versi Pembacaan:
 * 
 * Versi A: analogRead() - Nilai raw (0-4095), konversi manual ke Volt
 * Versi B: analogReadMilliVolts() - Nilai terkalibrasi dalam mV
 * 
 * Masing-masing versi memiliki 3 output:
 * - Output 1: Pembacaan langsung (tanpa averaging)
 * - Output 2: Averaging 5 sample terakhir
 * - Output 3: Averaging 10 sample terakhir
 * 
 * Pin ADC1 yang aman: GPIO1, GPIO2, GPIO4-GPIO10
 * Hindari GPIO3 (strapping pin)
 * ADC2 (GPIO11-20) tidak bisa digunakan saat WiFi aktif
 * 
 * Referensi: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html
 */

#include <Arduino.h>

// ============== KONFIGURASI PIN ADC1 ==============
#define ADC_PIN             4           // Satu GPIO untuk semua pembacaan

// ============== KONFIGURASI ADC ==============
#define ADC_RESOLUTION      12          // 12-bit (0-4095)
#define ADC_VREF            3.3         // Tegangan referensi (Volt)
#define SAMPLE_COUNT_5      5           // Jumlah sample untuk averaging 5
#define SAMPLE_COUNT_10     10          // Jumlah sample untuk averaging 10

// ============== KONFIGURASI INTERVAL ==============
#define READ_INTERVAL_MS    100         // Interval pembacaan (ms)
#define VREF_DISPLAY_INTERVAL_MS  1000  // Interval tampilan VREF (ms)

// ============== VARIABEL GLOBAL ==============
unsigned long previousMillis = 0;
unsigned long previousVrefMillis = 0;
float lastVREF = 0.0;  // Menyimpan VREF terakhir yang dihitung

// Buffer untuk analogRead (raw value 0-4095)
int rawBuffer[SAMPLE_COUNT_10];
int rawIndex = 0;
bool rawBufferFull = false;

// Buffer untuk analogReadMilliVolts (calibrated mV)
uint32_t mVBuffer[SAMPLE_COUNT_10];
int mVIndex = 0;
bool mVBufferFull = false;

// ============== FUNGSI PROTOTYPES ==============
float rawToVoltage(int adcValue);
float calculateEffectiveVREF(int rawValue, uint32_t mVValue);
void addRawSample(int value);
void addMvSample(uint32_t value);
int getRawAverage(int count);
uint32_t getMvAverage(int count);
void printHeader();
void printResults(int rawDirect, int rawAvg5, int rawAvg10,
                  uint32_t mVDirect, uint32_t mVAvg5, uint32_t mVAvg10);
void printVREF(float vref);

void setup() {
  // Inisialisasi Serial
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  Serial.println();
  Serial.println("=======================================================================");
  Serial.println("        ESP32-S3 ADC Demo - 2 Versi, Masing-masing 3 Output");
  Serial.println("=======================================================================");
  Serial.printf("Pin ADC1: GPIO%d\n", ADC_PIN);
  Serial.println();
  Serial.println("Versi A: analogRead()");
  Serial.println("  - Return: 0-4095 (raw, tidak terkalibrasi)");
  Serial.println("  - Konversi ke Volt: manual (raw * 3.3 / 4095)");
  Serial.println();
  Serial.println("Versi B: analogReadMilliVolts()");
  Serial.println("  - Return: 0-3100 mV (terkalibrasi)");
  Serial.println("  - Lebih akurat karena menggunakan kalibrasi internal chip");
  Serial.println();
  Serial.println("VREF Calculation:");
  Serial.println("  - Metode: Hitung dari perbandingan raw dan mV");
  Serial.println("  - Rumus: VREF = (mV * 4095) / (raw * 1000)");
  Serial.println("  - Format Output: CSV");
  Serial.println("  - VREF dihitung setiap 1 detik, ditampilkan di setiap baris");
  Serial.println("=======================================================================");
  Serial.println();
  
  // Set resolusi ADC (9-12 bit, default 12-bit)
  analogReadResolution(ADC_RESOLUTION);
  
  // Set attenuation untuk rentang 0-3.1V (ESP32-S3)
  // ADC_0db: 0-950mV, ADC_2_5db: 0-1250mV, ADC_6db: 0-1750mV, ADC_11db: 0-3100mV
  analogSetAttenuation(ADC_11db);
  
  // Inisialisasi buffer dengan 0
  for (int i = 0; i < SAMPLE_COUNT_10; i++) {
    rawBuffer[i] = 0;
    mVBuffer[i] = 0;
  }
  
  printHeader();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Non-blocking interval menggunakan millis
  if (currentMillis - previousMillis >= READ_INTERVAL_MS) {
    previousMillis = currentMillis;
    
    // ===== VERSI A: analogRead() =====
    int rawDirect = analogRead(ADC_PIN);
    addRawSample(rawDirect);
    int rawAvg5 = getRawAverage(SAMPLE_COUNT_5);
    int rawAvg10 = getRawAverage(SAMPLE_COUNT_10);
    
    // ===== VERSI B: analogReadMilliVolts() =====
    uint32_t mVDirect = analogReadMilliVolts(ADC_PIN);
    addMvSample(mVDirect);
    uint32_t mVAvg5 = getMvAverage(SAMPLE_COUNT_5);
    uint32_t mVAvg10 = getMvAverage(SAMPLE_COUNT_10);
    
    // Hitung VREF efektif setiap 1 detik (menggunakan avg10 untuk lebih stabil)
    unsigned long currentVrefMillis = millis();
    if (currentVrefMillis - previousVrefMillis >= VREF_DISPLAY_INTERVAL_MS) {
      previousVrefMillis = currentVrefMillis;
      lastVREF = calculateEffectiveVREF(rawAvg10, mVAvg10);
    }
    
    // Jika VREF belum pernah dihitung, hitung dari nilai saat ini
    if (lastVREF == 0.0 && rawAvg10 > 0) {
      lastVREF = calculateEffectiveVREF(rawAvg10, mVAvg10);
    }
    
    // Tampilkan hasil dalam format CSV
    printResults(rawDirect, rawAvg5, rawAvg10, mVDirect, mVAvg5, mVAvg10);
  }
}

/**
 * Konversi nilai ADC raw ke tegangan (Volt) - untuk Versi A
 * @param adcValue Nilai ADC mentah (0-4095 untuk 12-bit)
 * @return Tegangan dalam Volt
 */
float rawToVoltage(int adcValue) {
  return (adcValue * ADC_VREF) / (float)((1 << ADC_RESOLUTION) - 1);
}

/**
 * Hitung VREF efektif dari perbandingan raw dan mV (Metode 1)
 * Rumus: VREF = (mV * 4095) / (raw * 1000)
 * 
 * @param rawValue Nilai ADC raw (0-4095)
 * @param mVValue Nilai ADC dalam mV (terkalibrasi)
 * @return VREF efektif dalam Volt, atau 0 jika rawValue = 0
 */
float calculateEffectiveVREF(int rawValue, uint32_t mVValue) {
  if (rawValue == 0) {
    return 0.0;
  }
  // VREF = (mV * 4095) / (raw * 1000)
  return (mVValue * 4095.0) / (rawValue * 1000.0);
}

/**
 * Tambahkan sample ke circular buffer (Versi A - raw)
 * @param value Nilai ADC raw yang akan disimpan
 */
void addRawSample(int value) {
  rawBuffer[rawIndex] = value;
  rawIndex++;
  
  if (rawIndex >= SAMPLE_COUNT_10) {
    rawIndex = 0;
    rawBufferFull = true;
  }
}

/**
 * Tambahkan sample ke circular buffer (Versi B - mV)
 * @param value Nilai mV yang akan disimpan
 */
void addMvSample(uint32_t value) {
  mVBuffer[mVIndex] = value;
  mVIndex++;
  
  if (mVIndex >= SAMPLE_COUNT_10) {
    mVIndex = 0;
    mVBufferFull = true;
  }
}

/**
 * Hitung rata-rata dari buffer raw (Versi A)
 * @param count Jumlah sample yang akan di-rata-rata (5 atau 10)
 * @return Nilai rata-rata
 */
int getRawAverage(int count) {
  int availableSamples = rawBufferFull ? SAMPLE_COUNT_10 : rawIndex;
  
  if (availableSamples < count) {
    count = availableSamples;
  }
  
  if (count == 0) {
    return 0;
  }
  
  long sum = 0;
  int startIdx = rawIndex - count;
  
  for (int i = 0; i < count; i++) {
    int idx = (startIdx + i + SAMPLE_COUNT_10) % SAMPLE_COUNT_10;
    sum += rawBuffer[idx];
  }
  
  return (int)(sum / count);
}

/**
 * Hitung rata-rata dari buffer mV (Versi B)
 * @param count Jumlah sample yang akan di-rata-rata (5 atau 10)
 * @return Nilai rata-rata dalam mV
 */
uint32_t getMvAverage(int count) {
  int availableSamples = mVBufferFull ? SAMPLE_COUNT_10 : mVIndex;
  
  if (availableSamples < count) {
    count = availableSamples;
  }
  
  if (count == 0) {
    return 0;
  }
  
  unsigned long sum = 0;
  int startIdx = mVIndex - count;
  
  for (int i = 0; i < count; i++) {
    int idx = (startIdx + i + SAMPLE_COUNT_10) % SAMPLE_COUNT_10;
    sum += mVBuffer[idx];
  }
  
  return (uint32_t)(sum / count);
}

/**
 * Tampilkan header CSV
 */
void printHeader() {
  Serial.println("Timestamp(ms),Raw_Direct,Raw_Avg5,Raw_Avg10,Volt_Direct,Volt_Avg5,Volt_Avg10,mV_Direct,mV_Avg5,mV_Avg10,Effective_VREF");
}

/**
 * Tampilkan hasil pembacaan dalam format CSV
 */
void printResults(int rawDirect, int rawAvg5, int rawAvg10,
                  uint32_t mVDirect, uint32_t mVAvg5, uint32_t mVAvg10) {
  // Konversi raw ke Volt (Versi A)
  float voltDirect = rawToVoltage(rawDirect);
  float voltAvg5 = rawToVoltage(rawAvg5);
  float voltAvg10 = rawToVoltage(rawAvg10);
  
  // Format CSV: Timestamp,Raw_Direct,Raw_Avg5,Raw_Avg10,Volt_Direct,Volt_Avg5,Volt_Avg10,mV_Direct,mV_Avg5,mV_Avg10,Effective_VREF
  Serial.printf("%lu,%d,%d,%d,%.3f,%.3f,%.3f,%lu,%lu,%lu,%.3f\n",
                millis(),
                rawDirect, rawAvg5, rawAvg10,
                voltDirect, voltAvg5, voltAvg10,
                mVDirect, mVAvg5, mVAvg10,
                lastVREF);
}
