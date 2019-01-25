#include "updateChecker.h"

bool newVersionAvailable = false;

LPCWSTR versionUrl = L"https://raw.githubusercontent.com/andzno1/blagominer/master/.version";
//const char* versionPort = "443";

double getDiffernceinDays(const std::time_t end, std::time_t beginning) {
	return std::difftime(end, beginning) / (60 * 60 * 24);
}

//void checkForUpdate(LPCWSTR lpszURL) {
// Source partially from http://www.rohitab.com/discuss/topic/28719-downloading-a-file-winsock-http-c/page-2#entry10072081
void checkForUpdate() {

	std::time_t lastChecked = 0;
	while (!exit_flag) {
		bool error = false;
		if (getDiffernceinDays(std::time(nullptr), lastChecked) > checkForUpdateInterval) {
			Log("UPDATE CHECKER: Checking for new version.");
			LPSTR lpResult = NULL;
			LPSTREAM lpStream;
			if (SUCCEEDED(URLOpenBlockingStream(NULL, versionUrl, &lpStream, 0, NULL))) {
				STATSTG statStream;
				if (SUCCEEDED(lpStream->Stat(&statStream, STATFLAG_NONAME))) {
					DWORD dwSize = statStream.cbSize.LowPart + 1;
					lpResult = (LPSTR)malloc(dwSize);
					if (lpResult) {
						LARGE_INTEGER liPos;
						ZeroMemory(&liPos, sizeof(liPos));
						ZeroMemory(lpResult, dwSize);
						lpStream->Seek(liPos, STREAM_SEEK_SET, NULL);
						lpStream->Read(lpResult, dwSize - 1, NULL);
					}
					else {
						Log("UPDATE CHECKER: Error allocating memory.");
						error = true;
					}
				}
				else {
					Log("UPDATE CHECKER: Error retrieving stream data.");
					error = true;
				}
				lpStream->Release();

			}
			else {
				Log("UPDATE CHECKER: Error opening stream.");
				error = true;
			}

			if (!error) {
				Document document;	// Default template parameter uses UTF8 and MemoryPoolAllocator.
				if (document.Parse<0>(lpResult).HasParseError()) {
					Log("UPDATE CHECKER: Error (offset %u) parsing retrieved data: %s", (unsigned)document.GetErrorOffset(), GetParseError_En(document.GetParseError()));
				}
				else {
					if (document.IsObject()) {
						if (document.HasMember("release") && document["release"].IsString()) {
							std::string releaseVersion = document["release"].GetString();

							std::stringstream ss(releaseVersion);
							std::string item;
							std::vector<std::string> splittedStrings;
							while (std::getline(ss, item, '.'))
							{
								splittedStrings.push_back(item);
							}
							if (splittedStrings.size() == 2) {
								int releaseVersionMajor = std::stoi(splittedStrings.at(0));
								int releaseVersionMinor = std::stoi(splittedStrings.at(1));

								if (releaseVersionMajor >= versionMajor && releaseVersionMinor > versionMinor) {
									Log("UPDATE CHECKER: New verison availabe: %i.%i", releaseVersionMajor, releaseVersionMinor);
									newVersionAvailable = true;
									showNewVersion(releaseVersion);
								}
								else {
									Log("UPDATE CHECKER: The miner is up to date (%i.%i)", versionMajor, versionMinor);
								}
							}
							else {
								Log("UPDATE CHECKER: Error parsing release version number.");
							}
						}
					}
				}
			}
			lastChecked = std::time(nullptr);
		}
	}

	

	
}