#include "hardware/rtc.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include <FreeRTOS.h>
#include <hardware/gpio.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>
#include <time.h>

#include "gfx.h"
#include "ssd1306.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

#define TRIG_PIN 7
#define ECHO_PIN 8

SemaphoreHandle_t xSemaphoreTrigger = NULL;
QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;

void pin_callback(uint gpio, uint32_t events) {
    static uint32_t time_start;
    if (events == GPIO_IRQ_EDGE_RISE) {
        time_start = time_us_32();
    } else if (events == GPIO_IRQ_EDGE_FALL) {
        uint32_t time_end = time_us_32();
        uint32_t time_diff = time_end - time_start;
        xQueueReset(xQueueTime);
        xQueueSendFromISR(xQueueTime, &time_diff, NULL);
    }
}

void trigger_task(void *pvParameters) {
    while (1) {
        gpio_put(TRIG_PIN, 1);
        sleep_us(10);
        gpio_put(TRIG_PIN, 0);
        xSemaphoreGive(xSemaphoreTrigger);
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}

void echo_task(void *pvParameters) {
    uint32_t time_diff;
    while (1) {
        if (xQueueReceive(xQueueTime, &time_diff, pdMS_TO_TICKS(1000))){
            printf("Dist: %d cm\n", time_diff / 58);
            uint32_t distance = time_diff / 58;
            xQueueReset(xQueueDistance);
            xQueueSend(xQueueDistance, &distance, portMAX_DELAY);
        } else {
            int distance = 100000;
            xQueueReset(xQueueDistance);
            xQueueSend(xQueueDistance, &distance, portMAX_DELAY);
        }
    }
}

void oled_task(void *pvParameters) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    while (1) {
        char str_distance[20], progress_str[34];
        uint32_t distance;
        if (xQueueReceive(xQueueDistance, &distance, 0) == pdFALSE) {
            sprintf(str_distance, "Dist: ERRO");
        } else {
            if (xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(1000)) == pdTRUE) {
                xQueueReceive(xQueueDistance, &distance, portMAX_DELAY);
                if (distance > 800){
                    sprintf(str_distance, "Dist: ERRO");
                } else {
                    sprintf(str_distance, "Dist: %d cm", distance);
                    int progress = (distance > 21) ? 21 : distance;
                    memset(progress_str, '-', progress);
                    progress_str[progress] = '\0';
                }
            } else {
                strcpy(str_distance, "Dist: null");
            }
        }

        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 1, str_distance);
        gfx_draw_string(&disp, 0, 20, 1, progress_str);
        printf("%s\n", str_distance);
        printf("%s\n", progress_str);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gfx_show(&disp);
    }
}

int main() {
    stdio_init_all();

    gpio_init(TRIG_PIN);
    gpio_init(ECHO_PIN);

    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    xSemaphoreTrigger = xSemaphoreCreateBinary();
    xQueueTime = xQueueCreate(10, sizeof(uint32_t));
    xQueueDistance = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreate(trigger_task, "Trigger", 4096, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 4096, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED", 4096, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {
    }
}