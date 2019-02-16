#include "updateChecker.h"

LPCWSTR versionUrl = L"https://raw.githubusercontent.com/andzno1/blagominer/master/.version";

double getDiffernceinDays(const std::time_t end, std::time_t beginning) {
	return std::difftime(end, beginning) / (60 * 60 * 24);
}

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
						if (document.HasMember("major") && document["major"].IsUint() &&
							document.HasMember("minor") && document["minor"].IsUint() &&
							document.HasMember("revision") && document["revision"].IsUint()) {
							
							unsigned int major = document["major"].GetUint();
							unsigned int minor = document["minor"].GetUint();
							unsigned int revision = document["revision"].GetUint();
							
							if (major > versionMajor ||
								(major >= versionMajor && minor > versionMinor) ||
								(major >= versionMajor && minor >= versionMinor && revision > versionRevision)) {
								std::string releaseVersion =
									std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(revision);
								Log("UPDATE CHECKER: New version availabe: %s", releaseVersion.c_str());
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
			lastChecked = std::time(nullptr);
		}
		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(300000));
	}

}