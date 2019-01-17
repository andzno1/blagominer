#pragma once
#include "blagominer.h"
#include "inout.h"
#include "error.h"
#include <mutex>
#include <list>


//logger variables
extern bool use_log;
extern FILE * fh_Log;
extern std::mutex m;
extern std::list<std::string> loggingQueue;

//logger functions
void Log_init(void);
void Log_end(void);
const char* Log_server(char const *const strLog);

template<typename ... Args>
void Log(const std::string& format, Args ... args)
{
	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args ...);
	{
		std::lock_guard<std::mutex> lockGuard(m);
		loggingQueue.push_back(std::string(buf.get(), buf.get() + size - 1)); // We don't want the '\0' inside
	}
};