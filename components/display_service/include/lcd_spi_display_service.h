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

#include <driver/lcd_types.h>
#include <driver/spi.h>


#ifdef __cplusplus
extern "C" {
#endif

#ifndef LCD_SPI_DEVICE_NUM
    #ifdef CONFIG_LCD_SPI_DEVICE_NUM
        #define LCD_SPI_DEVICE_NUM CONFIG_LCD_SPI_DEVICE_NUM
    #else
        #define LCD_SPI_DEVICE_NUM 1
    #endif
#endif

#if (LCD_SPI_DEVICE_NUM > 1)
#define LCD_SPI_ID0   0
#define LCD_SPI_ID1   1
#else
#define LCD_SPI_ID    0
#endif

void lcd_spi_backlight_open(void);

void lcd_spi_backlight_close(void);

void lcd_spi_init(uint8_t id, const lcd_device_t *device);

void lcd_spi_deinit(uint8_t id);

void lcd_spi_display_frame(uint8_t id, uint8_t *frame_buffer, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

