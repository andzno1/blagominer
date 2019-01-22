#include "stdafx.h"
#include "logger.h"
FILE * fp_Log = nullptr;
const std::string csvFailBurst = "fail-" + std::string(coinNames[BURST]) + ".csv";
const std::string csvFailBhd = "fail-" + std::string(coinNames[BHD]) + ".csv";
const std::string csvSubmittedBurst = "stat-" + std::string(coinNames[BURST]) + ".csv";
const std::string csvSubmittedBhd = "stat-" + std::string(coinNames[BHD]) + ".csv";

bool use_log = true;

std::mutex mLog;
std::mutex mCsvFailBurst;
std::mutex mCsvFailBhd;
std::mutex mCsvSubmittedBurst;
std::mutex mCsvSubmittedBhd;
std::list<std::string> loggingQueue;
std::thread writer;
bool interruptWriter = false;

void _writer()
{
	while (!interruptWriter || !loggingQueue.empty()) {
		if (!loggingQueue.empty()) {
			std::string str;
			{
				std::lock_guard<std::mutex> lockGuard(mLog);
				str = loggingQueue.front();
				loggingQueue.pop_front();
			}
			SYSTEMTIME cur_time;
			GetLocalTime(&cur_time);
			fprintf_s(fp_Log, "%02d:%02d:%02d %s\n", cur_time.wHour, cur_time.wMinute, cur_time.wSecond, str.c_str());
			fflush(fp_Log);
		}
	}
}

bool existsFile(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

void Csv_Init()
{
	const char* headersFail = "Timestamp epoch;Timestamp local;Height;File;baseTarget;Network difficulty;Nonce;Deadline sent;Deadline confirmed;Response\n";
	const char* headersSubmitted = "Timestamp epoch;Timestamp local;Height;baseTarget;Network difficulty;Round time;Completed round; Deadline\n";
	if (burst->mining->enable && !existsFile(csvFailBurst))
	{
		std::lock_guard<std::mutex> lockGuard(mCsvFailBurst);
		Log("Writing headers to %s", csvFailBurst.c_str());
		FILE * pFile;
		fopen_s(&pFile, csvFailBurst.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, headersFail);
			fclose(pFile);
		}
		else
		{
			Log("Failed to open %s", csvFailBurst.c_str());
		}
	}
	if (burst->mining->enable && !existsFile(csvSubmittedBurst))
	{
		std::lock_guard<std::mutex> lockGuard(mCsvSubmittedBurst);
		Log("Writing headers to %s", csvSubmittedBurst.c_str());
		FILE * pFile;
		fopen_s(&pFile, csvSubmittedBurst.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, headersSubmitted);
			fclose(pFile);
		}
		else
		{
			Log("Failed to open %s", csvSubmittedBurst.c_str());
		}
	}

	if (bhd->mining->enable && !existsFile(csvFailBhd))
	{
		std::lock_guard<std::mutex> lockGuard(mCsvFailBhd);
		Log("Writing headers to %s", csvFailBhd.c_str());
		FILE * pFile;
		fopen_s(&pFile, csvFailBhd.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, headersFail);
			fclose(pFile);
		}
		else
		{
			Log("Failed to open %s", csvFailBhd.c_str());
		}
	}
	if (bhd->mining->enable && !existsFile(csvSubmittedBhd))
	{
		std::lock_guard<std::mutex> lockGuard(mCsvSubmittedBhd);
		Log("Writing headers to %s", csvSubmittedBhd.c_str());
		FILE * pFile;
		fopen_s(&pFile, csvSubmittedBhd.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, headersSubmitted);
			fclose(pFile);
		}
		else
		{
			Log("Failed to open %s", csvSubmittedBhd.c_str());
		}
	}
}

void Csv_Fail(Coins coin, const unsigned long long height, const std::string& file, const unsigned long long baseTarget,
	const unsigned long long netDiff, const unsigned long long nonce, const unsigned long long deadlineSent,
	const unsigned long long deadlineConfirmed, const std::string& response)
{
	std::time_t rawtime = std::time(nullptr);
	char timeDate[20];
	getLocalDateTime(rawtime, timeDate);

	FILE * pFile;
	if (coin == BURST && burst->mining->enable)
	{	
		std::lock_guard<std::mutex> lockGuard(mCsvFailBurst);
		fopen_s(&pFile, csvFailBurst.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, "%llu;%s;%llu;%s;%llu;%llu;%llu;%llu;%llu;%s\n", (unsigned long long)rawtime, timeDate, height, file.c_str(), baseTarget, netDiff,
				nonce, deadlineSent, deadlineConfirmed, response.c_str());
			fclose(pFile);
			return;
		}
		else
		{
			Log("Failed to open %s", csvFailBurst.c_str());
			return;
		}
	}
	else if (coin == BHD && bhd->mining->enable)
	{
		std::lock_guard<std::mutex> lockGuard(mCsvFailBhd);
		fopen_s(&pFile, csvFailBhd.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, "%llu;%s;%llu;%s;%llu;%llu;%llu;%llu;%llu;%s\n", (unsigned long long)rawtime, timeDate, height, file.c_str(), baseTarget, netDiff,
				nonce, deadlineSent, deadlineConfirmed, response.c_str());
			fclose(pFile);
			return;
		}
		else
		{
			Log("Failed to open %s", csvFailBhd.c_str());
			return;
		}
	}
	
}

void Csv_Submitted(Coins coin, const unsigned long long height, const unsigned long long baseTarget, const unsigned long long netDiff,
	const double roundTime, const bool completedRound, const unsigned long long deadline)
{
	std::time_t rawtime = std::time(nullptr);
	char timeDate[20];
	getLocalDateTime(rawtime, timeDate);

	FILE * pFile;
	if (coin == BURST && burst->mining->enable)
	{
		std::lock_guard<std::mutex> lockGuard(mCsvSubmittedBurst);
		fopen_s(&pFile, csvSubmittedBurst.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, "%llu;%s;%llu;%llu;%llu;%.1f;%s;%llu\n", (unsigned long long)rawtime, timeDate, height, baseTarget,
				netDiff, roundTime, completedRound ? "true" : "false", deadline);
			fclose(pFile);
			return;
		}
		else
		{
			Log("Failed to open %s", csvSubmittedBurst.c_str());
			return;
		}
	}
	else if (coin == BHD && bhd->mining->enable)
	{
		std::lock_guard<std::mutex> lockGuard(mCsvSubmittedBhd);
		fopen_s(&pFile, csvSubmittedBhd.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, "%llu;%s;%llu;%llu;%llu;%.1f;%s;%llu\n", (unsigned long long)rawtime, timeDate, height, baseTarget,
				netDiff, roundTime, completedRound ? "true" : "false", deadline);
			fclose(pFile);
			return;
		}
		else
		{
			Log("Failed to open %s", csvSubmittedBhd.c_str());
			return;
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
		SYSTEMTIME cur_time;
		GetLocalTime(&cur_time);
		GetLocalTime(&cur_time);
		ss << "Logs\\" << cur_time.wYear << "-" << cur_time.wMonth << "-" << cur_time.wDay << "_" << cur_time.wHour << "_" << cur_time.wMinute << "_" << cur_time.wSecond << ".log";
		std::string filename = ss.str();
		if ((fp_Log = _fsopen(filename.c_str(), "wt", _SH_DENYNO)) == NULL)
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
	if (fp_Log != nullptr)
	{
		fflush(fp_Log);
		fclose(fp_Log);
		fp_Log = nullptr;
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