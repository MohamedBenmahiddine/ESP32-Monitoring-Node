#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "esp_log.h"

#define LED_GPIO 2
#define BUTTON_GPIO 0

static const char *TAG = "MONITOR";

static TickType_t last_interrupt_time = 0;

static int shared_value = 0;

// Creation de event
typedef enum
{
    BUTTON_PRESSED,
    GPS_EVENT,
    CAN_EVENT,
    SENSOR_EVENT
} event_t;
typedef struct
{
    event_t type;
    int value;
} monitoring_event_t;

typedef enum
{
    SYSTEM_IDLE,
    SYSTEM_ACTIVE
} system_state_t;

static system_state_t system_state = SYSTEM_IDLE;

static QueueHandle_t event_queue = NULL;
static SemaphoreHandle_t resource_mutex = NULL;

// Monitoring Task
static void monitoring_task(void *arg)
{
    monitoring_event_t event;

    while (1)
    {
        if (xQueueReceive(event_queue, &event, portMAX_DELAY))
        {
            if (event.type == BUTTON_PRESSED)
            {
                system_state = !system_state;

                if (system_state == SYSTEM_ACTIVE)
                {
                    ESP_LOGI(TAG, "System ACTIVE");
                }
                else
                {
                    ESP_LOGI(TAG, "System IDLE");
                }

                gpio_set_level(LED_GPIO, system_state);
            }
            else if (event.type == SENSOR_EVENT)
            {
                ESP_LOGI(TAG, "Temperature: %d C", event.value);
            }
            else if (event.type == GPS_EVENT)
            {
                ESP_LOGI(TAG, "GPS satellites: %d", event.value);
            }
        }
    }
}

// ISR
static void IRAM_ATTR button_isr_handler(void *arg)
{
    TickType_t current_time = xTaskGetTickCountFromISR();

    if ((current_time - last_interrupt_time) > pdMS_TO_TICKS(200))
    {
        monitoring_event_t event = {
            .type = BUTTON_PRESSED,
            .value = 0};

        BaseType_t higher_priority_task_woken = pdFALSE;

        xQueueSendFromISR(
            event_queue,
            &event,
            &higher_priority_task_woken);

        last_interrupt_time = current_time;

        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

// sensor task
static void sensor_task(void *arg)
{
    int sensor_value = 0;

    while (1)
    {
        sensor_value = 20 + (sensor_value + 1) % 11;

        monitoring_event_t event = {
            .type = SENSOR_EVENT,
            .value = sensor_value};

        xQueueSend(event_queue, &event, portMAX_DELAY);

        if (xSemaphoreTake(resource_mutex, portMAX_DELAY))
        {
            shared_value++;

            ESP_LOGI(TAG, "Sensor: shared_value = %d", shared_value);

            xSemaphoreGive(resource_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// GPS task
static void gps_task(void *arg)
{
    int satellites = 8;
    while (1)
    {
        monitoring_event_t event = {
            .type = GPS_EVENT,
            .value = satellites};

        xQueueSend(event_queue, &event, portMAX_DELAY);

        satellites--;

        if (satellites < 4)
        {
            satellites = 8;
        }
        if (xSemaphoreTake(resource_mutex, portMAX_DELAY))
        {
            shared_value++;

            ESP_LOGI(TAG, "GPS: shared_value = %d", shared_value);

            xSemaphoreGive(resource_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
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

    // Creation Queue
    event_queue = xQueueCreate(10, sizeof(monitoring_event_t));

    resource_mutex = xSemaphoreCreateMutex();

    if (resource_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }

    // Creation de la Task
    xTaskCreate(
        monitoring_task,
        "monitoring_task",
        2048,
        NULL,
        5,
        NULL);

    xTaskCreate(
        sensor_task,
        "sensor_task",
        2048,
        NULL,
        4,
        NULL);

    xTaskCreate(
        gps_task,
        "gps_task",
        2048,
        NULL,
        6,
        NULL);

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