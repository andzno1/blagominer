#include "stdafx.h"
#include "logger.h"
FILE * fh_Log = nullptr;
bool use_log = true;

std::mutex m;
std::list<std::string> loggingQueue;
std::thread writer;
bool interruptWriter = false;

void _writer()
{
	while (!interruptWriter || !loggingQueue.empty()) {
		if (!loggingQueue.empty()) {
			std::string str;
			{
				std::lock_guard<std::mutex> lockGuard(m);
				str = loggingQueue.front();
				loggingQueue.pop_front();
			}
			GetLocalTime(&cur_time);
			fprintf_s(fh_Log, "%02d:%02d:%02d %s\n", cur_time.wHour, cur_time.wMinute, cur_time.wSecond, str.c_str());
			fflush(fh_Log);
		}
	}
}

void Log_init(void)
{
	if (use_log)
	{
		std::stringstream ss;
		if (CreateDirectory(L"Logs", nullptr) == ERROR_PATH_NOT_FOUND)
		{
			bm_wattron(12);
			bm_wprintw("CreateDirectory failed (%d)\n", GetLastError(), 0);
			bm_wattroff(12);
			use_log = false;
			return;
		}
		GetLocalTime(&cur_time);
		ss << "Logs\\" << cur_time.wYear << "-" << cur_time.wMonth << "-" << cur_time.wDay << "_" << cur_time.wHour << "_" << cur_time.wMinute << "_" << cur_time.wSecond << ".log";
		std::string filename = ss.str();
		if ((fh_Log = _fsopen(filename.c_str(), "wt", _SH_DENYNO)) == NULL)
		{
			bm_wattron(12);
			bm_wprintw("LOG: file openinig error\n", 0);
			bm_wattroff(12);
			use_log = false;
		}
		else {
			writer = std::thread(_writer);
			Log(version);
			atexit(Log_end);
		}
	}
}

void Log_end(void)
{
	if (writer.joinable())
	{
		interruptWriter = true;
		writer.join();
	}
	if (fh_Log != nullptr)
	{
		fflush(fh_Log);
		fclose(fh_Log);
		fh_Log = nullptr;
	}
}

const char* Log_server(char const *const strLog)
{
	size_t len_str = strlen(strLog);
	if ((len_str> 0) && use_log)
	{
		char * Msg_log = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, len_str * 2 + 1);
		if (Msg_log == nullptr)	ShowMemErrorExit();

		for (size_t i = 0, j = 0; i<len_str; i++, j++)
		{
			if (strLog[i] == '\r')
			{
				Msg_log[j] = '\\';
				j++;
				Msg_log[j] = 'r';
			}
			else
				if (strLog[i] == '\n')
				{
					Msg_log[j] = '\\';
					j++;
					Msg_log[j] = 'n';
				}
				else
					if (strLog[i] == '%')
					{
						Msg_log[j] = '%';
						j++;
						Msg_log[j] = '%';
					}
					else Msg_log[j] = strLog[i];
		}
		return Msg_log;
	}
	return "";
}