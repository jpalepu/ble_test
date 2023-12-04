
#include "esp_log.h"
#include "bletest.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char *TAG = "iot_services::Ble_tests";

#ifdef __cplusplus
extern "C" {
#endif

void ble_service_tests_impl() {


    adc_service_init();
    ESP_LOGI(TAG, "ADC service intialized...");

    esp_err_t result;
    result = irreo::iot_services::power_init();
    TEST_ASSERT(result == ESP_OK);
    ESP_LOGI(TAG, "Power service intialized...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    result = irreo::iot_services::ble_init();
    TEST_ASSERT(result == ESP_OK);
    ESP_LOGI(TAG, "Ble service intialized! Starting advertising");

    vTaskDelay(pdMS_TO_TICKS(25000));
    
    result = irreo::iot_services::ble_deinit();
    TEST_ASSERT(result == ESP_OK);
    ESP_LOGI(TAG, "Ble service deintialized! Modem now entering sleep mode...");

    result = irreo::iot_services::power_deinit();
    TEST_ASSERT(result == ESP_OK);
    ESP_LOGI(TAG, "Power service deintialized...");

    adc_service_deinit();
    ESP_LOGI(TAG, "ADC service deintialized...");

    #endif
}

#ifdef __cplusplus
}
#endif