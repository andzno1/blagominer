#pragma once
#include "blagominer.h"
#include "inout.h"
#include "error.h"
#include "common.h"
#include <mutex>
#include <list>
#include <ctime>


//logger variables
extern bool use_log;
extern FILE * fp_Log;
extern std::mutex mLog;
extern std::list<std::string> loggingQueue;

//logger functions
void Log_init(void);
void Log_end(void);
const char* Log_server(char const *const strLog);

// CSV functions
void Csv_Init();
void Csv_Fail(Coins coin, const unsigned long long height, const std::string& file, const unsigned long long baseTarget,
	const unsigned long long nonce, const unsigned long long deadlineSent, const unsigned long long deadlineConfirmed,
	const std::string& response);
void Csv_Submitted(Coins coin, const unsigned long long height, const unsigned long long baseTarget,
	const double roundTime, const bool completedRound, const unsigned long long deadline);

template<typename ... Args>
void Log(const std::string& format, Args ... args)
{
	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args ...);
	{
		std::lock_guard<std::mutex> lockGuard(mLog);
		loggingQueue.push_back(std::string(buf.get(), buf.get() + size - 1)); // We don't want the '\0' inside
	}
};