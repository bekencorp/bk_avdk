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
#include "lcd_disp_hal.h"
//#include "include/bk_lcd_commands.h"


#define TAG "ILI9488"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)

#define COMMAND_1 0x11

#define COMMAND_2 0x36
static uint32_t param_Command2[] = {0x48};

#define COMMAND_3 0x3A
static uint32_t param_Command3[] = {0x77};

#define COMMAND_4 0xF0
static uint32_t param_Command4[] = {0xC3};

#define COMMAND_5 0xF0
static uint32_t param_Command5[] = {0x96};

#define COMMAND_6 0xB0
static uint32_t param_Command6[] = {0x80};

#define COMMAND_7 0xB1
static uint32_t param_Command7[] = {0x80, 0x10};

#define COMMAND_8 0xB4
static uint32_t param_Command8[] = {0x01};

#define COMMAND_9 0xB6
static uint32_t param_Command9[] = {0x20, 0x02, 0x3B};

#define COMMAND_10 0xB7
static uint32_t param_Command10[] = {0xC6};

#define COMMAND_11 0xB9
static uint32_t param_Command11[] = {0x02};

#define COMMAND_12 0xC0
static uint32_t param_Command12[] = {0x80, 0x07};

#define COMMAND_13 0xC1
static uint32_t param_Command13[] = {0x09};

#define COMMAND_14 0xC2
static uint32_t param_Command14[] = {0xA7};

#define COMMAND_15 0xC5
static uint32_t param_Command15[] = {0x13};

#define COMMAND_16 0xE8
static uint32_t param_Command16[] = {0x40, 0x8A, 0x00, 0x00, 0x29, 0x1C, 0xAA, 0x33};

#define COMMAND_17 0xE0
static uint32_t param_Command17[] = {0xD2, 0x05, 0x08, 0x06, 0x05, 0x02, 0x2A, 0x44, 0x46, 0x39, 0x15, 0x15, 0x2D, 0x32};

#define COMMAND_18 0xE1
static uint32_t param_Command18[] = {0x96, 0x08, 0x0C, 0x09, 0x09, 0x25, 0x2E, 0x43, 0x42, 0x35, 0x11, 0x11, 0x28, 0x2E};

#define COMMAND_19 0x21

#define COMMAND_20 0x29


static bk_err_t lcd_ili9488_swap_xy(bool swap_axes)
{
	LOGI("%s\n", __func__);
	return BK_OK;
}

static bk_err_t lcd_ili9488_mirror(bool mirror_x, bool mirror_y)
{
	LOGI("%s\n", __func__);
	return BK_OK;
}

bk_err_t ili9488_lcd_on(void)
{
	LOGI("%s\n", __func__);
	return BK_OK;
}

static bk_err_t ili9488_lcd_off(void)
{
	LOGI("%s\n", __func__);
	return BK_OK;
}

bk_err_t ili9488_swreset(void)
{
	return BK_OK;
}

void lcd_ili9488_init(void)
{
	LOGI("%s\n", __func__);

	rtos_delay_milliseconds(120);

	lcd_hal_8080_cmd_send(0, COMMAND_1, NULL);

	rtos_delay_milliseconds(120);

	lcd_hal_8080_cmd_send(sizeof(param_Command2) / sizeof(param_Command2[0]), COMMAND_2, param_Command2);
	lcd_hal_8080_cmd_send(sizeof(param_Command3) / sizeof(param_Command3[0]), COMMAND_3, param_Command3);
	lcd_hal_8080_cmd_send(sizeof(param_Command4) / sizeof(param_Command4[0]), COMMAND_4, param_Command4);
	lcd_hal_8080_cmd_send(sizeof(param_Command5) / sizeof(param_Command5[0]), COMMAND_5, param_Command5);
	lcd_hal_8080_cmd_send(sizeof(param_Command6) / sizeof(param_Command6[0]), COMMAND_6, param_Command6);
	lcd_hal_8080_cmd_send(sizeof(param_Command7) / sizeof(param_Command7[0]), COMMAND_7, param_Command7);
	lcd_hal_8080_cmd_send(sizeof(param_Command8) / sizeof(param_Command8[0]), COMMAND_8, param_Command8);
	lcd_hal_8080_cmd_send(sizeof(param_Command9) / sizeof(param_Command9[0]), COMMAND_9, param_Command9);
	lcd_hal_8080_cmd_send(sizeof(param_Command10) / sizeof(param_Command10[0]), COMMAND_10, param_Command10);
	lcd_hal_8080_cmd_send(sizeof(param_Command11) / sizeof(param_Command11[0]), COMMAND_11, param_Command11);
	lcd_hal_8080_cmd_send(sizeof(param_Command12) / sizeof(param_Command12[0]), COMMAND_12, param_Command12);
	lcd_hal_8080_cmd_send(sizeof(param_Command13) / sizeof(param_Command13[0]), COMMAND_13, param_Command13);
	lcd_hal_8080_cmd_send(sizeof(param_Command14) / sizeof(param_Command14[0]), COMMAND_14, param_Command14);
	lcd_hal_8080_cmd_send(sizeof(param_Command15) / sizeof(param_Command15[0]), COMMAND_15, param_Command15);
	lcd_hal_8080_cmd_send(sizeof(param_Command16) / sizeof(param_Command16[0]), COMMAND_16, param_Command16);
	lcd_hal_8080_cmd_send(sizeof(param_Command17) / sizeof(param_Command17[0]), COMMAND_17, param_Command17);
	lcd_hal_8080_cmd_send(sizeof(param_Command18) / sizeof(param_Command18[0]), COMMAND_18, param_Command18);

    rtos_delay_milliseconds(120);

	lcd_hal_8080_cmd_send(0, COMMAND_19, NULL);
	lcd_hal_8080_cmd_send(0, COMMAND_20, NULL);
}

void lcd_ili9488_set_display_mem_area(uint16 xs, uint16 xe, uint16 ys, uint16 ye)
{

}

static const lcd_mcu_t lcd_mcu =
{
	.clk = LCD_60M,
	.set_xy_swap = lcd_ili9488_swap_xy,
	.set_mirror = lcd_ili9488_mirror,
	.set_display_area = lcd_ili9488_set_display_mem_area,
	.start_transform = NULL,
	.continue_transform = NULL,
};
    

const lcd_device_t lcd_device_st7796pi_mcu16 =
{
	.id = LCD_DEVICE_ST7796PI_MCU16,
	.name = "st7796pi_mcu16",
	.type = LCD_TYPE_MCU8080,
	.ppi = PPI_320X480,
	.mcu = &lcd_mcu,
	.src_fmt = PIXEL_FMT_RGB888,
	.out_fmt = PIXEL_FMT_RGB888_16BIT,
	.init = lcd_ili9488_init,
	.lcd_off = ili9488_lcd_off,
};


