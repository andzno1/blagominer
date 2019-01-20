#include "stdafx.h"
#include "inout.h"
#undef  MOUSE_MOVED
#include "curses.h" //include pdcurses

short win_size_x = 90;
short win_size_y = 60;
const short progress_lines = 3;
const short corrupted_lines = 2;
WINDOW * win_main;
WINDOW * win_progress;
WINDOW * win_corrupted;

//Turn on color attribute
int bm_wattron(int color) {
	return wattron(win_main, COLOR_PAIR(color));
}

//Turn off color attribute
int bm_wattroff(int color) {
	return wattroff(win_main, COLOR_PAIR(color));
}

//print
int bm_wprintw(const char * output, ...) {
	va_list args;
	va_start(args, output);
	return vw_printw(win_main, output, args);
	va_end(args);
}

//Turn on color attribute
int bm_wattronP(int color) {
	return wattron(win_progress, COLOR_PAIR(color));
}

//Turn off color attribute
int bm_wattroffP(int color) {
	return wattroff(win_progress, COLOR_PAIR(color));
}

//Turn on color attribute
int bm_wattronC(int color) {
	return wattron(win_corrupted, COLOR_PAIR(color));
}

//Turn off color attribute
int bm_wattroffC(int color) {
	return wattroff(win_corrupted, COLOR_PAIR(color));
}

//print
int bm_wprintwP(const char * output,...) {
	va_list args;
	va_start(args, output);
	return vw_printw(win_progress, output, args);
	va_end(args);
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
	win_corrupted = newwin(corrupted_lines, COLS, 0, 0);
	leaveok(win_corrupted, true);
}

void refreshMain(){
wrefresh(win_main);
}
void refreshProgress(){
wrefresh(win_progress);
}
void resizeWindows(int lineCount) {
	if (lineCount > -1) {
		wresize(win_main, LINES - 3 - corrupted_lines - lineCount, COLS);
		mvwin(win_main, corrupted_lines + lineCount, 0);
		wresize(win_corrupted, corrupted_lines + lineCount, COLS);
	}
}
void refreshCorrupted() {
	wrefresh(win_corrupted);
}
void clearProgress(){
wclear(win_progress);
}
void clearCorrupted() {
	wclear(win_corrupted);
}

int bm_wgetchMain() {
	return wgetch(win_main);
}
void boxProgress(){
box(win_progress, 0, 0);
}
void boxCorrupted() {
	wattron(win_corrupted, COLOR_PAIR(4));
	box(win_corrupted, 0, 0);
	wattroff(win_corrupted, COLOR_PAIR(4));
}
void bm_wmoveP() {
	wmove(win_progress, 1, 1);
};
int bm_wmoveC(int line, int column) {
	return wmove(win_corrupted, line, column);
};
