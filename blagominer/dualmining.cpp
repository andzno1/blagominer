#include "dualmining.h"

char *coinNames[] =
{
	(char *) "Burstcoin",
	(char *) "Bitcoin HD"
};

unsigned long long getHeight(std::shared_ptr<t_coin_info> coin) {
	std::lock_guard<std::mutex> lockGuard(coin->locks->mHeight);
	return coin->mining->height;
}
void setHeight(std::shared_ptr<t_coin_info> coin, const unsigned long long height) {
	std::lock_guard<std::mutex> lockGuard(coin->locks->mHeight);
	coin->mining->height = height;
}

unsigned long long getTargetDeadlineInfo(std::shared_ptr<t_coin_info> coin) {
	std::lock_guard<std::mutex> lockGuard(coin->locks->mTargetDeadlineInfo);
	return coin->mining->targetDeadlineInfo;
}
void setTargetDeadlineInfo(std::shared_ptr<t_coin_info> coin, const unsigned long long targetDeadlineInfo) {
	std::lock_guard<std::mutex> lockGuard(coin->locks->mTargetDeadlineInfo);
	coin->mining->targetDeadlineInfo = targetDeadlineInfo;
}

/**
	Don't forget to delete the pointer after using it.
**/
char* getSignature(std::shared_ptr<t_coin_info> coin) {
	char* sig = new char[33];
	RtlSecureZeroMemory(sig, 33);
	{
		std::lock_guard<std::mutex> lockGuard(coin->locks->mSignature);
		memmove(sig, coin->mining->signature, 32);
	}
	return sig;
}
void setSignature(std::shared_ptr<t_coin_info> coin, const char* signature) {
	std::lock_guard<std::mutex> lockGuard(coin->locks->mSignature);
	memmove(coin->mining->signature, signature, 32);
}

void updateOldSignature(std::shared_ptr<t_coin_info> coin) {
	std::lock_guard<std::mutex> lockGuard(coin->locks->mSignature);
	std::lock_guard<std::mutex> lockGuardO(coin->locks->mOldSignature);
	memmove(coin->mining->oldSignature, coin->mining->signature, 32);
}

bool signaturesDiffer(std::shared_ptr<t_coin_info> coin) {
	std::lock_guard<std::mutex> lockGuard(coin->locks->mSignature);
	std::lock_guard<std::mutex> lockGuardO(coin->locks->mOldSignature);
	return memcmp(coin->mining->signature, coin->mining->oldSignature, 32) != 0;
}

bool signaturesDiffer(std::shared_ptr<t_coin_info> coin, const char* sig) {
	std::lock_guard<std::mutex> lockGuard(coin->locks->mSignature);
	return memcmp(coin->mining->signature, sig, 32) != 0;
}