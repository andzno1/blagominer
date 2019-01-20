#include "stdafx.h"
#include "accounts.h"
#include "common.h"

size_t Get_index_acc(unsigned long long const key, unsigned long long const targetDeadlineInfo)
{
	EnterCriticalSection(&bestsLock);
	size_t acc_index = 0;
	for (auto it = bests.begin(); it != bests.end(); ++it)
	{
		if (it->account_id == key)
		{
			LeaveCriticalSection(&bestsLock);
			return acc_index;
		}
		acc_index++;
	}
	bests.push_back({ key, ULLONG_MAX, 0, 0, targetDeadlineInfo });
	LeaveCriticalSection(&bestsLock);
	return bests.size() - 1;
}