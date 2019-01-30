#include "stdafx.h"
#include "accounts.h"
#include "common.h"

size_t Get_index_acc(unsigned long long const key, std::shared_ptr<t_coin_info> coin, unsigned long long const targetDeadlineInfo)
{
	EnterCriticalSection(&coin->locks->bestsLock);
	size_t acc_index = 0;
	for (auto it = coin->mining->bests.begin(); it != coin->mining->bests.end(); ++it)
	{
		if (it->account_id == key)
		{
			LeaveCriticalSection(&coin->locks->bestsLock);
			return acc_index;
		}
		acc_index++;
	}
	coin->mining->bests.push_back({ key, ULLONG_MAX, 0, 0, targetDeadlineInfo });
	LeaveCriticalSection(&coin->locks->bestsLock);
	return coin->mining->bests.size() - 1;
}