/**
 * ESP32-S3 Analog Input Demo
 * 3 Versi Pembacaan ADC:
 * - Versi 1: Tanpa Averaging (GPIO4)
 * - Versi 2: Averaging 5 Sample (GPIO5)
 * - Versi 3: Averaging 10 Sample (GPIO6)
 * 
 * Pin ADC1 yang aman: GPIO1, GPIO2, GPIO4-GPIO10
 * Hindari GPIO3 (strapping pin)
 * ADC2 (GPIO11-20) tidak bisa digunakan saat WiFi aktif
 */

#include <Arduino.h>

// ============== KONFIGURASI PIN ADC1 ==============
#define ADC_PIN_NO_AVG      4   // Versi 1: Tanpa averaging
#define ADC_PIN_AVG_5       5   // Versi 2: Averaging 5 sample
#define ADC_PIN_AVG_10      6   // Versi 3: Averaging 10 sample

// ============== KONFIGURASI ADC ==============
#define ADC_RESOLUTION      12          // 12-bit (0-4095)
#define ADC_VREF            3.3         // Tegangan referensi (Volt)
#define SAMPLE_COUNT_5      5           // Jumlah sample untuk averaging versi 2
#define SAMPLE_COUNT_10     10          // Jumlah sample untuk averaging versi 3

// ============== KONFIGURASI INTERVAL ==============
#define READ_INTERVAL_MS    100         // Interval pembacaan (ms)

// ============== VARIABEL GLOBAL ==============
unsigned long previousMillis = 0;

// ============== FUNGSI PROTOTYPES ==============
float adcToVoltage(int adcValue);
int readADC_NoAveraging(int pin);
int readADC_Averaging(int pin, int sampleCount);
void printResults(int raw1, float volt1, int raw2, float volt2, int raw3, float volt3);

void setup() {
  // Inisialisasi Serial
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("   ESP32-S3 ADC Demo - 3 Versi");
  Serial.println("========================================");
  Serial.println("Pin ADC1 yang digunakan:");
  Serial.printf("  - GPIO%d: Tanpa Averaging\n", ADC_PIN_NO_AVG);
  Serial.printf("  - GPIO%d: Averaging 5 Sample\n", ADC_PIN_AVG_5);
  Serial.printf("  - GPIO%d: Averaging 10 Sample\n", ADC_PIN_AVG_10);
  Serial.println("========================================");
  Serial.println();
  
  // Set resolusi ADC (9-12 bit, default 12-bit)
  analogReadResolution(ADC_RESOLUTION);
  
  // Set attenuation untuk rentang 0-3.3V
  // ADC_0db: 0-1.1V, ADC_2_5db: 0-1.5V, ADC_6db: 0-2.2V, ADC_11db: 0-3.3V
  analogSetAttenuation(ADC_11db);
  
  Serial.println("Timestamp(ms) | No Avg (GPIO4)      | Avg 5 (GPIO5)       | Avg 10 (GPIO6)");
  Serial.println("--------------|---------------------|---------------------|--------------------");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Non-blocking interval menggunakan millis
  if (currentMillis - previousMillis >= READ_INTERVAL_MS) {
    previousMillis = currentMillis;
    
    // ===== VERSI 1: Tanpa Averaging =====
    int rawValue1 = readADC_NoAveraging(ADC_PIN_NO_AVG);
    float voltage1 = adcToVoltage(rawValue1);
    
    // ===== VERSI 2: Averaging 5 Sample =====
    int rawValue2 = readADC_Averaging(ADC_PIN_AVG_5, SAMPLE_COUNT_5);
    float voltage2 = adcToVoltage(rawValue2);
    
    // ===== VERSI 3: Averaging 10 Sample =====
    int rawValue3 = readADC_Averaging(ADC_PIN_AVG_10, SAMPLE_COUNT_10);
    float voltage3 = adcToVoltage(rawValue3);
    
    // Tampilkan hasil
    printResults(rawValue1, voltage1, rawValue2, voltage2, rawValue3, voltage3);
  }
  
  // Kode lain bisa berjalan di sini tanpa terblokir
}

/**
 * Konversi nilai ADC ke tegangan (Volt)
 * @param adcValue Nilai ADC mentah (0-4095 untuk 12-bit)
 * @return Tegangan dalam Volt
 */
float adcToVoltage(int adcValue) {
  return (adcValue * ADC_VREF) / (float)((1 << ADC_RESOLUTION) - 1);
}

/**
 * VERSI 1: Pembacaan ADC tanpa averaging
 * @param pin Pin GPIO yang akan dibaca
 * @return Nilai ADC mentah
 */
int readADC_NoAveraging(int pin) {
  return analogRead(pin);
}

/**
 * VERSI 2 & 3: Pembacaan ADC dengan averaging
 * @param pin Pin GPIO yang akan dibaca
 * @param sampleCount Jumlah sample untuk di-rata-rata
 * @return Nilai ADC rata-rata
 */
int readADC_Averaging(int pin, int sampleCount) {
  long sum = 0;
  
  for (int i = 0; i < sampleCount; i++) {
    sum += analogRead(pin);
    delayMicroseconds(100);  // Delay kecil antar pembacaan
  }
  
  return (int)(sum / sampleCount);
}

/**
 * Tampilkan hasil pembacaan ke Serial Monitor
 */
void printResults(int raw1, float volt1, int raw2, float volt2, int raw3, float volt3) {
  Serial.printf("%13lu | %4d = %6.3fV     | %4d = %6.3fV     | %4d = %6.3fV\n",
                millis(), raw1, volt1, raw2, volt2, raw3, volt3);
}