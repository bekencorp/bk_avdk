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

#include <common/bk_include.h>
#include <driver/media_types.h>

#include "media_mailbox_list_util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	media_storage_state_t state;
	media_storage_state_t capture_state;
	void *node;
	uint32_t param;
} storage_info_t;

typedef struct
{
	uint32_t flash_image_addr;
	uint32_t flasg_img_length;
} storage_flash_t;

typedef struct
{
	uint8_t type;
	uint32_t data;
} storages_task_msg_t;

typedef enum
{
	STORAGE_TASK_CAPTURE,
	STORAGE_TASK_SAVE,
	STORAGE_TASK_SAVE_STOP,
	STORAGE_TASK_EXIT,
} storage_task_evt_t;

typedef enum
{
	STORAGE_PICTURE_MODE = 0,
	STORAGE_STREAM_MODE,
} storage_mode_t;

bk_err_t storage_app_set_frame_name(char *name);
bk_err_t storage_app_event_handle(media_mailbox_msg_t *msg);
bk_err_t storage_major_event_handle(media_mailbox_msg_t *msg);
bk_err_t bk_sdcard_read_to_mem(char *filename, uint32_t *paddr, uint32_t *total_len);
bk_err_t bk_mem_save_to_sdcard(char *filename, uint8_t *paddr, uint32_t total_len);
bk_err_t bk_mem_save_to_flash(char *filename, uint8_t *paddr, uint32_t total_len, storage_flash_t **info);
bk_err_t bk_mem_append_save_to_sdcard(char *filename, uint8_t *paddr, uint32_t total_len);
bk_err_t bk_read_sdcard_file_length(char *filename);


#ifdef __cplusplus
}
#endif
