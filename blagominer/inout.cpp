﻿#include "stdafx.h"
#include "inout.h"
#undef  MOUSE_MOVED
#include "curses.h" //include pdcurses

short win_size_x = 96;
short win_size_y = 60;
int minimumWinMainHeight = 5;
const short progress_lines = 3;
const short corrupted_lines = 2;
const short new_version_lines = 3;
WINDOW * win_main;
WINDOW * win_progress;
WINDOW * win_corrupted;
WINDOW * win_new_version;

std::mutex mConsoleQueue;
std::mutex mConsoleWindow;
std::list<ConsoleOutput> consoleQueue;
std::thread consoleWriter;
bool interruptConsoleWriter = false;

void _consoleWriter()
{
	while (!interruptConsoleWriter) {
		if (!consoleQueue.empty()) {
			ConsoleOutput consoleOutput;
			{
				std::lock_guard<std::mutex> lockGuard(mConsoleQueue);
				consoleOutput = consoleQueue.front();
				consoleQueue.pop_front();
			}
			
			{
				std::lock_guard<std::mutex> lockGuard(mConsoleWindow);
				WINDOW* win;
				switch (consoleOutput.window)
				{
				case VERSION:
					win = win_new_version;
					break;
				case MAIN:
					win = win_main;
					break;
				case PROGRESS:
					win = win_progress;
					wclear(win);
					wmove(win, 1, 1);
					box(win_progress, 0, 0);
					break;
				default:
					Log("Uncovered window case.");
					continue;
					break;
				}
			
				if (consoleOutput.colorPair >= 0) {
					wattron(win, COLOR_PAIR(consoleOutput.colorPair));
				}
				if (consoleOutput.leadingNewLine) {
					waddstr(win, "\n");
				}
				waddstr(win, consoleOutput.message.c_str());
				if (consoleOutput.fillLine) {
					int y;
					int x;
					getyx(win, y, x);
					const int remaining = COLS - x;

					if (remaining > 0) {
						waddstr(win, std::string(remaining, ' ').c_str());
						int newY;
						int newX;
						getyx(win, newY, newX);
						if (newX != 0) {
							waddstr(win, std::string("\n").c_str());
						}
					}
				}
				if (consoleOutput.colorPair >= 0) {
					wattroff(win, COLOR_PAIR(consoleOutput.colorPair));
				}
				wrefresh(win);
			}
			
		}
		else {
			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
}

bool currentlyDisplayingCorruptedPlotFiles() {
	return getbegy(win_corrupted) >= 0;
}

bool currentlyDisplayingNewVersion() {
	return getbegy(win_new_version) >= 0;
}

int bm_wgetchMain() {
	return wgetch(win_main);
}

//Turn on color attribute
int bm_wattronC(int color) {
	return wattron(win_corrupted, COLOR_PAIR(color));
}

//Turn off color attribute
int bm_wattroffC(int color) {
	return wattroff(win_corrupted, COLOR_PAIR(color));
}

int bm_wprintwC(const char * output, ...) {
	va_list args;
	va_start(args, output);
	return vw_printw(win_corrupted, output, args);
	va_end(args);
}

// init screen
void bm_init() {
	initscr();
	raw();
	cbreak();		// не использовать буфер для getch()
	noecho();		// не отображать нажатия клавиш
	curs_set(0);	// убрать курсор
	start_color();	// будет всё цветное 			

	//int init_pair(short pair, short foreground, short background);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(4, COLOR_RED, COLOR_BLACK);
	init_pair(5, COLOR_BLACK, COLOR_WHITE);
	init_pair(6, COLOR_CYAN, COLOR_BLACK);
	init_pair(7, COLOR_WHITE, COLOR_BLACK);
	init_pair(8, COLOR_BLACK, COLOR_YELLOW);
	init_pair(9, 9, COLOR_BLACK);
	init_pair(10, 10, COLOR_BLACK);
	init_pair(11, 11, COLOR_BLACK);
	init_pair(12, 12, COLOR_BLACK);
	init_pair(14, 14, COLOR_BLACK);
	init_pair(15, 15, COLOR_BLACK);
	init_pair(25, 15, COLOR_BLUE);

	win_main = newwin(LINES - 2, COLS, 0, 0);
	scrollok(win_main, true);
	keypad(win_main, true);
	nodelay(win_main, true);
	win_progress = newwin(progress_lines, COLS, LINES - progress_lines, 0);
	leaveok(win_progress, true);
	win_corrupted = newwin(corrupted_lines, COLS, -1, 0);
	leaveok(win_corrupted, true);
	win_new_version = newwin(new_version_lines, COLS, -1, 0);
	leaveok(win_corrupted, true);

	consoleWriter = std::thread(_consoleWriter);
}

void bm_end() {
	interruptConsoleWriter = true;
	if (consoleWriter.joinable())
	{
		consoleWriter.join();
	}
}

void refreshMain(){
	wrefresh(win_main);
}

void refreshCorrupted() {
	if (currentlyDisplayingCorruptedPlotFiles()) {
		wrefresh(win_corrupted);
	}
}

void showNewVersion(std::string version) {
	std::lock_guard<std::mutex> lockGuardConsoleWindow(mConsoleWindow);
	version = "New version available: " + version;
	if (!currentlyDisplayingNewVersion()) {
		mvwin(win_new_version, 0, 0);
		int winMainY = getbegy(win_main);
		int winMainRow = getmaxy(win_main);

		int winMainOffset = winMainY + new_version_lines;
		winMainRow -= new_version_lines;

		if (currentlyDisplayingCorruptedPlotFiles()) {
			int totalSpaceNeeded = new_version_lines + corrupted_lines + getRowsCorrupted() + progress_lines + minimumWinMainHeight + 3;
			if (totalSpaceNeeded > LINES) {
				Log("Terminal too small to output everything (%i).", totalSpaceNeeded);
				int corruptedLineCount = LINES - new_version_lines - corrupted_lines - progress_lines - minimumWinMainHeight - 3 + 2;
				Log("Setting corrupted linecount to %i", corruptedLineCount);
				resizeCorrupted(corruptedLineCount);
			}
			else {
				wresize(win_main, winMainRow, COLS);
				mvwin(win_main, winMainOffset, 0);
			}
			mvwin(win_corrupted, new_version_lines, 0);
		}
		else {
			wresize(win_main, winMainRow, COLS);
			mvwin(win_main, winMainOffset, 0);
		}
	}

	clearNewVersion();
	wattron(win_new_version, COLOR_PAIR(4));
	box(win_new_version, 0, 0);
	wattroff(win_new_version, COLOR_PAIR(4));
	wmove(win_new_version, 1, 1);
	wattron(win_new_version, COLOR_PAIR(14));
	waddstr(win_new_version, version.c_str());
	wattroff(win_new_version, COLOR_PAIR(14));

	wrefresh(win_new_version);
	refreshCorrupted();
	refreshMain();
}

void resizeCorrupted(int lineCount) {
	
	int extraSpace = progress_lines - 1;

	int winVerRow = 0;
	if (currentlyDisplayingNewVersion()) {
		winVerRow = getmaxy(win_new_version);
		if (lineCount == 0) {
			extraSpace++;
		}
	}
		
	if (lineCount > 0) {
		extraSpace++;
		if (!currentlyDisplayingCorruptedPlotFiles()) {
			mvwin(win_corrupted, winVerRow, 0);
		}
		
		int totalSpaceNeeded = lineCount + corrupted_lines + winVerRow + extraSpace + minimumWinMainHeight;
		if (totalSpaceNeeded > LINES) {
			Log("Terminal too small to output everything (lineCount: %i).", lineCount);
			lineCount = LINES - winVerRow - extraSpace - corrupted_lines - minimumWinMainHeight;
			Log("Setting linecount to %i", lineCount);
		}
		
		wresize(win_main, LINES - extraSpace - corrupted_lines - lineCount - winVerRow, COLS);
		mvwin(win_main, corrupted_lines + lineCount + winVerRow, 0);
				

		wresize(win_corrupted, corrupted_lines + lineCount, COLS);
	}
	else if (lineCount == 0) {
		mvwin(win_main, winVerRow, 0);
		wresize(win_main, LINES - extraSpace - winVerRow, COLS);
	}
}

int getRowsCorrupted() {
	return getmaxy(win_corrupted);
}

void clearProgress(){
	wclear(win_progress);
}
void clearCorrupted() {
	if (currentlyDisplayingCorruptedPlotFiles()) {
		mvwin(win_corrupted, -1, 0);
		wclear(win_corrupted);
	}
}
void clearCorruptedLine() {
	wclrtoeol(win_corrupted);
}
void clearNewVersion() {
	wclear(win_new_version);
}

void hideCorrupted() {
	if (currentlyDisplayingCorruptedPlotFiles()) {
		win_corrupted = newwin(corrupted_lines, COLS, -1, 0);
		leaveok(win_corrupted, true);
	}
}

int bm_wmoveC(int line, int column) {
	return wmove(win_corrupted, line, column);
};

void boxCorrupted() {
	wattron(win_corrupted, COLOR_PAIR(4));
	box(win_corrupted, 0, 0);
	wattroff(win_corrupted, COLOR_PAIR(4));
}