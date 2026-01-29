/**
 * ESP32-S3 Analog Input Demo
 * 1 GPIO dengan 3 Output berbeda:
 * - Output 1: Pembacaan langsung (tanpa averaging)
 * - Output 2: Averaging 5 sample
 * - Output 3: Averaging 10 sample
 * 
 * Pin ADC1 yang aman: GPIO1, GPIO2, GPIO4-GPIO10
 * Hindari GPIO3 (strapping pin)
 * ADC2 (GPIO11-20) tidak bisa digunakan saat WiFi aktif
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

// ============== VARIABEL GLOBAL ==============
unsigned long previousMillis = 0;

// Buffer untuk menyimpan sample
int sampleBuffer[SAMPLE_COUNT_10];
int sampleIndex = 0;
bool bufferFull = false;

// ============== FUNGSI PROTOTYPES ==============
float adcToVoltage(int adcValue);
void addSample(int value);
int getAverage(int count);
void printResults(int rawDirect, int rawAvg5, int rawAvg10);

void setup() {
  // Inisialisasi Serial
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  Serial.println();
  Serial.println("================================================");
  Serial.println("   ESP32-S3 ADC Demo - 1 GPIO, 3 Output");
  Serial.println("================================================");
  Serial.printf("Pin ADC1: GPIO%d\n", ADC_PIN);
  Serial.println("Output:");
  Serial.println("  1. Langsung (tanpa averaging)");
  Serial.println("  2. Averaging 5 sample");
  Serial.println("  3. Averaging 10 sample");
  Serial.println("================================================");
  Serial.println();
  
  // Set resolusi ADC (9-12 bit, default 12-bit)
  analogReadResolution(ADC_RESOLUTION);
  
  // Set attenuation untuk rentang 0-3.3V
  // ADC_0db: 0-1.1V, ADC_2_5db: 0-1.5V, ADC_6db: 0-2.2V, ADC_11db: 0-3.3V
  analogSetAttenuation(ADC_11db);
  
  // Inisialisasi buffer dengan 0
  for (int i = 0; i < SAMPLE_COUNT_10; i++) {
    sampleBuffer[i] = 0;
  }
  
  Serial.println("Timestamp(ms) | Langsung            | Avg 5 Sample        | Avg 10 Sample");
  Serial.println("--------------|---------------------|---------------------|--------------------");
}

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

/**
 * Konversi nilai ADC ke tegangan (Volt)
 * @param adcValue Nilai ADC mentah (0-4095 untuk 12-bit)
 * @return Tegangan dalam Volt
 */
float adcToVoltage(int adcValue) {
  return (adcValue * ADC_VREF) / (float)((1 << ADC_RESOLUTION) - 1);
}

/**
 * Tambahkan sample ke circular buffer
 * @param value Nilai ADC yang akan disimpan
 */
void addSample(int value) {
  sampleBuffer[sampleIndex] = value;
  sampleIndex++;
  
  if (sampleIndex >= SAMPLE_COUNT_10) {
    sampleIndex = 0;
    bufferFull = true;
  }
}

/**
 * Hitung rata-rata dari buffer
 * @param count Jumlah sample yang akan di-rata-rata (5 atau 10)
 * @return Nilai rata-rata, atau nilai terakhir jika buffer belum cukup
 */
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

/**
 * Tampilkan hasil pembacaan ke Serial Monitor
 */
void printResults(int rawDirect, int rawAvg5, int rawAvg10) {
  float voltDirect = adcToVoltage(rawDirect);
  float voltAvg5 = adcToVoltage(rawAvg5);
  float voltAvg10 = adcToVoltage(rawAvg10);
  
  Serial.printf("%13lu | %4d = %6.3fV     | %4d = %6.3fV     | %4d = %6.3fV\n",
                millis(), rawDirect, voltDirect, rawAvg5, voltAvg5, rawAvg10, voltAvg10);
}