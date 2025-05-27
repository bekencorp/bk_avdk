// Copyright 2022 Mobvoi Inc. All Rights Reserved.
// Author: chuhong.zhuang@mobvoi.com

#ifndef __MOBVOI_BK7258_PIPELINE_H__
#define __MOBVOI_BK7258_PIPELINE_H__

#ifdef __cplusplus
extern "C" {
#endif

unsigned int mobvoi_dsp_get_memory_needed(void);

void mobvoi_dsp_set_memory_base(void *base, unsigned int total);

void* mobvoi_bk7258_pipeline_init(int sample_rate, int drc_gain);


int mobvoi_bk7258_pipeline_process(void* instance, 
                                   const int16* mic0, 
                                   const int16* mic1,
                                   int16* out);

void mobvoi_bk7258_pipeline_cleanup(void* instance);

#ifdef __cplusplus
}
#endif

#endif

