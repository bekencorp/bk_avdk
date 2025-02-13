#pragma once

#include <driver/audio_ring_buff.h>
#include <modules/aec.h>
//#include <driver/aud_types.h>
#include <driver/dma.h>
#include "aud_intf_private.h"
#include <driver/uac.h>
#include "media_mailbox_list_util.h"
#include <driver/psram_types.h>
#include <driver/psram.h>
#include "aud_aec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	AUD_TRAS_DRV_IDLE = 0,

	/* aud intf op */
	AUD_TRAS_DRV_INIT,
	AUD_TRAS_DRV_DEINIT,		/**< deinit aud tras drv */
	AUD_TRAS_DRV_SET_MODE,		/**< set work mode */

	/* voc op */
	AUD_TRAS_DRV_VOC_INIT,		/**< init voc */
	AUD_TRAS_DRV_VOC_DEINIT,	/**< deinit voc */
	AUD_TRAS_DRV_VOC_START,		/**< start voc */
	AUD_TRAS_DRV_VOC_STOP,		/**< stop voc */
	AUD_TRAS_DRV_VOC_CTRL_MIC,	/**< set voc mic enable */
	AUD_TRAS_DRV_VOC_CTRL_SPK,	/**< set voc spk enable */
	AUD_TRAS_DRV_VOC_CTRL_AEC,	/**< set voc aec enable */
	AUD_TRAS_DRV_VOC_SET_MIC_GAIN,		/**< set audio adc gain */
	AUD_TRAS_DRV_VOC_SET_SPK_GAIN,		/**< set audio dac gain */
	AUD_TRAS_DRV_VOC_SET_AEC_PARA,		/**< set AEC parameters */
	AUD_TRAS_DRV_VOC_GET_AEC_PARA,		/**< get AEC parameters */
	/* voc int op */
	AUD_TRAS_DRV_AEC,			/**< aec process mic data */
	AUD_TRAS_DRV_ENCODER,		/**< encoder mic data processed by aec */
	AUD_TRAS_DRV_DECODER,		/**< decoder speaker data encoded */
//	AUD_TRAS_DRV_TX_DONE,

	AUD_TRAS_DRV_START,
	AUD_TRAS_DRV_EXIT,

	AUD_TRAS_DRV_CONTROL,

	/* UAC op */
	AUD_TRAS_DRV_UAC_REGIS_CONT_STATE_CB,		/**< register uac mic and speaker connect state callback */
	AUD_TRAS_DRV_UAC_MIC_CONT,					/**< recover uac mic status after uac automatically connect */
	AUD_TRAS_DRV_UAC_MIC_DISCONT,				/**< uac mic abnormal disconnect */
	AUD_TRAS_DRV_UAC_SPK_CONT,					/**< recover uac speaker status after uac automatically connect */
	AUD_TRAS_DRV_UAC_SPK_DISCONT,				/**< uac speaker abnormal disconnect */
	AUD_TRAS_DRV_UAC_AUTO_CONT_CTRL,			/**< uac automatically connect enable control*/

	/* debug op */
#if CONFIG_AUD_TRAS_DAC_DEBUG
	AUD_TRAS_VOC_DAC_BEBUG,
#endif

	AUD_TRAS_DRV_MAX,
} aud_tras_drv_op_t;

typedef struct {
	aud_tras_drv_op_t op;
	void *param;
} aud_tras_drv_msg_t;

typedef enum {
	EVENT_AUD_TRAS_DRV_INIT_CMP,
	EVENT_AUD_TRAS_DRV_START_CMP,
	EVENT_AUD_TRAS_DRV_STOP_CMP,
	EVENT_AUD_TRAS_DRV_MAX,
} aud_tras_drv_event_t;

/*
typedef struct {
	void (*audio_transfer_event_cb)(audio_tras_event_t event);
	int (*audio_send_mic_data)(unsigned char *data, unsigned int len);
} aud_cb_t;
*/

/* temporary data buffer used in voice transfer mode */
typedef struct {
	int16_t *pcm_data;
	uint8_t *law_data;
} aud_tras_drv_encode_temp_t;

typedef struct {
	int16_t *pcm_data;
	uint8_t *law_data;
} aud_tras_drv_decode_temp_t;

/* voice transfer status */
typedef enum {
	AUD_TRAS_DRV_VOC_STA_NULL = 0,		/**< default status: the voice is not init */
	AUD_TRAS_DRV_VOC_STA_IDLE,			/**< idle status: the voice is init */
	AUD_TRAS_DRV_VOC_STA_START, 		/**< start status: the voice is playing */
	AUD_TRAS_DRV_VOC_STA_STOP,			/**< stop status: the voice is stop */
	AUD_TRAS_DRV_VOC_STA_MAX,
} aud_tras_voc_sta_t;

typedef struct {
	uint32_t buff_size;
	uint8_t *buff_addr;
} aud_tras_uac_buff_t;

typedef struct {
	//audio_tras_drv_mode_t mode;			//AUD_TRAS_DRV_MODE_CPU0: audio transfer work in cpu0, AUD_TRAS_DRV_MODE_CPU1:audio transfer work in cpu1
	aud_tras_voc_sta_t status;
	bool aec_enable;
	aec_info_t *aec_info;

	dma_id_t adc_dma_id;				//audio transfer ADC DMA id
	uint16_t mic_samp_rate_points;		//the number of points in mic frame
	uint8_t mic_frame_number;			//the max frame number of mic ring buffer
	int32_t *mic_ring_buff;				//save mic data from audio adc
	RingBufferContext mic_rb;			//mic ring buffer context

	uint16_t speaker_samp_rate_points;	//the number of points in speaker frame
	uint8_t speaker_frame_number;		//the max frame number of speaker ring buffer
	int32_t *speaker_ring_buff;			//save dac data of speaker
	RingBufferContext speaker_rb;		//speaker ring buffer context
	dma_id_t dac_dma_id;				//audio transfer DAC DMA id

	aud_adc_config_t *adc_config;
	aud_dac_config_t *dac_config;
	tx_info_t tx_info;
	rx_info_t rx_info;			//rx_context shared by cpu0 and cpu1, cpu0 malloc

	/* audio transfer callback */
	void (*aud_tras_drv_voc_event_cb)(aud_tras_drv_voc_event_t event, bk_err_t result);

	RingBufferContext *aud_tx_rb;

	aud_intf_voc_data_type_t data_type;
	aud_intf_voc_mic_ctrl_t mic_en;
	aud_intf_voc_spk_ctrl_t spk_en;
	aud_intf_mic_type_t mic_type;			/**< audio mic type: uac or microphone */
	aud_intf_spk_type_t spk_type;			/**< audio speaker type: uac or speaker */

	E_USB_HUB_PORT_INDEX mic_port_index;
	E_USB_HUB_PORT_INDEX spk_port_index;

	aud_tras_drv_encode_temp_t encoder_temp;
	aud_tras_drv_decode_temp_t decoder_temp;
	uint8_t *uac_spk_buff;			//uac speaker read data buffer
	uint32_t uac_spk_buff_size; 	//uac speaker read data buffer size (byte)
	aud_uac_config_t *uac_config;	//uac_config
	aud_tras_uac_buff_t uac_urb_mic_buff;
	aud_tras_uac_buff_t uac_urb_spk_buff;

	bk_usb_hub_port_info *mic_port_info[4];
	bk_usb_hub_port_info *spk_port_info[4];
} aud_tras_drv_voc_info_t;

/******************************** aud tras drv info ****************************************/
/* audio transfer driver status */
typedef enum {
	AUD_TRAS_DRV_STA_NULL = 0,
	AUD_TRAS_DRV_STA_IDLE,
	AUD_TRAS_DRV_STA_WORK,
	AUD_TRAS_DRV_STA_MAX,
} aud_tras_drv_sta_t;

/* audio transfer driver information */
typedef struct {
	aud_intf_work_mode_t work_mode;
	aud_tras_drv_sta_t status;
	aud_tras_drv_voc_info_t voc_info;

	/* callbacks */
	int (*aud_tras_tx_mic_data)(unsigned char *data, unsigned int size);	/**< the api is called when collecting a frame mic packet data is complete */
	bk_err_t (*aud_tras_rx_spk_data)(unsigned int size);					/**< the api is called when playing a frame speaker packet data is complete */
	void (*aud_tras_drv_com_event_cb)(aud_tras_drv_com_event_t event, bk_err_t result);

	/* uac connect state callback */
	aud_intf_uac_sta_t uac_mic_status;
	aud_intf_uac_sta_t uac_spk_status;
	bool uac_mic_open_status;
	bool uac_spk_open_status;
	bool uac_mic_open_current;
	bool uac_spk_open_current;
	bool uac_auto_connect;
	bool uac_connect_state_cb_exist;
	void (*aud_tras_drv_uac_connect_state_cb)(uint8_t state);		/**< the api is called when uac abnormal disconnet and recover connect */
} aud_tras_drv_info_t;

#define DEFAULT_AUD_TRAS_DRV_INFO() {                                              \
        .work_mode = AUD_INTF_WORK_MODE_NULL,                                      \
        .status = AUD_TRAS_DRV_STA_NULL,                                           \
        .voc_info = {                                                              \
                        .status = AUD_TRAS_DRV_VOC_STA_NULL,                       \
                        .aec_enable = false,                                       \
                        .aec_info = NULL,                                          \
                        .adc_dma_id = DMA_ID_MAX,                                  \
                        .mic_samp_rate_points = 0,                                 \
                        .mic_frame_number = 0,                                     \
                        .mic_ring_buff = NULL,                                     \
                        .mic_rb = {                                                \
                                      .address = NULL,                             \
                                      .capacity = 0,                               \
                                      .wp = 0,                                     \
                                      .rp = 0,                                     \
                                      .dma_id = DMA_ID_MAX,                        \
                                      .dma_type = 0,                               \
                                  },                                               \
                        .speaker_samp_rate_points = 0,                             \
                        .speaker_frame_number = 0,                                 \
                        .speaker_ring_buff =NULL,                                  \
                        .speaker_rb = {                                            \
                                          .address = NULL,                         \
                                          .capacity = 0,                           \
                                          .wp = 0,                                 \
                                          .rp = 0,                                 \
                                          .dma_id = DMA_ID_MAX,                    \
                                          .dma_type = 0,                           \
                                      },                                           \
                        .dac_dma_id = DMA_ID_MAX,                                  \
                        .adc_config = NULL,                                        \
                        .dac_config = NULL,                                        \
                        .tx_info = {                                               \
                                       .tx_buff_status = false,                    \
                                       .ping = {                                   \
                                                   .buff_addr = NULL,              \
                                                   .busy_status = false,           \
                                               },                                  \
                                       .pang = {                                   \
                                                   .buff_addr = NULL,              \
                                                   .busy_status = false,           \
                                               },                                  \
                                       .buff_length = 0,                           \
                                   },                                              \
                        .rx_info = {                                               \
                                       .rx_buff_status = false,                    \
                                       .decoder_ring_buff = NULL,                  \
                                       .decoder_rb = NULL,                         \
                                       .frame_size = 0,                            \
                                       .frame_num = 0,                             \
                                       .rx_buff_seq_tail = 0,                      \
                                       .aud_trs_read_seq = 0,                      \
                                       .fifo_frame_num = 0,                        \
                                   },                                              \
                        .aud_tx_rb = NULL,                                         \
                        .data_type = AUD_INTF_VOC_DATA_TYPE_MAX,                   \
                        .mic_en = AUD_INTF_VOC_MIC_MAX,                            \
                        .spk_en = AUD_INTF_VOC_SPK_MAX,                            \
                        .mic_type = AUD_INTF_MIC_TYPE_MAX,                         \
                        .spk_type = AUD_INTF_SPK_TYPE_MAX,                         \
                        .mic_port_index = USB_HUB_PORT_1,                          \
                        .spk_port_index = USB_HUB_PORT_1,                          \
                        .encoder_temp = {                                          \
                                            .pcm_data = NULL,                      \
                                            .law_data = NULL,                      \
                                        },                                         \
                        .decoder_temp = {                                          \
                                            .pcm_data = NULL,                      \
                                            .law_data = NULL,                      \
                                        },                                         \
                        .uac_spk_buff = NULL,                                      \
                        .uac_spk_buff_size = 0,                                    \
                        .uac_config = NULL,                                        \
                        .uac_urb_mic_buff = {                                      \
                                                .buff_size = 0,                    \
                                                .buff_addr = NULL,                 \
                                            },                                     \
                        .uac_urb_spk_buff = {                                      \
                                                .buff_size = 0,                    \
                                                .buff_addr = NULL,                 \
                                            },                                     \
                        .mic_port_info = {                                         \
                                             NULL, NULL, NULL, NULL                \
                                         },                                        \
                        .spk_port_info = {                                         \
                                             NULL, NULL, NULL, NULL                \
                                         },                                        \
                    },                                                             \
        .aud_tras_tx_mic_data = NULL,                                              \
        .aud_tras_rx_spk_data = NULL,                                              \
        .uac_mic_status = AUD_INTF_UAC_MIC_NORMAL_DISCONNECTED,                    \
        .uac_spk_status = AUD_INTF_UAC_SPK_NORMAL_DISCONNECTED,                    \
        .uac_mic_open_status = false,                                              \
        .uac_spk_open_status = false,                                              \
        .uac_mic_open_current = false,                                             \
        .uac_spk_open_current = false,                                             \
        .uac_auto_connect = true,                                                  \
        .uac_connect_state_cb_exist = false,                                       \
        .aud_tras_drv_uac_connect_state_cb = NULL,                                 \
    }

/* audio transfer mic information in general mode */
typedef struct {
	uint32_t ptr_data;		/**< the mic data need to send */
	uint32_t length;		/**< the data size (byte) */
} aud_tras_drv_mic_notify_t;



bk_err_t aud_tras_drv_init(aud_intf_drv_config_t *setup_cfg);

bk_err_t aud_tras_drv_deinit(void);

bk_err_t aud_tras_drv_send_msg(aud_tras_drv_op_t op, void *param);

bk_err_t audio_event_handle(media_mailbox_msg_t * msg);

void *audio_tras_drv_malloc(uint32_t size);

void audio_tras_drv_free(void *mem);


#ifdef __cplusplus
}
#endif

