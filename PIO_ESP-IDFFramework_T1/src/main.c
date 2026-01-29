/**
 * ESP32-S3 Analog Input Demo - ESP-IDF Framework
 * Port dari Arduino Framework ke ESP-IDF
 * 
 * 1 GPIO dengan 2 Versi Pembacaan:
 * 
 * Versi A: adc_oneshot_read() - Nilai raw (0-4095), konversi manual ke Volt
 * Versi B: adc_cali_raw_to_voltage() - Nilai terkalibrasi dalam mV
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
 * Referensi: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/adc/index.html
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "soc/adc_channel.h"

static const char *TAG = "ADC_Demo";

// ============== KONFIGURASI PIN ADC1 ==============
#define ADC_GPIO             4           // GPIO4 = ADC1_CHANNEL_3
#define ADC_CHANNEL          ADC_CHANNEL_3
#define ADC_UNIT             ADC_UNIT_1

// ============== KONFIGURASI ADC ==============
#define ADC_BITWIDTH         ADC_BITWIDTH_12  // 12-bit (0-4095)
#define ADC_ATTEN            ADC_ATTEN_DB_12  // 0-3100mV (ESP32-S3)
#define ADC_VREF             3.3              // Tegangan referensi (Volt) - untuk konversi manual
#define SAMPLE_COUNT_5       5                // Jumlah sample untuk averaging 5
#define SAMPLE_COUNT_10      10               // Jumlah sample untuk averaging 10

// ============== KONFIGURASI INTERVAL ==============
#define READ_INTERVAL_MS     100              // Interval pembacaan (ms)
#define VREF_DISPLAY_INTERVAL_MS  1000        // Interval tampilan VREF (ms)

// ============== VARIABEL GLOBAL ==============
// TickType_t previousTick = 0;  // Tidak digunakan lagi - menggunakan vTaskDelayUntil
// TickType_t previousVrefTick = 0;  // Tidak digunakan - VREF calculation di-comment out
float lastVREF = 0.0f;  // Menyimpan VREF terakhir yang dihitung (tidak digunakan, di-comment out)

// ADC handles
adc_oneshot_unit_handle_t adc1_handle = NULL;
adc_cali_handle_t adc_cali_handle = NULL;

// Buffer untuk raw value (0-4095)
int rawBuffer[SAMPLE_COUNT_10];
int rawIndex = 0;
bool rawBufferFull = false;

// Buffer untuk calibrated mV
uint32_t mVBuffer[SAMPLE_COUNT_10];
int mVIndex = 0;
bool mVBufferFull = false;

// ============== FUNGSI PROTOTYPES ==============
static bool adc_calibration_init(void);
static void adc_init(void);
static float rawToVoltage(int adcValue);
// static float calculateEffectiveVREF(int rawValue, uint32_t mVValue);  // COMMENTED OUT - tidak digunakan
static void addRawSample(int value);
static void addMvSample(uint32_t value);
static int getRawAverage(int count);
static uint32_t getMvAverage(int count);
static void printHeader(void);
static void printResults(int rawDirect, int rawAvg5, int rawAvg10,
                         uint32_t mVDirect, uint32_t mVAvg5, uint32_t mVAvg10);

/**
 * Inisialisasi ADC Calibration
 * Menggunakan Curve Fitting Scheme untuk ESP32-S3
 */
static bool adc_calibration_init(void)
{
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };

    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC Calibration initialized successfully");
        return true;
    } else if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "ADC Calibration not supported. eFuse bits may not be set.");
        return false;
    } else {
        ESP_LOGE(TAG, "ADC Calibration initialization failed: %s", esp_err_to_name(ret));
        return false;
    }
}

/**
 * Inisialisasi ADC Oneshot Mode
 */
static void adc_init(void)
{
    // Inisialisasi ADC1 unit
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // Konfigurasi channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    ESP_LOGI(TAG, "ADC1 initialized - GPIO%d (Channel %d)", ADC_GPIO, ADC_CHANNEL);
}

/**
 * Konversi nilai ADC raw ke tegangan (Volt) - untuk Versi A
 * @param adcValue Nilai ADC mentah (0-4095 untuk 12-bit)
 * @return Tegangan dalam Volt
 */
static float rawToVoltage(int adcValue)
{
    return (adcValue * ADC_VREF) / 4095.0f;
}

/**
 * Hitung VREF efektif dari perbandingan raw dan mV (Metode 1)
 * 
 * Fungsi ini menghitung VREF efektif dengan membandingkan nilai raw ADC
 * (tidak terkalibrasi) dengan nilai terkalibrasi dalam mV.
 * 
 * Rumus: VREF = (mV * 4095) / (raw * 1000)
 * 
 * Penjelasan:
 * - mV adalah nilai terkalibrasi dari adc_cali_raw_to_voltage()
 * - raw adalah nilai ADC mentah (0-4095 untuk 12-bit)
 * - 4095 adalah nilai maksimum ADC (2^12 - 1)
 * - 1000 adalah konversi dari mV ke Volt
 * 
 * Catatan Penting:
 * - VREF efektif ini BERVARIASI tergantung input voltage
 * - Ini normal karena kalibrasi internal menggunakan polynomial curve fitting,
 *   bukan VREF linier tunggal
 * - VREF efektif ini adalah pendekatan linier sederhana dari kalibrasi kompleks
 * 
 * @param rawValue Nilai ADC raw (0-4095 untuk 12-bit)
 * @param mVValue Nilai ADC dalam mV (terkalibrasi dari adc_cali_raw_to_voltage)
 * @return VREF efektif dalam Volt, atau 0 jika rawValue = 0
 * 
 * Contoh:
 *   rawValue = 2048, mVValue = 1650
 *   VREF = (1650 * 4095) / (2048 * 1000) = 3.297V
 */
/*
static float calculateEffectiveVREF(int rawValue, uint32_t mVValue)
{
    // Cek pembagi nol
    if (rawValue == 0) {
        return 0.0f;
    }
    
    // Rumus: VREF = (mV * 4095) / (raw * 1000)
    // - mVValue * 4095: skala mV ke resolusi ADC penuh
    // - rawValue * 1000: konversi raw ke mV (asumsi linier) lalu ke Volt
    // Hasil: VREF efektif dalam Volt
    return (mVValue * 4095.0f) / (rawValue * 1000.0f);
}
*/

/**
 * Tambahkan sample ke circular buffer (Versi A - raw)
 * @param value Nilai ADC raw yang akan disimpan
 */
static void addRawSample(int value)
{
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
static void addMvSample(uint32_t value)
{
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
static int getRawAverage(int count)
{
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
static uint32_t getMvAverage(int count)
{
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
static void printHeader(void)
{
    // Effective_VREF di-comment out
    // printf("Timestamp(ms),Raw_Direct,Raw_Avg5,Raw_Avg10,Volt_Direct,Volt_Avg5,Volt_Avg10,mV_Direct,mV_Avg5,mV_Avg10,Effective_VREF\n");
    printf("Timestamp(ms),Raw_Direct,Raw_Avg5,Raw_Avg10,Volt_Direct,Volt_Avg5,Volt_Avg10,mV_Direct,mV_Avg5,mV_Avg10\n");
}

/**
 * Tampilkan hasil pembacaan dalam format CSV
 */
static void printResults(int rawDirect, int rawAvg5, int rawAvg10,
                         uint32_t mVDirect, uint32_t mVAvg5, uint32_t mVAvg10)
{
    // Konversi raw ke Volt (Versi A)
    float voltDirect = rawToVoltage(rawDirect);
    float voltAvg5 = rawToVoltage(rawAvg5);
    float voltAvg10 = rawToVoltage(rawAvg10);
    
    // Format CSV: Timestamp,Raw_Direct,Raw_Avg5,Raw_Avg10,Volt_Direct,Volt_Avg5,Volt_Avg10,mV_Direct,mV_Avg5,mV_Avg10
    // Effective_VREF di-comment out
    // Format CSV (old): Timestamp,Raw_Direct,Raw_Avg5,Raw_Avg10,Volt_Direct,Volt_Avg5,Volt_Avg10,mV_Direct,mV_Avg5,mV_Avg10,Effective_VREF
    TickType_t currentTick = xTaskGetTickCount();
    uint32_t timestamp_ms = (currentTick * 1000) / configTICK_RATE_HZ;
    
    // printf("%lu,%d,%d,%d,%.3f,%.3f,%.3f,%lu,%lu,%lu,%.3f\n",
    //        timestamp_ms,
    //        rawDirect, rawAvg5, rawAvg10,
    //        voltDirect, voltAvg5, voltAvg10,
    //        mVDirect, mVAvg5, mVAvg10,
    //        lastVREF);
    printf("%lu,%d,%d,%d,%.3f,%.3f,%.3f,%lu,%lu,%lu\n",
           timestamp_ms,
           rawDirect, rawAvg5, rawAvg10,
           voltDirect, voltAvg5, voltAvg10,
           mVDirect, mVAvg5, mVAvg10);
}

/**
 * Task utama untuk pembacaan ADC
 */
static void adc_read_task(void *pvParameters)
{
    ESP_LOGI(TAG, "ADC read task started");
    
    // Inisialisasi waktu untuk vTaskDelayUntil
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(READ_INTERVAL_MS);
    
    while (1) {
        // Gunakan vTaskDelayUntil untuk timing yang akurat dan tidak memblokir IDLE task
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // ===== VERSI A: adc_oneshot_read() - Raw Value =====
        int rawDirect = 0;
        esp_err_t ret = adc_oneshot_read(adc1_handle, ADC_CHANNEL, &rawDirect);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(ret));
            continue;
        }
        
        addRawSample(rawDirect);
        int rawAvg5 = getRawAverage(SAMPLE_COUNT_5);
        int rawAvg10 = getRawAverage(SAMPLE_COUNT_10);
        
        // ===== VERSI B: adc_cali_raw_to_voltage() - Calibrated mV =====
        uint32_t mVDirect = 0;
        if (adc_cali_handle != NULL) {
            int voltage_mv = 0;
            ret = adc_cali_raw_to_voltage(adc_cali_handle, rawDirect, &voltage_mv);
            if (ret == ESP_OK) {
                mVDirect = (uint32_t)voltage_mv;
            } else {
                ESP_LOGW(TAG, "ADC calibration failed: %s", esp_err_to_name(ret));
                mVDirect = 0;
            }
        } else {
            // Jika kalibrasi tidak tersedia, konversi manual
            mVDirect = (uint32_t)(rawToVoltage(rawDirect) * 1000.0f);
        }
        
        addMvSample(mVDirect);
        uint32_t mVAvg5 = getMvAverage(SAMPLE_COUNT_5);
        uint32_t mVAvg10 = getMvAverage(SAMPLE_COUNT_10);
        
        // Hitung VREF efektif setiap 1 detik (menggunakan avg10 untuk lebih stabil)
        // COMMENTED OUT - fungsi calculateEffectiveVREF tidak digunakan
        /*
        TickType_t currentVrefTick = xTaskGetTickCount();
        if ((currentVrefTick - previousVrefTick) >= pdMS_TO_TICKS(VREF_DISPLAY_INTERVAL_MS)) {
            previousVrefTick = currentVrefTick;
            lastVREF = calculateEffectiveVREF(rawAvg10, mVAvg10);
        }
        
        // Jika VREF belum pernah dihitung, hitung dari nilai saat ini
        if (lastVREF == 0.0f && rawAvg10 > 0) {
            lastVREF = calculateEffectiveVREF(rawAvg10, mVAvg10);
        }
        */
        
        // Set VREF ke 0 karena fungsi calculateEffectiveVREF di-comment out
        lastVREF = 0.0f;
        
        // Tampilkan hasil dalam format CSV
        printResults(rawDirect, rawAvg5, rawAvg10, mVDirect, mVAvg5, mVAvg10);
    }
}

/**
 * Main application entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "\n=======================================================================");
    ESP_LOGI(TAG, "        ESP32-S3 ADC Demo - ESP-IDF Framework");
    ESP_LOGI(TAG, "        2 Versi, Masing-masing 3 Output");
    ESP_LOGI(TAG, "=======================================================================");
    ESP_LOGI(TAG, "Pin ADC1: GPIO%d (Channel %d)", ADC_GPIO, ADC_CHANNEL);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Versi A: adc_oneshot_read()");
    ESP_LOGI(TAG, "  - Return: 0-4095 (raw, tidak terkalibrasi)");
    ESP_LOGI(TAG, "  - Konversi ke Volt: manual (raw * 3.3 / 4095)");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Versi B: adc_cali_raw_to_voltage()");
    ESP_LOGI(TAG, "  - Return: 0-3100 mV (terkalibrasi)");
    ESP_LOGI(TAG, "  - Lebih akurat karena menggunakan kalibrasi internal chip");
    ESP_LOGI(TAG, "");
    // VREF Calculation di-comment out - tidak digunakan
    /*
    ESP_LOGI(TAG, "VREF Calculation:");
    ESP_LOGI(TAG, "  - Metode: Hitung dari perbandingan raw dan mV");
    ESP_LOGI(TAG, "  - Rumus: VREF = (mV * 4095) / (raw * 1000)");
    ESP_LOGI(TAG, "  - Format Output: CSV");
    ESP_LOGI(TAG, "  - VREF dihitung setiap 1 detik, ditampilkan di setiap baris");
    */
    ESP_LOGI(TAG, "=======================================================================");
    ESP_LOGI(TAG, "");
    
    // Inisialisasi buffer dengan 0
    memset(rawBuffer, 0, sizeof(rawBuffer));
    memset(mVBuffer, 0, sizeof(mVBuffer));
    
    // Inisialisasi ADC
    adc_init();
    
    // Inisialisasi ADC Calibration
    bool cali_enable = adc_calibration_init();
    if (!cali_enable) {
        ESP_LOGW(TAG, "Calibration disabled. Using manual conversion.");
    }
    
    // Tampilkan header CSV
    printHeader();
    
    // Buat task untuk pembacaan ADC
    xTaskCreate(adc_read_task, "adc_read_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Application started. ADC readings will begin...");
}
