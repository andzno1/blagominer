#pragma once
#include "error.h"
#include <mutex>
#include <list>
#include <ctime>
#include <time.h>


//logger variables
extern std::mutex mLog;
extern std::list<std::wstring> loggingQueue;

extern bool loggingInitialized;

//logger functions
void Log_init(void);
void Log_end(void);
std::string Log_server(char const *const strLog);

template<typename ... Args>
void Log(const wchar_t * format, Args ... args)
{
	if (!loggingInitialized) {
		return;
	}
	SYSTEMTIME cur_time;
	GetLocalTime(&cur_time);
	wchar_t timeBuff[9];
	swprintf(timeBuff, sizeof(timeBuff), L"%02d:%02d:%02d", cur_time.wHour, cur_time.wMinute, cur_time.wSecond);
	std::wstring time = timeBuff;

	int size = swprintf(nullptr, 0, format, args ...) + 1;
	std::unique_ptr<wchar_t[]> buf(new wchar_t[size]);
	swprintf(buf.get(), size, format, args ...);
	{
		std::lock_guard<std::mutex> lockGuard(mLog);
		loggingQueue.push_back(time + L" " + std::wstring(buf.get(), buf.get() + size - 1));
	}
};