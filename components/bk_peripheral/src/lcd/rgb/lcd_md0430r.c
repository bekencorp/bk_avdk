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

#include <driver/gpio.h>
#include <driver/media_types.h>
#include <driver/lcd_types.h>
#include "lcd_panel_devices.h"
#include "gpio_map.h"
#include <driver/lcd.h>



static const lcd_rgb_t lcd_rgb =
{
	.clk = LCD_32M,
	.data_out_clk_edge = POSEDGE_OUTPUT,

	.hsync_pulse_width = 2,
	.vsync_pulse_width = 2,
	.hsync_back_porch = 40,
	.hsync_front_porch = 48,
	.vsync_back_porch = 32,
	.vsync_front_porch = 13,
};

const lcd_device_t lcd_device_md0430r =
{
	.id = LCD_DEVICE_MD0430R,
	.name = "md0430r",
	.type = LCD_TYPE_RGB565,
	.ppi = PPI_800X480,
	.rgb = &lcd_rgb,
	.init = NULL,
	.lcd_off = NULL,
};

