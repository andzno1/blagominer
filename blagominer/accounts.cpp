#include "stdafx.h"
#include "accounts.h"
#include "dualmining.h"

size_t Get_index_acc(std::shared_ptr<t_mining_info> miningInfo, unsigned long long const key)
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
	bests.push_back({ key, ULLONG_MAX, 0, 0, miningInfo->targetDeadlineInfo });
	LeaveCriticalSection(&bestsLock);
	return bests.size() - 1;
}