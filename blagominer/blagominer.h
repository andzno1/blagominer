#pragma once
#include "InstructionSet.h"
#include "bfs.h"
#include "network.h"
#include "shabal.h"
#include "dualmining.h"

// blago version
#ifdef __AVX512F__
	char const *const version = "v1.170997.2_AVX512";
#else
#ifdef __AVX2__
	char const *const version = "v1.170997.2_AVX2";
#else
	#ifdef __AVX__
		char const *const version = "v1.170997.2_AVX";
	#else
			char const *const version = "v1.170997.2_SSE";
		//	char const *const version = "v1.170997.2";
	#endif
#endif 
#endif 

extern HANDLE hHeap;							//heap

// locks
extern CRITICAL_SECTION sessionsLock;			// session lock
extern CRITICAL_SECTION bestsLock;				// best lock
extern CRITICAL_SECTION sharesLock;				// shares lock

// global variables

// miner
extern bool exit_flag;							// true if miner is to be exited
extern volatile int stopThreads;
extern char *pass;								// passphrase for solo mining
extern unsigned long long total_size;			// sum of all local plot file sizes
extern std::vector<std::string> paths_dir;      // plot paths

//miner config items
extern bool use_debug;							// output debug information if true

// PoC2
extern bool POC2;								// true if PoC2 is activated

// structures

struct t_shares {
	std::string file_name;
	unsigned long long account_id;// = 0;
	unsigned long long best;// = 0;
	unsigned long long nonce;// = 0;
};

extern std::vector<t_shares> shares;

struct t_best {
	unsigned long long account_id;// = 0;
	unsigned long long best;// = 0;
	unsigned long long nonce;// = 0;
	unsigned long long DL;// = 0;
	unsigned long long targetDeadline;// = 0;
};

extern std::vector<t_best> bests;

struct t_session {
	SOCKET Socket;
	unsigned long long deadline;
	t_shares body;
};

extern std::vector<t_session> sessions;

struct t_files {
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
#include "worker.h"

//headers
size_t GetFiles(const std::string &str, std::vector <t_files> *p_files);
