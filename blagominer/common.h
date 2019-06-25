#pragma once

#include "stdafx.h"
#include <mutex>
#include <vector>
#include <ctime>
#include <math.h>
#include "logger.h"

#include <curl/curl.h>

// blago version
extern const unsigned int versionMajor;
extern const unsigned int versionMinor;
extern const unsigned int versionRevision;
extern std::wstring version;

extern const wchar_t sepChar;

extern bool lockWindowSize;
extern double checkForUpdateInterval;
extern bool ignoreSuspectedFastBlocks;
extern volatile bool exit_flag;							// true if miner is to be exited

extern HANDLE hHeap;							//heap

enum Coins {
	BURST,
	BHD
};

enum MiningState {
	QUEUED,
	MINING,
	DONE,
	INTERRUPTED
};

extern wchar_t *coinNames[];

struct t_shares {
	t_shares(std::string file_name, unsigned long long account_id, unsigned long long best,
		unsigned long long nonce, unsigned long long deadline, unsigned long long height,
		unsigned long long baseTarget) :
		file_name(file_name), account_id(account_id), best(best), nonce(nonce), deadline(deadline),
		height(height), baseTarget(baseTarget) {}
	std::string file_name;
	unsigned long long account_id;// = 0;
	// This is the best Target, not Deadline.
	unsigned long long best;// = 0;
	unsigned long long nonce;// = 0;
	unsigned long long deadline;
	unsigned long long height;
	unsigned long long baseTarget;
};

struct t_best {
	unsigned long long account_id;// = 0;
	unsigned long long best;// = 0;
	unsigned long long nonce;// = 0;
	unsigned long long DL;// = 0;
	unsigned long long targetDeadline;// = 0;
};

struct t_session {
	t_session(SOCKET Socket, unsigned long long deadline, t_shares body) : Socket(Socket), deadline(deadline), body(body) {}
	SOCKET Socket;
	unsigned long long deadline;
	t_shares body;
};
struct t_session2 {
	t_session2(CURL* curl, unsigned long long deadline, t_shares body) : curl(curl), deadline(deadline), body(body) {}
	CURL* curl;
	unsigned long long deadline;
	t_shares body;
};

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
	bool enableLogging;
	bool enableCsv;
	bool logAllGetMiningInfos;				// Prevent spamming the log file by only outputting
											// GMI when there is a change in the GMI
};

struct t_locks {
	std::mutex mHeight;
	std::mutex mTargetDeadlineInfo;
	std::mutex mSignature;
	std::mutex mStrSignature;
	std::mutex mOldSignature;
	std::mutex mCurrentStrSignature;
	std::mutex mNewMiningInfoReceived;

	CRITICAL_SECTION sessionsLock;			// session lock
	CRITICAL_SECTION sessions2Lock;			// session2 lock
	CRITICAL_SECTION bestsLock;				// best lock
	CRITICAL_SECTION sharesLock;			// shares lock
};

struct t_mining_info {
	bool enable;
	bool newMiningInfoReceived;
	size_t miner_mode;						// miner mode. 0=solo, 1=pool
	size_t priority;
	MiningState state;
	unsigned long long baseTarget;			// base target of current block
	unsigned long long targetDeadlineInfo;	// target deadline info from pool
	unsigned long long height;				// current block height
	unsigned long long deadline;			// current deadline
	unsigned long long my_target_deadline;
	unsigned long long POC2StartBlock;
	unsigned int scoop;						// currenty scoop
	std::vector<std::shared_ptr<t_directory_info>> dirs;

	// Values for current mining process
	char currentSignature[33];
	char current_str_signature[65];
	unsigned long long currentHeight = 0;
	unsigned long long currentBaseTarget = 0;
	std::vector<t_best> bests;
	std::vector<std::shared_ptr<t_shares>> shares;

	// Values for new mining info check
	char signature[33];						// signature of current block
	char oldSignature[33];					// signature of last block
	char str_signature[65];
};

struct t_network_info {
	std::string nodeaddr;
	std::string nodeport;
	unsigned submitTimeout;
	std::string updateraddr;
	std::string updaterport;
	bool enable_proxy;						// enable client/server functionality
	std::string proxyport;
	size_t send_interval;
	size_t update_interval;
	size_t proxy_update_interval;
	int network_quality;
	bool usehttps;
	std::string sendextraquery;
	std::string sendextraheader;
	std::vector<std::shared_ptr<t_session>> sessions;
	std::vector<std::shared_ptr<t_session2>> sessions2;
	std::thread sender;
	std::thread confirmer;
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
char* getCurrentStrSignature(std::shared_ptr<t_coin_info> coin);
void setSignature(std::shared_ptr<t_coin_info> coin, const char* signature);
void setStrSignature(std::shared_ptr<t_coin_info> coin, const char* signature);
void updateOldSignature(std::shared_ptr<t_coin_info> coin);
void updateCurrentStrSignature(std::shared_ptr<t_coin_info> coin);
bool signaturesDiffer(std::shared_ptr<t_coin_info> coin);
bool signaturesDiffer(std::shared_ptr<t_coin_info> coin, const char* sig);
bool haveReceivedNewMiningInfo(const std::vector<std::shared_ptr<t_coin_info>>& coins);
void setnewMiningInfoReceived(std::shared_ptr<t_coin_info> coin, const bool val);
int getNetworkQuality(std::shared_ptr<t_coin_info> coin);

void getLocalDateTime(const std::time_t &rawtime, char* local, const std::string sepTime = ":");

std::wstring toWStr(int number, const unsigned short length);
std::wstring toWStr(unsigned long long number, const unsigned short length);
std::wstring toWStr(std::wstring str, const unsigned short length);
std::wstring toWStr(std::string str, const unsigned short length);

std::string toStr(unsigned long long number, const unsigned short length);
std::string toStr(std::string str, const unsigned short length);