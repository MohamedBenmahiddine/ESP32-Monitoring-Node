#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"

#define LED_GPIO 2
#define BUTTON_GPIO 0

static const char *TAG = "MONITOR";

static TaskHandle_t button_task_handle = NULL;
static void button_task(void *arg)
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG, "Button event received");
    }
}

// ISR
static void IRAM_ATTR button_isr_handler(void *arg)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    vTaskNotifyGiveFromISR(
        button_task_handle,
        &higher_priority_task_woken);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void app_main(void)
{
    // LED
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    // Button
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };

    gpio_config(&button_config);

    // Creation de la Task
    xTaskCreate(
        button_task,
        "button_task",
        2048,
        NULL,
        5,
        &button_task_handle);

    // Install GPIO interrupt service
    gpio_install_isr_service(0);

    // Attach ISR to button
    gpio_isr_handler_add(
        BUTTON_GPIO,
        button_isr_handler,
        NULL);

    ESP_LOGI(TAG, "Monitoring Node started");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}