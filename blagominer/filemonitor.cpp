#include "filemonitor.h"

std::mutex m;

std::map<std::string, t_file_stats> fileStats = std::map<std::string, t_file_stats>();
bool showCorruptedPlotFiles = true;
int oldLineCount = -1;
bool currentlyDisplayingCorruptedPlotFiles = false;
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
	if (!showCorruptedPlotFiles) {
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
	
	if (lineCount == 0 && currentlyDisplayingCorruptedPlotFiles) {
		clearCorrupted();
		resizeWindows(0);
		refreshCorrupted();
		currentlyDisplayingCorruptedPlotFiles = false;
		return;
	}
	else if (lineCount == 0) {
		return;
	}

	// Incese for header and for clear message.
	lineCount += 2;

	resizeWindows(lineCount);
	if (lineCount != oldLineCount) {
		clearCorrupted();
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
			bm_wprintwC("%-46s %11llu", element.first.c_str(), element.second.matchingDeadlines, 0);
			if (element.second.conflictingDeadlines > 0) {
				bm_wattronC(4);
			}
			bm_wprintwC(" %9llu", element.second.conflictingDeadlines, 0);
			bm_wattroffC(4);
			bm_wattronC(14);
			if (element.second.readErrors > 0) {
				bm_wattronC(4);
			}
			bm_wprintwC(" %9llu\n", element.second.readErrors, 0);
			bm_wattroffC(4);
			
			++lineCount;
		}
	}
	bm_wattroffC(14);
	bm_wmoveC(lineCount, 1);
	bm_wprintwC("Press 'f' to clear data.");
	boxCorrupted();
	refreshCorrupted();
	currentlyDisplayingCorruptedPlotFiles = true;
}