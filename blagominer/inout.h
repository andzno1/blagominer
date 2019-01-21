#pragma once
extern short win_size_x;
extern short win_size_y;

void bm_init();
// main window functions
int bm_wattron(int color);
int bm_wattroff(int color);
int bm_wprintw(const char * output,...);
int bm_wprintwFill(const char * output, ...);
void refreshMain();
int bm_wgetchMain(); //get input vom main window

// progress window function
int bm_wattronP(int color);
int bm_wattroffP(int color);
int bm_wprintwP(const char * output,...);
int bm_wattronC(int color);
int bm_wattroffC(int color);
int bm_wprintwC(const char * output, ...);
void refreshProgress();
void resizeWindows(int lineCount);
void refreshCorrupted();
void clearProgress();
void clearCorrupted();
void bm_wmoveP();
int bm_wmoveC(int line, int column);
void boxProgress();
void boxCorrupted();
