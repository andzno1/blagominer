#pragma once

#include "stdafx.h"
#include <mutex>
#include <vector>
#include <ctime>

// blago version
#ifdef __AVX512F__
	char const *const version = "v1.180121_AVX512";
#else
#ifdef __AVX2__
	char const *const version = "v1.180121_AVX2";
#else
	#ifdef __AVX__
		char const *const version = "v1.180121_AVX";
	#else
		char const *const version = "v1.180121_SSE";
		//	char const *const version = "v1.180121";
	#endif
#endif 
#endif 

extern HANDLE hHeap;							//heap

enum Coins {
	BURST,
	BHD
};

extern char *coinNames[];

struct t_files {
	bool done;
	std::string Path;
	std::string Name;
	unsigned long long Size;
	unsigned long long Key;
	unsigned long long StartNonce;
	unsigned long long Nonces;
	unsigned long long Stagger;
	unsigned long long Offset;
	bool P2;
	bool BFS;
};

struct t_directory_info {
	t_directory_info(std::string dir, bool done, std::vector<t_files> files) : dir(dir), done(done), files(files) {}
	std::string dir;
	bool done;
	std::vector<t_files> files;
};

struct t_logging {
	bool logAllGetMiningInfos;				// Prevent spamming the log file by only outputting
											// GMI when there is a change in the GMI
};

struct t_locks {
	std::mutex mHeight;
	std::mutex mTargetDeadlineInfo;
	std::mutex mSignature;
	std::mutex mOldSignature;
	std::mutex mNewMiningInfoReceived;
};

struct t_mining_info {
	bool enable;
	bool newMiningInfoReceived;
	size_t miner_mode;						// miner mode. 0=solo, 1=pool
	size_t priority;
	bool interrupted;						// Flag for interrupted block when dual mining.
	unsigned long long baseTarget;			// base target of current block
	unsigned long long targetDeadlineInfo;	// target deadline info from pool
	unsigned long long height;				// current block height
	unsigned long long deadline;			// current deadline
	unsigned long long my_target_deadline;
	unsigned long long POC2StartBlock;
	unsigned int scoop;						// currenty scoop
	bool show_winner;
	char signature[33];						// signature of current block
	char oldSignature[33];					// signature of last block
	std::vector<std::shared_ptr<t_directory_info>> dirs;
};

struct t_network_info {
	std::string nodeaddr;
	std::string nodeport;
	std::string updateraddr;
	std::string updaterport;
	std::string infoaddr;
	std::string infoport;
	bool enable_proxy;						// enable client/server functionality
	std::string proxyport;
	size_t send_interval;
	size_t update_interval;
	int network_quality;
};

struct t_coin_info {
	Coins coin;
	std::shared_ptr<t_mining_info> mining;
	std::shared_ptr<t_network_info> network;
	std::shared_ptr<t_locks> locks;
};


extern std::shared_ptr<t_coin_info> burst;
extern std::shared_ptr<t_coin_info> bhd;
extern t_logging loggingConfig;

unsigned long long getHeight(std::shared_ptr<t_coin_info> coin);
void setHeight(std::shared_ptr<t_coin_info> coin, const unsigned long long height);

unsigned long long getTargetDeadlineInfo(std::shared_ptr<t_coin_info> coin);
void setTargetDeadlineInfo(std::shared_ptr<t_coin_info> coin, const unsigned long long targetDeadlineInfo);
char* getSignature(std::shared_ptr<t_coin_info> coin);
void setSignature(std::shared_ptr<t_coin_info> coin, const char* signature);
void updateOldSignature(std::shared_ptr<t_coin_info> coin);
bool signaturesDiffer(std::shared_ptr<t_coin_info> coin);
bool signaturesDiffer(std::shared_ptr<t_coin_info> coin, const char* sig);
bool haveReceivedNewMiningInfo(const std::vector<std::shared_ptr<t_coin_info>>& coins);
void setnewMiningInfoReceived(std::shared_ptr<t_coin_info> coin, const bool val);

void getLocalDateTime(const std::time_t &rawtime, char* local, const std::string sepTime = ":");