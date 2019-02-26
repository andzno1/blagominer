#include "loggerCsv.h"

const std::string csvFailBurst = "fail-" + std::string((char*)coinNames[BURST]) + ".csv";
const std::string csvFailBhd = "fail-" + std::string((char*)coinNames[BHD]) + ".csv";
const std::string csvSubmittedBurst = "stat-" + std::string((char*)coinNames[BURST]) + ".csv";
const std::string csvSubmittedBhd = "stat-" + std::string((char*)coinNames[BHD]) + ".csv";

std::mutex mCsvFailBurst;
std::mutex mCsvFailBhd;
std::mutex mCsvSubmittedBurst;
std::mutex mCsvSubmittedBhd;

bool existsFile(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

void Csv_Init()
{
	if (!loggingConfig.enableCsv) {
		return;
	}
	Log(L"Initializing csv logging.");
	const char* headersFail = "Timestamp epoch;Timestamp local;Height;File;baseTarget;Network difficulty;Nonce;Deadline sent;Deadline confirmed;Response\n";
	const char* headersSubmitted = "Timestamp epoch;Timestamp local;Height;baseTarget;Network difficulty;Round time;Completed round; Deadline\n";
	if ((burst->mining->enable || burst->network->enable_proxy) && !existsFile(csvFailBurst))
	{
		std::lock_guard<std::mutex> lockGuard(mCsvFailBurst);
		Log(L"Writing headers to %S", csvFailBurst.c_str());
		FILE * pFile;
		fopen_s(&pFile, csvFailBurst.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, headersFail);
			fclose(pFile);
		}
		else
		{
			Log(L"Failed to open %S", csvFailBurst.c_str());
		}
	}
	if ((burst->mining->enable || burst->network->enable_proxy) && !existsFile(csvSubmittedBurst))
	{
		std::lock_guard<std::mutex> lockGuard(mCsvSubmittedBurst);
		Log(L"Writing headers to %S", csvSubmittedBurst.c_str());
		FILE * pFile;
		fopen_s(&pFile, csvSubmittedBurst.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, headersSubmitted);
			fclose(pFile);
		}
		else
		{
			Log(L"Failed to open %S", csvSubmittedBurst.c_str());
		}
	}

	if ((bhd->mining->enable || burst->network->enable_proxy) && !existsFile(csvFailBhd))
	{
		std::lock_guard<std::mutex> lockGuard(mCsvFailBhd);
		Log(L"Writing headers to %S", csvFailBhd.c_str());
		FILE * pFile;
		fopen_s(&pFile, csvFailBhd.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, headersFail);
			fclose(pFile);
		}
		else
		{
			Log(L"Failed to open %S", csvFailBhd.c_str());
		}
	}
	if ((bhd->mining->enable || bhd->network->enable_proxy) && !existsFile(csvSubmittedBhd))
	{
		std::lock_guard<std::mutex> lockGuard(mCsvSubmittedBhd);
		Log(L"Writing headers to %S", csvSubmittedBhd.c_str());
		FILE * pFile;
		fopen_s(&pFile, csvSubmittedBhd.c_str(), "a+t");
		if (pFile != nullptr)
		{
			fprintf(pFile, headersSubmitted);
			fclose(pFile);
		}
		else
		{
			Log(L"Failed to open %S", csvSubmittedBhd.c_str());
		}
	}
}

void Csv_Fail(Coins coin, const unsigned long long height, const std::string& file, const unsigned long long baseTarget,
	const unsigned long long netDiff, const unsigned long long nonce, const unsigned long long deadlineSent,
	const unsigned long long deadlineConfirmed, const std::string& response)
{
	if (!loggingConfig.enableCsv) {
		return;
	}
	std::time_t rawtime = std::time(nullptr);
	char timeDate[20];
	getLocalDateTime(rawtime, timeDate);

	FILE * pFile;
	if (coin == BURST && (burst->mining->enable || burst->network->enable_proxy))
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
			Log(L"Failed to open %S", csvFailBurst.c_str());
			return;
		}
	}
	else if (coin == BHD && (bhd->mining->enable || bhd->network->enable_proxy))
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
			Log(L"Failed to open %S", csvFailBhd.c_str());
			return;
		}
	}

}

void Csv_Submitted(Coins coin, const unsigned long long height, const unsigned long long baseTarget, const unsigned long long netDiff,
	const double roundTime, const bool completedRound, const unsigned long long deadline)
{
	if (!loggingConfig.enableCsv) {
		return;
	}
	std::time_t rawtime = std::time(nullptr);
	char timeDate[20];
	getLocalDateTime(rawtime, timeDate);

	FILE * pFile;
	if (coin == BURST && (burst->mining->enable || burst->network->enable_proxy))
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
			Log(L"Failed to open %S", csvSubmittedBurst.c_str());
			return;
		}
	}
	else if (coin == BHD && (bhd->mining->enable || bhd->network->enable_proxy))
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
			Log(L"Failed to open %S", csvSubmittedBhd.c_str());
			return;
		}
	}

}