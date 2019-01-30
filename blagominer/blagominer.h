#pragma once
#include "InstructionSet.h"
#include "bfs.h"
#include "network.h"
#include "shabal.h"
#include "common.h"
#include "filemonitor.h"
#include "updateChecker.h"

// miner
extern volatile int stopThreads;
extern char *pass;								// passphrase for solo mining
extern unsigned long long total_size;			// sum of all local plot file sizes
extern std::vector<std::string> paths_dir;      // plot paths

//miner config items
extern bool use_debug;							// output debug information if true

// PoC2
extern bool POC2;								// true if PoC2 is activated

#include "worker.h"

//headers
size_t GetFiles(const std::string &str, std::vector <t_files> *p_files);
