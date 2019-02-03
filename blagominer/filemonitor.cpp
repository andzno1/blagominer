#include "filemonitor.h"

std::mutex m;

std::map<std::string, t_file_stats> fileStats = std::map<std::string, t_file_stats>();
bool showCorruptedPlotFiles = true;
int oldLineCount = -1;
const std::string header = "File name                                             +DLs      -DLs       I/O";

void increaseMatchingDeadline(std::string file) {
	if (!showCorruptedPlotFiles) {
		return;
	}
	std::lock_guard<std::mutex> lockGuard(m);
	++fileStats[file].matchingDeadlines;
}

void increaseConflictingDeadline(std::string file) {
	if (!showCorruptedPlotFiles) {
		return;
	}
	std::lock_guard<std::mutex> lockGuard(m);
	++fileStats[file].conflictingDeadlines;
}

void increaseReadError(std::string file) {
	if (!showCorruptedPlotFiles) {
		return;
	}
	std::lock_guard<std::mutex> lockGuard(m);
	++fileStats[file].readErrors;
}

void resetFileStats() {
	if (!showCorruptedPlotFiles || !currentlyDisplayingCorruptedPlotFiles()) {
		return;
	}
	std::lock_guard<std::mutex> lockGuard(m);
	fileStats.clear();
}

void printFileStats() {
	if (!showCorruptedPlotFiles) {
		return;
	}
	std::lock_guard<std::mutex> lockGuard(m);
	int lineCount = 0;
	for (auto& element : fileStats) {
		if (element.second.conflictingDeadlines > 0 || element.second.readErrors > 0) {
			++lineCount;
		}
	}
	
	if (lineCount == 0 && currentlyDisplayingCorruptedPlotFiles()) {
		hideCorrupted();
		resizeCorrupted(0);
		refreshCorrupted();
		return;
	}
	else if (lineCount == 0) {
		return;
	}

	// Increase for header and for clear message.
	lineCount += 2;

	if (lineCount != oldLineCount) {
		clearCorrupted();
		resizeCorrupted(lineCount);
		oldLineCount = lineCount;
	}
	refreshCorrupted();

	lineCount = 1;
	bm_wmoveC(lineCount++, 1);
	bm_wprintwC("%s", header.c_str(), 0);
	
	for (auto& element : fileStats) {
		if (element.second.conflictingDeadlines > 0 || element.second.readErrors > 0) {
			bm_wattronC(14);
			bm_wmoveC(lineCount, 1);
			bm_wprintwC("%s %s", toStr(element.first, 46).c_str(), toStr(element.second.matchingDeadlines, 11).c_str(), 0);
			if (element.second.conflictingDeadlines > 0) {
				bm_wattronC(4);
			}
			bm_wprintwC(" %s", toStr(element.second.conflictingDeadlines, 9).c_str(), 0);
			bm_wattroffC(4);
			bm_wattronC(14);
			if (element.second.readErrors > 0) {
				bm_wattronC(4);
			}
			bm_wprintwC(" %s\n", toStr(element.second.readErrors, 9).c_str(), 0);
			bm_wattroffC(4);
			
			++lineCount;
		}
	}
	bm_wattroffC(14);
	
	int rowsCorrupted = getRowsCorrupted();
	if (rowsCorrupted < lineCount + 2) {
		bm_wmoveC(rowsCorrupted - 3, 1);
		clearCorruptedLine();
		bm_wprintwC("Not enough space to display all data.");
		bm_wmoveC(rowsCorrupted - 2, 1);
		clearCorruptedLine();
		bm_wprintwC("Press 'f' to clear data.");
	}
	else {
		
		bm_wmoveC(lineCount, 1);
		bm_wprintwC("Press 'f' to clear data.");
	}   
	boxCorrupted();
	refreshCorrupted();
}