#pragma once
#include "error.h"
#include <mutex>
#include <list>
#include <ctime>
#include <time.h>


//logger variables
extern FILE * fp_Log;
extern std::mutex mLog;
extern std::list<std::string> loggingQueue;

extern bool loggingInitialized;

//logger functions
void Log_init(void);
void Log_end(void);
std::string Log_server(char const *const strLog);

template<typename ... Args>
void Log(const std::string& format, Args ... args)
{
	if (!loggingInitialized) {
		return;
	}
	SYSTEMTIME cur_time;
	GetLocalTime(&cur_time);
	char timeBuff[9];
	snprintf(timeBuff, sizeof(timeBuff), "%02d:%02d:%02d", cur_time.wHour, cur_time.wMinute, cur_time.wSecond);
	std::string time = timeBuff;

	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args ...);
	{
		std::lock_guard<std::mutex> lockGuard(mLog);
		loggingQueue.push_back(time + " " + std::string(buf.get(), buf.get() + size - 1)); // We don't want the '\0' inside
	}
};