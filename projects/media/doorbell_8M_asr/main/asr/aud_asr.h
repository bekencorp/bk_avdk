// Copyright 2023-2024 Beken
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

#ifndef __AUD_ASR_H__
#define __AUD_ASR_H__

#if CONFIG_AUD_ASR

#include <os/os.h>
#include "aud_intf_types.h"

typedef enum {
	AUD_ASR_CMD_SEND_MIC_DATA = 0,
	AUD_ASR_CMD_RSP_RESULT,
	AUD_ASR_CMD_CP2_STARTUP,
}aud_asr_cmd_t;

typedef struct aud_asr_mb_data{
	uint8_t cmd;
	uint32_t param1;
	uint32_t param2;
}aud_asr_mb_data_t;

#endif

#endif
