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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	AUDIO_PLAY_IDLE = 0,
	AUDIO_PLAY_EXIT,
	AUDIO_PLAY_MAX
} audio_play_op_t;

typedef struct {
	audio_play_op_t op;
	void *param;
} audio_play_msg_t;

bk_err_t audio_play_sdcard_mp3_music_start(char *file_name);

bk_err_t audio_play_sdcard_mp3_music_stop(void);

#ifdef __cplusplus
}
#endif


