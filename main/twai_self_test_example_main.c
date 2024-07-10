#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/twai.h"
#include "can.h"

#define EXAMPLE_TAG             "TWAI Self Test"

void app_main(void)
{
    uint32_t id = 0;
    uint8_t buff[8] = {0, 1, 2, 3, 4, 5, 6, 7};

    can_start();
    
    while (true) {
        can_send(id, buff, 8);
        id++;
//        id++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
