#include "filemonitor.h"

std::mutex m;

std::map<std::string, t_file_stats> fileStats = std::map<std::string, t_file_stats>();
bool showCorruptedPlotFiles = true;
int oldLineCount = -1;
const std::string header = "File name                                              Matching DLs   Conflicting DLs";

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

void printFileStats() {
	if (!showCorruptedPlotFiles) {
		return;
	}
	std::lock_guard<std::mutex> lockGuard(m);
	int lineCount = 0;
	for (auto& element : fileStats) {
		if (element.second.conflictingDeadlines > 0) {
			++lineCount;
		}
	}
	
	if (lineCount == 0) {
		clearCorrupted();
		return;
	}

	resizeWindows(++lineCount);
	boxCorrupted();
	if (lineCount != oldLineCount) {
		clearCorrupted();
		oldLineCount = lineCount;
	}
	refreshCorrupted();

	lineCount = 1;
	bm_wmoveC(lineCount++, 1);
	bm_wprintwC("%s", header.c_str(), 0);
	
	for (auto& element : fileStats) {
		if (element.second.conflictingDeadlines > 0) {
			bm_wattronC(14);
			bm_wmoveC(lineCount, 1);
			bm_wprintwC("%-54s %12llu", element.first.c_str(), element.second.matchingDeadlines, 0);
			bm_wattronC(4);
			bm_wprintwC(" %17llu", element.second.conflictingDeadlines, 0);
			bm_wattroffC(4);
			++lineCount;
		}
	}
	bm_wattroffC(14);
	refreshCorrupted();
}