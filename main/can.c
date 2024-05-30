#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/twai.h"
#include "hal/twai_types.h"
#include "can.h"

#define TWAI_TIMING_CONFIG    TWAI_TIMING_CONFIG_100KBITS
#define SELF_TEST       false
#define TX_PIN          38
#define RX_PIN          39

static bool initialized = false;
static bool task_stopped = false;
static bool need_to_stop_task = false;
static TaskHandle_t task_handle = NULL;
static char *TAG = "CAN";

void can_task(void *params);

esp_err_t can_start() {
	ESP_LOGI(TAG, "Start");
/*
	if (!initialized)
	{
		ESP_LOGE(TAG, "CAN not initialized");
		return ESP_ERR_INVALID_STATE;
	}
*/
    can_stop();
    task_stopped = false;
    need_to_stop_task = false;
    xTaskCreatePinnedToCore(can_task, "CanMainTask", 4 * 1024, (void *) NULL, 2, &task_handle, 0);
    return ESP_OK;
}

esp_err_t can_stop() {
    if (!task_handle) {
        return ESP_OK;
    }
        
    need_to_stop_task = true;
    if (task_handle)
    {    
        while (!task_stopped) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    ESP_LOGI(TAG, "Stopped");
    return ESP_OK;
}

void can_reinit() {
	twai_stop();
	twai_driver_uninstall();

	bool selfTest = SELF_TEST;

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(38, 39, TWAI_MODE_NORMAL);
//    twai_general_config_t g_config;

	if (selfTest)
		g_config.mode = TWAI_MODE_NO_ACK;   //Set to NO_ACK mode due to self testing with single module
	else
    	g_config.mode = TWAI_MODE_NORMAL;

    g_config.tx_io = (gpio_num_t) TX_PIN;
    g_config.rx_io = (gpio_num_t) RX_PIN;
    g_config.clkout_io = -1;
    g_config.bus_off_io = -1;
    g_config.tx_queue_len = 5;
    g_config.rx_queue_len = 5;
/*
    g_config.alerts_enabled = TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | // | TWAI_ALERT_BUS_ERROR; //TWAI_ALERT_NONE
    TWAI_ALERT_BUS_RECOVERED | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF;
    g_config.clkout_divider = 0;
    g_config.intr_flags = (1<<1);// ESP_INTR_FLAG_LEVEL1;
*/
    
	twai_timing_config_t t_config = TWAI_TIMING_CONFIG();

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());
}

void can_send(uint32_t id, uint8_t* buff, uint8_t len) {
	twai_message_t tx_msg;
    tx_msg.identifier = id;
    tx_msg.extd = 1;
			
#if (SELF_TEST)
	tx_msg.self = 1;
#else
    tx_msg.self = 0;
#endif

    tx_msg.data_length_code = len;
    memcpy(tx_msg.data, buff, len);

    ESP_LOGI(TAG, "TX [ID %lu, len %d, body %02x %02x %02x %02x %02x %02x %02x %02x]", 
        id, len, buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6], buff[7]);

    esp_err_t err = twai_transmit(&tx_msg, 10);// portMAX_DELAY);
    if (err == ESP_OK) {
	    uint32_t alerts;
    	twai_read_alerts(&alerts, 10);

        if (alerts & (TWAI_ALERT_TX_FAILED | TWAI_ALERT_BUS_ERROR)) {
            ESP_LOGE(TAG, "Send NACK");
        }
        else if (alerts & TWAI_ALERT_TX_SUCCESS) {
        }
    }
    else {
        ESP_LOGE(TAG, "Send error %d", (int) err);
        can_reinit();
    }
}

void can_task(void *params) {
    ESP_LOGI(TAG, "Task started");

	can_reinit();

    while (!need_to_stop_task) 
    {
		bool needDelay = true;
		twai_message_t rx_msg;

        if (twai_receive(&rx_msg, 0) == ESP_OK)
        {
			needDelay = false;
            uint32_t id = rx_msg.identifier;
            uint8_t len = rx_msg.data_length_code;
            uint8_t* buff = rx_msg.data;
            ESP_LOGI(TAG, "RX [ID %lu, len %d, body %02x %02x %02x %02x %02x %02x %02x %02x]", 
                id, len, buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6], buff[7]);
        }

        twai_status_info_t status;
        if (twai_get_status_info(&status) == ESP_OK)
        {
			if (status.state == TWAI_STATE_BUS_OFF)
			{
				ESP_LOGE(TAG, "Reinit"); 
				can_reinit();
			}
		}

		if (needDelay) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    ESP_LOGI(TAG, "Task stopped");
    task_stopped = true;

    vTaskDelete(NULL);
}
