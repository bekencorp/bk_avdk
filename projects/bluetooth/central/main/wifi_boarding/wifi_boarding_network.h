#pragma once

#include "lwip/sockets.h"
#include "net.h"

int boarding_wifi_sta_connect(char *ssid, char *key);
int boarding_wifi_soft_ap_start(char *ssid, char *key, uint16_t channel);


