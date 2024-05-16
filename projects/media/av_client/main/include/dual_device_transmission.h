// Copyright 2020-2021 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "media_app.h"
#include "lcd_act.h"

#ifdef __cplusplus
extern "C" {
#endif

#define THREAD_SIZE                (4 * 1024)
#define THREAD_PROIRITY            4
#define VOICE_PORT                 5001
#define VIDEO_PORT                 7180
#define TRANSFER_BUF_SIZE          1472    //(4 * 1024)

typedef struct {
	media_camera_device_t device;
	lcd_open_t lcd_dev;
} video_device_t;

typedef struct {
	int (*av_aud_voc_send_packet)(unsigned char *data, unsigned int len);
} av_aud_voc_setup_t;



void aud_voc_start(av_aud_voc_setup_t setup);

void aud_voc_stop(void);

bk_err_t dual_device_transmission_udp_client_init(video_device_t *video_dev);

bk_err_t dual_device_transmission_udp_client_deinit(void);

bk_err_t dual_device_transmission_tcp_client_init(video_device_t *video_dev);

bk_err_t dual_device_transmission_tcp_client_deinit(void);



#ifdef __cplusplus
}
#endif


