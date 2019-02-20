#pragma once

#include "common.h"
#include <mutex>
#include <list>
#include <ctime>
#include <time.h>

extern short win_size_x;
extern short win_size_y;

//void printToConsole(int foregroundColor, int backgroundColor, bool fillLine, const char * output, ...);

enum ConsoleWindow {
	VERSION,
	CORRUPTED,
	MAIN,
	PROGRESS
};

struct ConsoleOutput {
	ConsoleWindow window;
	int colorPair;
	bool leadingNewLine;
	bool fillLine;
	std::string message;
};


extern std::mutex mConsoleQueue;
extern std::mutex mConsoleWindow;
extern std::list<ConsoleOutput> consoleQueue;

template<typename ... Args>
void printToConsole(ConsoleWindow window, int colorPair, bool printTimestamp, bool leadingNewLine,
	bool fillLine, const char * format, Args ... args)
{
	std::string message;
	if (printTimestamp) {
		SYSTEMTIME cur_time;
		GetLocalTime(&cur_time);
		char timeBuff[9];
		snprintf(timeBuff, sizeof(timeBuff), "%02d:%02d:%02d", cur_time.wHour, cur_time.wMinute, cur_time.wSecond);
		message = timeBuff;
		message += " ";
	}

	size_t size = snprintf(nullptr, 0, format, args ...) + 1; // Extra space for '\0'
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format, args ...);
	message += std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
	{
		std::lock_guard<std::mutex> lockGuard(mConsoleQueue);
		consoleQueue.push_back({
			window,
			colorPair,
			leadingNewLine,
			fillLine,
			message });
	}
};


void bm_init();
void bm_end();

bool currentlyDisplayingCorruptedPlotFiles();
bool currentlyDisplayingNewVersion();

int bm_wgetchMain(); //get input vom main window

int bm_wattronC(int color);
int bm_wattroffC(int color);
int bm_wprintwC(const char * output, ...);

void refreshMain();
void refreshCorrupted();
void showNewVersion(std::string version);

void resizeCorrupted(int lineCount);
int getRowsCorrupted();

void clearCorrupted();
void clearCorruptedLine();
void clearNewVersion();

void hideCorrupted();

int bm_wmoveC(int line, int column);

void boxCorrupted();