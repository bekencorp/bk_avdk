#include <common/bk_include.h>

#include "doorbell_comm.h"
#include "doorbell_transmission.h"

#include "lwip/tcp.h"
#include "bk_uart.h"
#include <os/mem.h>
#include <os/os.h>
#include <common/bk_kernel_err.h>

#include "lwip/sockets.h"
#include "avdk_crc.h"

#include <components/video_transfer.h>

#if CONFIG_ARCH_CM33
#include <driver/aon_rtc.h>
#endif

#define TAG "db-tran"

#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

//#define DEBUG_DUMP
//#define DEBUG_HEAD
//#define DEBUG_CRC


#define CRC8_INIT_VALUE 0xFF

/*******************************************************************************
* crc8
*
* Computes a crc8 over the input data using the polynomial:
*
*       x^8 + x^7 +x^6 + x^4 + x^2 + 1
*
* The caller provides the initial value (either CRC8_INIT_VALUE
* or the previous returned value) to allow for processing of
* discontiguous blocks of data.  When generating the CRC the
* caller is responsible for complementing the final return value
* and inserting it into the byte stream.  When checking, a final
* return value of CRC8_GOOD_VALUE indicates a valid CRC.
*
* Reference: Dallas Semiconductor Application Note 27
*   Williams, Ross N., "A Painless Guide to CRC Error Detection Algorithms",
*     ver 3, Aug 1993, ross@guest.adelaide.edu.au, Rocksoft Pty Ltd.,
*     ftp://ftp.rocksoft.com/clients/rocksoft/papers/crc_v3.txt
*
* ****************************************************************************
*/

void doorbell_hex_dump(uint8_t *data, uint32_t length)
{
#ifdef DUMP_DEBUG
	for (int i = 0; i < length; i++)
	{
		os_printf("%02X ", data[i]);

		if ((i + 1) % 20 == 0)
		{
			os_printf("\n");
		}
	}
	os_printf("\n");
#endif
}

db_channel_t *doorbell_transmission_malloc(uint16_t max_rx_size, uint16_t max_tx_size)
{
	db_channel_t *db_channel = (db_channel_t *)os_malloc(sizeof(db_channel_t));

	if (db_channel == NULL)
	{
		LOGE("malloc db_channel failed\n");
		goto error;
	}

	os_memset(db_channel, 0, sizeof(db_channel_t));

	db_channel->cbuf = os_malloc(max_rx_size + sizeof(db_trans_head_t));

	if (db_channel->cbuf == NULL)
	{
		LOGE("malloc cache buffer failed\n");
		goto error;
	}

	db_channel->csize = max_rx_size + sizeof(db_trans_head_t);

	db_channel->tbuf = os_malloc(max_tx_size + sizeof(db_trans_head_t));

	if (db_channel->tbuf == NULL)
	{
		LOGE("malloc cache buffer failed\n");
		goto error;
	}

	db_channel->tsize = max_tx_size;

	LOGI("%s, %p, %p %d, %p %d\n", __func__, db_channel, db_channel->cbuf, db_channel->csize, db_channel->tbuf, db_channel->tsize);

	return db_channel;


error:

	if (db_channel->cbuf)
	{
		os_free(db_channel->cbuf);
		db_channel->cbuf = NULL;
	}

	if (db_channel)
	{
		os_free(db_channel);
		db_channel = NULL;
	}

	return db_channel;
}

void doorbell_transmission_unpack(db_channel_t *channel, uint8_t *data, uint32_t length, doorbell_transmission_recive_cb_t cb)
{
	db_trans_head_t head, *ptr;
	uint8_t *p = data;
	uint32_t left = length;
	int cp_len = 0;

#ifdef DUMP_DEBUG
	static uint32_t count = 0;

	LOGD("DUMP DATA %u, size: %u\n", count++, length);

	doorbell_hex_dump(data, length);
#else
	LOGD("recv unpack: %u\n", length);
#endif

	while (left != 0)
	{
		if (channel->ccount == 0)
		{
			if (left < HEAD_SIZE_TOTAL)
			{
				//LOGE("left head size not enough: %d, ccount: %d\n", left, channel->ccount);
				os_memcpy(channel->cbuf + channel->ccount, p, left);
				channel->ccount += left;
				break;
			}

			ptr = (db_trans_head_t *)p;

			head.magic = CHECK_ENDIAN_UINT16(ptr->magic);

			if (head.magic == HEAD_MAGIC_CODE)
			{
				/*
				*   Magic code  2 bytes
				*   Flags       2 bytes
				*   Timestamp   4 bytes
				*   Squence     2 bytes
				*   Length      2 bytes
				*   CRC         1 byte
				*   RESERVED    3 byte
				*/

				head.flags = CHECK_ENDIAN_UINT16(ptr->flags);
				head.timestamp = CHECK_ENDIAN_UINT32(ptr->timestamp);
				head.sequence = CHECK_ENDIAN_UINT16(ptr->sequence);
				head.length = CHECK_ENDIAN_UINT16(ptr->length);
				head.crc = ptr->crc;
				head.reserved[0] = ptr->reserved[0];
				head.reserved[1] = ptr->reserved[1];
				head.reserved[2] = ptr->reserved[2];
#ifdef DEBUG_HEAD
				LOGI("head size: %d, %d, flags: %04X\n", HEAD_SIZE_TOTAL, sizeof(db_trans_head_t), head.flags);
				LOGI("time: %u, len: %u, seq: %u, crc: %02X\n",
					head.timestamp, head.length, head.sequence, head.crc);
#endif
			}
			else
			{
				LOGE("invaild src data\n");
				doorbell_hex_dump(p, left);
				LOGE("dump src data\n");
				//TODO FIXME
				doorbell_hex_dump(data, length);
				break;
			}

			if (left < head.length + HEAD_SIZE_TOTAL)
			{
				//LOGE("left payload size not enough: %d, ccount: %d, pay len: %d\n", left, channel->ccount, head.length);
				os_memcpy(channel->cbuf + channel->ccount, p, left);
				channel->ccount += left;
				break;
			}

#ifdef DEBUG_CRC
			if (HEAD_FLAGS_CRC & head.flags)
			{

				uint8_t ret_crc = hnd_crc8(p + HEAD_SIZE_TOTAL, head.length, CRC8_INIT_VALUE);

				if (ret_crc != head.crc)
				{
					LOGI("check crc failed\n");
				}

				LOGI("CRC SRC: %02X,  CALC: %02X\n", head.crc, ret_crc);
			}
#endif

			if (cb)
			{
				cb(channel, head.sequence, head.flags, head.timestamp, head.sequence, ptr->payload, head.length);
			}

			p += HEAD_SIZE_TOTAL + head.length;
			left -= HEAD_SIZE_TOTAL + head.length;
		}
		else
		{
			if (channel->ccount < HEAD_SIZE_TOTAL)
			{
				cp_len = HEAD_SIZE_TOTAL - channel->ccount;

				if (cp_len < 0)
				{
					//LOGE("cp_len error: %d at %d\n", cp_len, __LINE__);
					break;
				}


				if (left < cp_len)
				{
					os_memcpy(channel->cbuf + channel->ccount, p, left);
					channel->ccount += left;
					left = 0;
					//LOGE("cp_len head size not enough: %d, ccount: %d\n", cp_len, channel->ccount);
					break;
				}
				else
				{
					os_memcpy(channel->cbuf + channel->ccount, p, cp_len);
					channel->ccount += cp_len;
					p += cp_len;
					left -= cp_len;
				}
			}

			ptr = (db_trans_head_t *)channel->cbuf;

			head.magic = CHECK_ENDIAN_UINT32(ptr->magic);

			if (head.magic == HEAD_MAGIC_CODE)
			{
				/*
				*   Magic code  2 bytes
				*   Flags       2 bytes
				*   Timestamp   4 bytes
				*   Squence     2 bytes
				*   Length      2 bytes
				*   CRC         1 byte
				*   RESERVED    3 byte
				*/

				head.flags = CHECK_ENDIAN_UINT16(ptr->flags);
				head.timestamp = CHECK_ENDIAN_UINT32(ptr->timestamp);
				head.sequence = CHECK_ENDIAN_UINT16(ptr->sequence);
				head.length = CHECK_ENDIAN_UINT16(ptr->length);
				head.crc = ptr->crc;
				head.reserved[0] = ptr->reserved[0];
				head.reserved[1] = ptr->reserved[1];
				head.reserved[2] = ptr->reserved[2];

#ifdef DEBUG_HEAD
				LOGI("head size: %d, %d, flags: %04X\n", HEAD_SIZE_TOTAL, sizeof(db_trans_head_t), head.flags);
				LOGI("time: %u, len: %u, seq: %u, crc: %02X\n",
					head.timestamp, head.length, head.sequence, head.crc);
#endif
			}
			else
			{
				LOGE("invaild cached data, %04X, %d\n", head.magic, __LINE__);
				doorbell_hex_dump(channel->cbuf, channel->ccount);
				//TODO FIXME
				break;
			}

			if (channel->ccount < HEAD_SIZE_TOTAL + head.length)
			{
				cp_len = head.length + HEAD_SIZE_TOTAL - channel->ccount;

				if (cp_len < 0)
				{
					LOGE("cp_len error: %d at %d\n", cp_len, __LINE__);
					break;
				}

				if (left < cp_len)
				{
					os_memcpy(channel->cbuf + channel->ccount, p, left);
					channel->ccount += left;
					left = 0;
					///LOGE("cp_len payload size not enough: %d, ccount: %d\n", cp_len, channel->ccount);
					break;
				}
				else
				{
					os_memcpy(channel->cbuf + channel->ccount, p, cp_len);
					left -= cp_len;
					p += cp_len;
					channel->ccount += cp_len;
				}

#ifdef DEBUG_CRC
				if (HEAD_FLAGS_CRC & head.flags)
				{

					uint8_t ret_crc = hnd_crc8(channel->cbuf + HEAD_SIZE_TOTAL, head.length, CRC8_INIT_VALUE);

					if (ret_crc != head.crc)
					{
						LOGI("check crc failed\n");
					}

					LOGI("CRC SRC: %02X,  CALC: %02X\n", head.crc, ret_crc);
				}
#endif

				if (cb)
				{
					cb(channel, head.sequence, head.flags, head.timestamp, head.sequence, ptr->payload, head.length);
				}

				//LOGI("cached: %d, left: %d\n", channel->ccount, left);

				channel->ccount = 0;
			}
			else
			{
				LOGE("invaild flow data\n");
				doorbell_hex_dump(channel->cbuf, channel->ccount);
				//SHOULD NOT BE HERE
				//TODO FIMXME
				break;
			}
		}
	}

	//LOGI("next cached: %d\n", channel->ccount);
}

uint32_t doorbell_transmission_get_milliseconds(void)
{
	uint32_t time = 0;

#if CONFIG_ARCH_RISCV
	extern u64 riscv_get_mtimer(void);

	time = (riscv_get_mtimer() / 26000) & 0xFFFFFFFF;
#elif CONFIG_ARCH_CM33

	time = (bk_aon_rtc_get_us() / 1000) & 0xFFFFFFFF;
#endif

	return time;
}

void doorbell_transmission_pack(db_channel_t *channel, uint8_t *data, uint32_t length)
{
	db_trans_head_t *head = channel->tbuf;


	/*
	*   Magic code  2 bytes
	*   Flags       2 bytes
	*   Timestamp   4 bytes
	*   Squence     2 bytes
	*   Length      2 bytes
	*   CRC         1 byte
	*   RESERVED    3 byte
	*/
	head->magic = CHECK_ENDIAN_UINT16(HEAD_MAGIC_CODE);
	head->flags = CHECK_ENDIAN_UINT16(HEAD_FLAGS_CRC);
	head->timestamp = CHECK_ENDIAN_UINT32(doorbell_transmission_get_milliseconds());
	head->sequence = CHECK_ENDIAN_UINT16(++channel->sequence);
	head->length = CHECK_ENDIAN_UINT16(length);
	head->crc = hnd_crc8(data, length, CRC8_INIT_VALUE);;
	head->reserved[0] = 0;
	head->reserved[1] = 0;
	head->reserved[2] = 0;

	os_memcpy(head->payload, data, length);

}

int doorbell_transmission_pack_send(db_channel_t *channel, uint8_t *data, uint32_t length, doorbell_transmission_send_t cb)
{
	bk_err_t ret = BK_FAIL;

	doorbell_transmission_pack(channel, data, length);

	if (channel->cb && channel->cb->tsend)
	{
		return channel->cb->tsend((uint8_t *)channel->tbuf, sizeof(db_trans_head_t) + length);
	}

	if (cb)
	{
		ret = cb((uint8_t *)channel->tbuf, sizeof(db_trans_head_t) + length);
	}

	return ret;
}

