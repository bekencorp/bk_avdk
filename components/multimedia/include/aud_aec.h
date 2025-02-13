#pragma once

#include <driver/audio_ring_buff.h>
#include <modules/aec.h>
#include "aud_intf_private.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	AECContext* aec;
	aec_config_t *aec_config;
	uint32_t samp_rate;        //the sample rate of AEC
	uint16_t samp_rate_points; //the number of points in AEC frame
	int16_t* ref_addr;
	int16_t* mic_addr;
	int16_t* out_addr;
	int16_t *aec_ref_ring_buff; //save ref data of aec
	int16_t *aec_out_ring_buff; //save audio data processed by AEC
	RingBufferContext ref_rb;   //ref data of AEC context
	RingBufferContext aec_rb;   //out data of AEC context
} aec_info_t;


bk_err_t aud_aec_init(aec_info_t *aec_info);

bk_err_t aud_aec_deinit(aec_info_t *aec_info);

bk_err_t aud_aec_proc(aec_info_t *aec_info);

bk_err_t aud_aec_set_para(aec_info_t *aec_info, aud_intf_voc_aec_ctl_t *aec_ctl);

bk_err_t aud_aec_set_mic_delay(AECContext *aec, uint32_t value);

bk_err_t aud_aec_print_para(aec_config_t *aec_config);


#ifdef __cplusplus
}
#endif

