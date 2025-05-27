Headset
======================================

:link_to_translation:`zh_CN:[中文]`

1. Function Overview
-------------------------------------
    This project show how headset work with main function: 
	
	| 1.Receive music data transmitted by the peer as a2dp sink
	| 2.Control the peer as avrcp ct/tg (play pause,etc.)
	| 3.Receive voice data transmitted by the peer as hfp hf
	| 4.ble gatt server/gatt client(see the introduction of Central project for details)

1.1 Specification
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    * a2dp:
        * avdtp sink
    * avrcp:
        * tg
        * ct
    * hfp:
        * hf
    * ble:
        * gap
        * gatt server
        * gatt client
        * smp legacy pair/secure connection pair

1.2 Code Path and build cmd
-------------------------------------
	demo:`./projects/bluetooth/headset <https://gitlab.bekencorp.com/wifi/armino/-/tree/main/projects/bluetooth/headset>`_

	build cmd:``make bk7258 PROJECT=bluetooth/headset``

2. Introduction to cli commands
-------------------------------------

2.1 a2dp/avrcp
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    +-------------------------------------------+---------------------+
    | headset connect <xx:xx:xx:xx:xx:xx>       | connect             |
    +-------------------------------------------+---------------------+
    | headset disconnect <xx:xx:xx:xx:xx:xx>    | disconnect          |
    +-------------------------------------------+---------------------+
    | headset play                              | play                |
    +-------------------------------------------+---------------------+
    | headset pause                             | pause               |
    +-------------------------------------------+---------------------+
    | headset next                              | next song           |
    +-------------------------------------------+---------------------+
    | headset prev                              | prev song           |
    +-------------------------------------------+---------------------+
    | headset rewind [msec]                     | rewind              |
    +-------------------------------------------+---------------------+
    | headset fast_forward [msec]               | fast forward        |
    +-------------------------------------------+---------------------+
    | headset vol_up                            | volume up           |
    +-------------------------------------------+---------------------+
    | headset vol_down                          | volume down         |
    +-------------------------------------------+---------------------+
    | headset pair_mode                         | enter pairing mode  |
    +-------------------------------------------+---------------------+

3. Frame Diagram
---------------------------------

3.1 Software Module Architecture Diagram
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    As shown in the figure below,bluetooth is responsible for receiving music data, receiving and sending voice data;
	audio is responsible for decoding and data,playing music and voice data, and collecting and encoding MIC data;
	storage is responsible for storing bluetooth information,such as encryption keys.

.. figure:: ../../../../_static/bluetooth_headset_arch.png
    :align: center
    :alt: module architecture Overview
    :figclass: align-center

    Figure 1. software module architecture

3.2 Flowchart
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	A2DP workflow and HFP workflow are shown in the figure below:

.. figure:: ../../../../_static/bluetooth_headset_a2dp_flow.png
    :align: center
    :alt: a2dp flow Overview
    :figclass: align-center

    Figure 2. a2dp flow chart

.. figure:: ../../../../_static/bluetooth_headset_hfp_flow.png
    :align: center
    :alt: hfp flow Overview
    :figclass: align-center

    Figure 3. hfp flow chart

4. Configuration
---------------------------------

4.1 Demo enable configuration
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    In the project path config/bk7258/config,modify the macro to configure whether to enable A2DP and HFP demo.Currently,A2DP demo is enabled by default;

    | //enable A2DP demo
    | CONFIG_A2DP_SINK_DEMO=y

    | //enable HFP demo
    | CONFIG_HFP_HF_DEMO=y

4.2 Codec configuration
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

4.2.1 A2DP
.................................

    +---------------+--------------+----------------------+------------------------------------+
    |               |  CONFIG_SBC  |  CONFIG_AAC_DECODER  |  parameter of a2dp_sink_demo_init  |
    +---------------+--------------+----------------------+------------------------------------+
    |  support SBC  |       Y      |           N          |               0                    |
    +---------------+--------------+----------------------+------------------------------------+
    |  support AAC  |       Y      |           Y          |               1                    |
    +---------------+--------------+----------------------+------------------------------------+

4.2.1 HFP
.................................

    +---------------+--------------+---------------------------------+
    |               |  CONFIG_SBC  |  parameter of hfp_hf_demo_init  |
    +---------------+--------------+---------------------------------+
    | support CVSD  |       N      |               0                 |
    +---------------+--------------+---------------------------------+
    | support mSBC  |       Y      |               1                 |
    +---------------+--------------+---------------------------------+

5. Code Explanation
---------------------------------

5.1 A2DP demo
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

5.1.1 Get music data reported by BT
.......................................

::

	void bt_audio_sink_media_data_ind(const uint8_t *data, uint16_t data_len)
	{
		bt_audio_sink_demo_msg_t demo_msg;
		int rc = -1;

		os_memset(&demo_msg, 0x0, sizeof(bt_audio_sink_demo_msg_t));

		if (bt_audio_sink_demo_msg_que == NULL)
		{
			return;
		}

		demo_msg.data = (char *) os_malloc(data_len);

		if (demo_msg.data == NULL)
		{
			LOGE("%s, malloc failed\r\n", __func__);
			return;
		}

		os_memcpy(demo_msg.data, data, data_len);
		demo_msg.type = BT_AUDIO_D2DP_DATA_IND_MSG;
		demo_msg.len = data_len;

		//send to audio_sink_demo task
		rc = rtos_push_to_queue(&bt_audio_sink_demo_msg_que, &demo_msg, BEKEN_NO_WAIT);

		...
	}

5.1.2 Store music data in ring buffer
............................................

::

	void bt_audio_sink_demo_main(void *arg)
	{
		...

		case BT_AUDIO_D2DP_DATA_IND_MSG:
		{
			...
			
			uint8 *fb = (uint8_t *)msg.data;
			if(s_spk_is_started)
			{
				//store sbc format data into the ring buffer frame by frame
				if (CODEC_AUDIO_SBC == bt_audio_a2dp_sink_codec.type)
				{
					uint8_t payload_header = *fb++;
					uint8_t frame_num = payload_header&0xF;
					if(msg.len - 1 != frame_length*frame_num)
					{
						LOGI("recv undef sbc, payload_header %d, payload_len: %d, frame_num:%d \r\n", payload_header, msg.len - 1, frame_num);
					}
					for(uint8_t i=0;i<frame_num;i++)
					{
						if (ring_buffer_node_get_free_nodes(&s_a2dp_frame_nodes))
						{
							uint8_t *node = ring_buffer_node_get_write_node(&s_a2dp_frame_nodes);
							os_memcpy(node, fb, frame_length);
							fb += frame_length;
						}
						else
						{
							LOGI("A2DP frame nodes buffer(sbc) is full\n");
							//os_free(msg.data);
							break;
						}
					}
				}
	#if CONFIG_AAC_DECODER
				//First parse the frame length of the AAC format data, and then store it in the ring buffer according to the frame length
				else if(CODEC_AUDIO_AAC == bt_audio_a2dp_sink_codec.type)
				{
					// LOGI("-> %d \n", msg.len);
					uint8_t *inbuf = &fb[9];
					uint32_t inlen = 0;
					uint8_t  len   = 255;
					do
					{
						inlen += len = *inbuf++;
					}
					while (len == 255);
					{
						if (ring_buffer_node_get_free_nodes(&s_a2dp_frame_nodes))
						{
							uint8_t *node = ring_buffer_node_get_write_node(&s_a2dp_frame_nodes);
							*((uint32_t *)node) = inlen;
							os_memcpy(node + 4, inbuf, inlen);
						}
						else
						{
							LOGI("A2DP frame nodes buffer(aac) is full\n");
							os_free(msg.data);
							break;
						}
					}
				}
	#endif
				else
				{
					LOGE("%s, Unsupported a2dp codec %d \r\n", __func__, bt_audio_a2dp_sink_codec.type);
				}
				if(a2dp_speaker_sema)
				{
					rtos_set_semaphore(&a2dp_speaker_sema);
				}
			}
			
			...

		}
		break;

		...

	}

5.1.3 Decode music data
.................................

::

	static void speaker_task(void *arg)
	{
		...

		for (; i < s_frame; i++)
		{
			//get data from ring buffer
			uint8_t *inbuf = ring_buffer_node_get_read_node(&s_a2dp_frame_nodes);
			bk_err_t ret;

			if (CODEC_AUDIO_SBC == bt_audio_a2dp_sink_codec.type)
			{
				//perform sbc decoding
				ret = bk_sbc_decoder_frame_decode(&bt_audio_sink_sbc_decoder, inbuf, frame_length);
				if (ret < 0)
				{
					LOGE("sbc_decoder_decode error <%d>\n", ret);
					continue;
				}
				int16_t *dst = (int16_t*)bt_audio_sink_sbc_decoder.pcm_sample;
				int16_t w_len = bt_audio_sink_sbc_decoder.pcm_length * 4;
				if(CONFIG_BOARD_AUDIO_CHANNLE_NUM == 1)
				{
					for(int i=0; i<bt_audio_sink_sbc_decoder.pcm_length * 2; i++)
					{
						dst[i] = dst[i*2];
					}
					w_len = bt_audio_sink_sbc_decoder.pcm_length * 2;
				}
				//send decoded data to the speaker
				int size = raw_stream_write(raw_write, (char *)dst, w_len);
				if (size <= 0)
				{
					LOGE("raw_stream_write size fail: %d \n", size);
					break;
				}
				else
				{
					//LOGI("raw_stream_write size: %d \n", size);
				}
			}
	#if (CONFIG_AAC_DECODER)
			else if(CODEC_AUDIO_AAC == bt_audio_a2dp_sink_codec.type)
			{
				uint32_t len = *(uint32_t *)inbuf;
				// LOGI("<- %d \n", len);
				uint8_t *out_buf = 0;
				uint32_t out_len = 0;
				
				//perform aac decoding
				ret = bk_aac_decoder_decode(&bt_audio_sink_aac_decoder, (inbuf+4), len, &out_buf, &out_len);
				if(ret == 0)
				{
					int16_t *dst = (int16_t*)out_buf;
					int16_t w_len = out_len;
					if(CONFIG_BOARD_AUDIO_CHANNLE_NUM == 1)
					{
						for(uint16_t i=0;i<out_len/2;i++)
						{
							dst[i] = dst[i*2];
						}
						w_len = out_len/2;
					}
					//send decoded data to the speaker
					int size = raw_stream_write(raw_write, (char *)dst, w_len);
					if (size <= 0)
					{
						LOGE("raw_stream_write size fail: %d \n", size);
						break;
					}
					else
					{
						// LOGI("raw_stream_write size: %d \n", size);
					}
				}else
				{
					LOGE("sbc_decoder_decode error <%d>\n", ret);
				}
			}
	#endif
			else
			{
				LOGE("unsupported codec!! /n");
			}
		}

		...
	}

5.2 HFP demo
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

5.2.1 Get voice data reported by BT
.......................................

::

	void bt_audio_hfp_client_voice_data_ind(const uint8_t *data, uint16_t data_len)
	{
		bt_audio_hf_demo_msg_t demo_msg;
		int rc = -1;

		os_memset(&demo_msg, 0x0, sizeof(bt_audio_hf_demo_msg_t));
		if (bt_audio_hf_demo_msg_que == NULL)
			return;

		demo_msg.data = (char *) os_malloc(data_len);
		if (demo_msg.data == NULL)
		{
			LOGI("%s, malloc failed\r\n", __func__);
			return;
		}

		os_memcpy(demo_msg.data, data, data_len);
		demo_msg.type = BT_AUDIO_VOICE_IND_MSG;
		demo_msg.len = data_len;

		//send to audio_hf_demo task
		rc = rtos_push_to_queue(&bt_audio_hf_demo_msg_que, &demo_msg, BEKEN_NO_WAIT);
		...
	}

5.2.2 Decode voice data
............................................

::

	void bt_audio_hf_demo_main(void *arg)
	{
		...

		case BT_AUDIO_VOICE_IND_MSG:
		{
			...
			//decode msbc format voice data
			if (CODEC_VOICE_MSBC == bt_audio_hfp_hf_codec)
			{
				fb += 2; //Skip Synchronization Header
				ret = bk_sbc_decoder_frame_decode(&bt_audio_hf_sbc_decoder, fb, msg.len - 2);
	//                        LOGI("sbc decod %d \n", ret);
				if (ret < 0)
				{
					LOGE("msbc decode fail, ret:%d\n", ret);
				}
				else
				{
					ret = BK_OK;
					fb = (uint8_t*)bt_audio_hf_sbc_decoder.pcm_sample;
					packet_len = r_len = SCO_MSBC_SAMPLES_PER_FRAME*2;
					packet_num = 4;
				}
			}
			else
			{
				packet_len = r_len = SCO_CVSD_SAMPLES_PER_FRAME * 2;
				packet_num = 8;
			}

			if(ret == BK_OK)
			{
				...
				//store the original voice data into hf_speaker_buffer
				os_memcpy(hf_speaker_buffer + hf_speaker_data_count, fb, r_len);
				hf_speaker_data_count += r_len;
				if (hf_speaker_data_count >= packet_len * packet_num)
				{
					if (hf_speaker_sema)
					{
						rtos_set_semaphore(&hf_speaker_sema);
					}
					hf_speaker_data_count -= packet_len * packet_num;
				}
			}else
			{
				//LOGE("write spk data fail \r\n");
			}

			os_free(msg.data);
		}
		break;

		...
	}

5.2.3 Play voice data
............................................

::

	static void speaker_task(void *arg)
	{
		...

		while (hf_auido_start)
		{
			rtos_get_semaphore(&hf_speaker_sema, BEKEN_WAIT_FOREVER);

			//send the voice data in hf_speaker_buffer to SPEAKER
			int size = raw_stream_write(raw_write, (char *)hf_speaker_buffer, packet_len);

			if (size <= 0)
			{
				LOGE("raw_stream_write size: %d \n", size);
				break;
			}
			else
			{
				//LOGI("raw_stream_write size: %d \n", size);
			}
		}

	   ...
	}

5.2.4 Get uplink MIC data
.................................

::

	static void mic_task(void *arg)
	{
		...

		while (hf_auido_start)
		{
			if (hf_mic_data_count+read_size < sizeof(hf_mic_sco_data))
			{
				//get MIC data
				int size = raw_stream_read(raw_read, (char *)(hf_mic_sco_data + hf_mic_data_count), read_size);
				if (size > 0)
				{
					...

					while (hf_mic_data_count >= send_len)
					{
						//decide whether to perform msbc encoding based on the selected encoding format
						if (CODEC_VOICE_MSBC == bt_audio_hfp_hf_codec)
						{
							int32_t produced = sbc_encoder_encode(&bt_audio_hf_sbc_encoder, (int16_t *)(hf_mic_sco_data + send_len * i));
	//                        LOGI("[send_mic_data_to_air_msbc]  %d \r\n",produced);
							//send the encoded voice data to BT
							bk_bt_hf_client_voice_out_write(hfp_peer_addr, (uint8_t *)&bt_audio_hf_sbc_encoder.stream [ -2 ], produced + 2);
						}else
						{
							//send uncoded voice data to BT
							bk_bt_hf_client_voice_out_write(hfp_peer_addr, hf_mic_sco_data + send_len * i, send_len);
						}
						i++;
						hf_mic_data_count -= send_len;
					}
					if (hf_mic_data_count)
					{
						os_memmove(hf_mic_sco_data, hf_mic_sco_data + send_len * i, hf_mic_data_count);
					}
				}
			}
			else
			{
				LOGE("MIC BUFFER FULL \r\n");
				hf_mic_data_count = 0;
			}
		}

		...
	}



