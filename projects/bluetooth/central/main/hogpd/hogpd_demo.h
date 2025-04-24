#pragma once

#define HOGPD_DEMO_ENABLE 1

int32_t hogpd_demo_init(void);
int32_t hogpd_demo_deinit(uint8_t deinit_bluetooth_future);
int32_t hogpd_demo_deinit_because_bluetooth_deinit_future();
