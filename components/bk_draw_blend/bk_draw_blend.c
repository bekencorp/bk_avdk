
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

#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <stdio.h>
#include "frame_buffer.h"
#include "bk_draw_blend.h"

#define TAG "bk_blend"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#ifdef DRAW_DIAG_DEBUG
#define DRAW_START()        do { GPIO_UP(GPIO_DVP_D3); } while (0)
#define DRAW_END()          do { GPIO_DOWN(GPIO_DVP_D3); } while (0)
#else
#define DRAW_START()
#define DRAW_END()
#endif


/**
 * @brief  blend font by cpu
 * @param  blend background layer frame
 * @param  blend panel lcd width, to calculate postion in panel by (x, y) pos
 * @param  blend panel lcd heighe, to calculate postion in panel by (x, y) pos
 * @return 
 *     - BK_OK: no error
 *     - BK_FAIL:not find blend image
 */
bk_err_t bk_display_blend_font_handle(frame_buffer_t *frame, uint16_t lcd_width, uint16_t lcd_height, const blend_info_t *font_info)
{
    if ((frame == NULL) && (font_info == NULL) && (font_info->addr == NULL))
    {
        LOGI("%s %d ERROR \n", __func__, __LINE__);
        return BK_FAIL;
    }

#if (CONFIG_BLEND)
    const bk_blend_t *font_strings = font_info->addr;
    font_blend_cfg_t cfg = {0};
    cfg.pbg_addr = (uint8_t *)(frame->frame);
    cfg.xsize = font_strings->width;
    cfg.ysize = font_strings->height;
    cfg.xpos = font_strings->xpos;
    cfg.ypos = font_strings->ypos;
    
    cfg.str_num = 1;
    if (frame->fmt == PIXEL_FMT_VUYY)
    {
        cfg.font_format = FONT_VUYY;
    }
    else if (frame->fmt == PIXEL_FMT_YUYV)
    {
        cfg.font_format = FONT_YUYV;
    }
    else
    {
        cfg.font_format = FONT_RGB565;
    }
    cfg.str[0] = (font_str_t)
    {
        (const char *)font_info->content, font_strings->font.color, font_strings->font.font_digit_type, 0, 0
    };
    cfg.bg_data_format = frame->fmt;
    cfg.bg_width = frame->width;
    cfg.bg_height = frame->height;
    cfg.lcd_width = lcd_width;
    cfg.lcd_height = lcd_height;
    bk_font_blend(&cfg);
#endif
    return BK_OK;
}


/**
 * @brief  only blend icon of ARGB888 image by cpu or hardware dma2d
 * @param  blend background layer frame
 * @param  blend panel lcd width, to calculate postion in panel by (x, y) pos
 * @param  blend panel lcd heighe, to calculate postion in panel by (x, y) pos
 * @return 
 *     - BK_OK: no error
 *     - BK_FAIL:not find blend image
 */
bk_err_t bk_display_blend_img_handle(frame_buffer_t *frame, uint16_t lcd_width, uint16_t lcd_height, const bk_blend_t *img_info)
{
    if ((frame == NULL) && (img_info == NULL))
    {
        return BK_FAIL;
    }
#if CONFIG_LCD_DMA2D_BLEND
    /**>  b7258 hw rotate output RGB565(big endian) is incompatible with DMA2D input RGB565 data format.*/
    image_blend_cfg_t cfg = {0};
    const bk_blend_t *img_dsc = (bk_blend_t *)img_info;
    cfg.pfg_addr = (uint8_t *)img_dsc->image.data;
    cfg.pbg_addr = (uint8_t *)(frame->frame);
    cfg.xsize = img_dsc->width;
    cfg.ysize = img_dsc->height;
    cfg.xpos = img_dsc->xpos;
    cfg.ypos = img_dsc->ypos;
    cfg.fg_alpha_value = 0xFF;
    cfg.fg_data_format = img_dsc->image.format;
    cfg.bg_data_format = frame->fmt;
    cfg.bg_width = frame->width;
    cfg.bg_height = frame->height;
    cfg.lcd_width = lcd_width;
    cfg.lcd_height = lcd_height;
    bk_dma2d_image_blend(&cfg);
#elif CONFIG_BLEND
    image_blend_cfg_t cfg = {0};
    const bk_blend_t *img_dsc = (bk_blend_t *)img_info;
    cfg.pfg_addr = (uint8_t *)img_dsc->image.data;
    cfg.pbg_addr = (uint8_t *)(frame->frame);
    cfg.xpos = img_dsc->xpos;
    cfg.ypos = img_dsc->ypos;
    cfg.xsize = img_dsc->width;
    cfg.ysize = img_dsc->height;
    cfg.fg_alpha_value = 0xff;
    cfg.fg_data_format = img_dsc->image.format;
    cfg.bg_data_format = frame->fmt;
    cfg.bg_width = frame->width;
    cfg.bg_height = frame->height;
    cfg.lcd_width = lcd_width;
    cfg.lcd_height = lcd_height;
    bk_image_blend(&cfg);
#endif
    return BK_OK;
}


bk_err_t bk_display_blend_handle(frame_buffer_t *frame, uint16_t lcd_width, uint16_t lcd_height, const blend_info_t *array, uint8_t array_size)
{
#if (CONFIG_BLEND)
    for (int i = 0; i < array_size; i++)
    {
        if ((array[i].addr != NULL) && array[i].addr->enable)
        {
            if (array[i].addr->blend_type == BLEND_TYPE_FONT)
                bk_display_blend_font_handle(frame, lcd_width, lcd_height, &array[i]);
            else if (array[i].addr->blend_type == BLEND_TYPE_IMAGE)
                bk_display_blend_img_handle(frame, lcd_width, lcd_height, array[i].addr);
        }
        else if (array[i].find_addr != NULL)
        {
            int j = 0;
            const bk_blend_t *(*temp_1)[0] = (array[i].find_addr);
             while ((*temp_1)[j] != NULL) 
             {
                if (os_strcmp((*temp_1)[j]->name, array[i].content) == 0)
                {
                    break;
                }
                j++;
            };
            if ((*temp_1)[j] != NULL)
            {
                bk_display_blend_img_handle(frame, lcd_width, lcd_height, (*temp_1)[j]);
            }
            else
            {
                LOGI("%s %d, i=%d, j=%d not find img %s \n", __func__, __LINE__, i,j, array[i].content);
            }
        }
        else
        {
        }
    }
#endif
    return BK_OK;
}

