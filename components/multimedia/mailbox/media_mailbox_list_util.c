// Copyright 2020-2021 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <os/os.h>
#include <os/str.h>
#include <os/mem.h>
#include <components/log.h>
#include <common/bk_include.h>
#include <driver/int.h>

#include "bk_list.h"
#include "media_mailbox_list_util.h"

#define TAG "media_mailbox_list_util"


#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

void media_mailbox_list_clear(LIST_HEADER_T *list, uint8_t flag)
{
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	LIST_HEADER_T *pos, *n;
	media_mailbox_msg_t *node = NULL;
	media_mailbox_list_t *tmp = NULL;
	if (!list_empty(list))
	{
		list_for_each_safe(pos, n, list)
		{
			tmp = list_entry(pos, media_mailbox_list_t, list);
			if (tmp != NULL)
			{
				node = tmp->msg;
				if (flag)
				{
					rtos_set_semaphore(&node->sem);
				}
				list_del(pos);
				os_free(tmp);
			}
		}
		INIT_LIST_HEAD(list);
	}
	GLOBAL_INT_RESTORE();
}

media_mailbox_msg_t *media_mailbox_list_get_node(beken_semaphore_t msg, LIST_HEADER_T *list)
{
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	LIST_HEADER_T *pos, *n;
	media_mailbox_msg_t *node = NULL;
	media_mailbox_list_t *tmp = NULL;
	list_for_each_safe(pos, n, list)
	{
		tmp = list_entry(pos, media_mailbox_list_t, list);
		if (tmp != NULL)
		{
			if(tmp->msg->sem == msg)
			{
				node = tmp->msg;
				break;
			}
		}
	}
	GLOBAL_INT_RESTORE();
	return node;
}

media_mailbox_msg_t *media_mailbox_list_del_node_by_event(uint32_t event, LIST_HEADER_T *list)
{
	GLOBAL_INT_DECLARATION();
	GLOBAL_INT_DISABLE();
	LIST_HEADER_T *pos, *n;
	media_mailbox_msg_t *node = NULL;
	media_mailbox_list_t *tmp = NULL;
	list_for_each_safe(pos, n, list)
	{
		tmp = list_entry(pos, media_mailbox_list_t, list);
		if (tmp != NULL)
		{
			if(tmp->msg->event == event)
			{
				node = tmp->msg;
				list_del(pos);
				os_free(tmp);
				break;
			}
		}
	}
	GLOBAL_INT_RESTORE();
	return node;
}

media_mailbox_msg_t *media_mailbox_list_del_node(beken_semaphore_t msg, LIST_HEADER_T *list)
{
	GLOBAL_INT_DECLARATION();
	GLOBAL_INT_DISABLE();
	LIST_HEADER_T *pos, *n;
	media_mailbox_msg_t *node = NULL;
	media_mailbox_list_t *tmp = NULL;
	list_for_each_safe(pos, n, list)
	{
		tmp = list_entry(pos, media_mailbox_list_t, list);
		if (tmp != NULL)
		{
			if(tmp->msg->sem == msg)
			{
				node = tmp->msg;
				list_del(pos);
				os_free(tmp);
				break;
			}
		}
	}
	GLOBAL_INT_RESTORE();
	return node;
}

media_mailbox_msg_t *media_mailbox_list_pop(LIST_HEADER_T *list)
{
	GLOBAL_INT_DECLARATION();
	GLOBAL_INT_DISABLE();
	LIST_HEADER_T *pos, *n;
	media_mailbox_msg_t *node = NULL;
	media_mailbox_list_t *tmp = NULL;
	list_for_each_safe(pos, n, list)
	{
		tmp = list_entry(pos, media_mailbox_list_t, list);
		if (tmp != NULL)
		{
			node = tmp->msg;
			list_del(pos);
			os_free(tmp);
			break;
		}
	}
	GLOBAL_INT_RESTORE();
	return node;
}

bk_err_t media_mailbox_list_push(media_mailbox_msg_t *tmp, LIST_HEADER_T *list)
{
	bk_err_t ret = BK_OK;
	media_mailbox_list_t *mailbox_list = os_malloc(sizeof(media_mailbox_list_t));
	if (mailbox_list == NULL)
	{
		LOGE("%s, malloc failed!\n", __func__);
		return BK_ERR_NO_MEM;
	}
	mailbox_list->msg = tmp;
	GLOBAL_INT_DECLARATION();
	GLOBAL_INT_DISABLE();
	list_add_tail(&mailbox_list->list, list);
	GLOBAL_INT_RESTORE();
	return ret;
}

