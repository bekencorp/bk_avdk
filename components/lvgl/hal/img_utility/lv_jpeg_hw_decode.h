#ifndef __LV_JPEG_HW_DECODE_H_
#define __LV_JPEG_HW_DECODE_H_

#include "lvgl.h"

typedef enum {
    JH_OUTPUT_RGB565,
    JH_OUTPUT_YUYV,
} JPEG_HW_OUTPUT_FMT_T;


s32 lv_jpeg_hw_decode(frame_buffer_t *jpeg_frame, lv_img_dsc_t *img_dst);

void lv_jpeg_hw_decode_output_fmt_set(JPEG_HW_OUTPUT_FMT_T jpeg_output_fmt);

#endif

