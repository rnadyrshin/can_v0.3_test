#pragma once

esp_err_t can_start();
esp_err_t can_stop();
void can_send(uint32_t id, uint8_t* buff, uint8_t len);
