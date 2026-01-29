#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "driver/temperature_sensor.h"
#include "driver/touch_pad.h"
#include "driver/gpio.h"
#include "soc/soc.h"

// ===== Konfigurasi =====
#define TOUCH_PAD_NUM       TOUCH_PAD_NUM1  // GPIO1 pada ESP32-S3
#define LED_GPIO            GPIO_NUM_48     // Onboard RGB LED (atau ganti sesuai board)
#define TOUCH_THRESHOLD     500             // Threshold untuk touch detection

// ===== Global Variables =====
static temperature_sensor_handle_t temp_sensor = NULL;

// =====================================================
// TASK 1: Internal Temperature Sensor
// =====================================================
void task_temperature(void *pvParameters) {
    float temperature = 0;
    
    while (1) {
        if (temp_sensor != NULL) {
            temperature_sensor_get_celsius(temp_sensor, &temperature);
            printf("[TEMP] Internal Temperature: %.2f °C\n", temperature);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));  // Setiap 2 detik
    }
}

// =====================================================
// TASK 2: Heap Monitor (RAM Usage)
// =====================================================
void task_heap_monitor(void *pvParameters) {
    while (1) {
        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t min_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        
        printf("[HEAP] Free: %u KB | Min Free: %u KB | PSRAM Free: %u KB\n", 
               free_heap / 1024, min_heap / 1024, free_psram / 1024);
        
        vTaskDelay(pdMS_TO_TICKS(3000));  // Setiap 3 detik
    }
}

// =====================================================
// TASK 3: Random Number Generator
// =====================================================
void task_random_generator(void *pvParameters) {
    while (1) {
        uint32_t random_value = esp_random();
        int random_0_100 = random_value % 101;
        int random_dice = (random_value % 6) + 1;
        
        printf("[RAND] Raw: %lu | 0-100: %d | Dice: %d\n", 
               random_value, random_0_100, random_dice);
        
        vTaskDelay(pdMS_TO_TICKS(1500));  // Setiap 1.5 detik
    }
}

// =====================================================
// TASK 4: Uptime Counter
// =====================================================
void task_uptime(void *pvParameters) {
    while (1) {
        int64_t uptime_us = esp_timer_get_time();
        int64_t uptime_sec = uptime_us / 1000000;
        
        int hours = uptime_sec / 3600;
        int minutes = (uptime_sec % 3600) / 60;
        int seconds = uptime_sec % 60;
        
        printf("[UPTIME] %02d:%02d:%02d (Total: %lld seconds)\n", 
               hours, minutes, seconds, uptime_sec);
        
        vTaskDelay(pdMS_TO_TICKS(5000));  // Setiap 5 detik
    }
}

// =====================================================
// TASK 5: Touch Pad Sensor
// =====================================================
void task_touch_sensor(void *pvParameters) {
    uint32_t touch_value = 0;
    bool is_touched = false;
    bool prev_touched = false;
    
    while (1) {
        touch_pad_read_raw_data(TOUCH_PAD_NUM, &touch_value);
        is_touched = (touch_value > TOUCH_THRESHOLD);
        
        // Hanya print jika ada perubahan state atau setiap beberapa detik
        if (is_touched != prev_touched) {
            if (is_touched) {
                printf("[TOUCH] Pin TOUCHED! Value: %lu\n", touch_value);
            } else {
                printf("[TOUCH] Pin RELEASED. Value: %lu\n", touch_value);
            }
            prev_touched = is_touched;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));  // Poll setiap 100ms
    }
}

// =====================================================
// TASK 6: LED Blink
// =====================================================
void task_led_blink(void *pvParameters) {
    bool led_state = false;
    
    while (1) {
        led_state = !led_state;
        gpio_set_level(LED_GPIO, led_state);
        
        printf("[LED] State: %s\n", led_state ? "ON" : "OFF");
        
        vTaskDelay(pdMS_TO_TICKS(500));  // Toggle setiap 500ms
    }
}

// =====================================================
// Initialization Functions
// =====================================================
void init_temperature_sensor(void) {
    temperature_sensor_config_t temp_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    
    if (temperature_sensor_install(&temp_config, &temp_sensor) == ESP_OK) {
        temperature_sensor_enable(temp_sensor);
        printf("Temperature sensor initialized\n");
    } else {
        printf("Failed to initialize temperature sensor\n");
    }
}

void init_touch_pad(void) {
    touch_pad_init();
    touch_pad_config(TOUCH_PAD_NUM);
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_fsm_start();
    printf("Touch pad initialized (GPIO1)\n");
}

void init_led(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 0);
    printf("LED initialized (GPIO%d)\n", LED_GPIO);
}

// =====================================================
// Print System Info
// =====================================================
void print_system_info(void) {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║     ESP32-S3 Multi-Task Demo             ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Chip: %-10s  Cores: %d              ║\n", CONFIG_IDF_TARGET, chip_info.cores);
    printf("║ Revision: %d                              ║\n", chip_info.revision);
    
    uint32_t flash_size;
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        printf("║ Flash: %lu MB                            ║\n", flash_size / (1024 * 1024));
    }
    
    printf("║ Internal RAM: %lu KB                     ║\n", 
           heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024);
    
    size_t psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psram > 0) {
        printf("║ PSRAM: %lu MB                            ║\n", psram / (1024 * 1024));
    } else {
        printf("║ PSRAM: Not enabled                       ║\n");
    }
    printf("╚══════════════════════════════════════════╝\n\n");
}

// =====================================================
// Main Application
// =====================================================
void app_main() {
    print_system_info();
    
    // Initialize peripherals
    printf("Initializing peripherals...\n");
    init_temperature_sensor();
    init_touch_pad();
    init_led();
    printf("\n");
    
    // Create all 6 tasks - ALL pinned to Core 0 (single core demo)
    printf("Creating 6 tasks on Core 0...\n\n");
    
    xTaskCreatePinnedToCore(task_temperature,     "Temperature",  4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(task_heap_monitor,    "HeapMonitor",  4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(task_random_generator,"RandomGen",    4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(task_uptime,          "Uptime",       4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(task_touch_sensor,    "TouchSensor",  4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(task_led_blink,       "LEDBlink",     2048, NULL, 1, NULL, 0);
    
    printf("╔══════════════════════════════════════════╗\n");
    printf("║  Task         │ Interval  │ Priority    ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Temperature  │ 2000ms    │ 2           ║\n");
    printf("║  HeapMonitor  │ 3000ms    │ 1           ║\n");
    printf("║  RandomGen    │ 1500ms    │ 1           ║\n");
    printf("║  Uptime       │ 5000ms    │ 1           ║\n");
    printf("║  TouchSensor  │ 100ms     │ 3 (high)    ║\n");
    printf("║  LEDBlink     │ 500ms     │ 1           ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");
    
    printf("All tasks running on Core 0. Watch the serial output!\n");
    printf("Touch GPIO1 to see touch detection.\n\n");
    printf("========== TASKS OUTPUT ==========\n\n");
}
