// blagominer.cpp
#include "stdafx.h"
#include "blagominer.h"

// Initialize static member data
std::vector<std::thread> worker;	        // worker threads
const InstructionSet::InstructionSet_Internal InstructionSet::CPU_Rep;
size_t miner_mode = 0;
bool exit_flag = false;
char *p_minerPath = nullptr;
char *pass = nullptr;
unsigned long long baseTarget = 0;
unsigned long long total_size = 0;
unsigned long long POC2StartBlock = 502000;
bool POC2 = false;
unsigned long long targetDeadlineInfo = 0;
volatile int stopThreads = 0;
unsigned long long height = 0;
unsigned long long deadline = 0;
unsigned int scoop = 0;
bool enable_proxy = false;						// enable client/server functionality
bool use_wakeup = false;						// wakeup HDDs if true
bool show_winner = false;
unsigned int hddWakeUpTimer = 180;              // HDD wakeup timer in seconds

bool use_debug = false;

unsigned long long my_target_deadline = MAXDWORD;	// 4294967295;

bool done = false;
HANDLE hHeap;

char signature[33];						// signature of current block
char oldSignature[33];					// signature of last block

 CRITICAL_SECTION sessionsLock;
 CRITICAL_SECTION bestsLock;
 CRITICAL_SECTION sharesLock;
std::vector<t_worker_progress> worker_progress;
std::vector<t_shares> shares;
std::vector<t_best> bests;
std::vector<t_session> sessions;
std::vector<std::string> paths_dir; // ïóòè

SYSTEMTIME cur_time;
sph_shabal_context  local_32;

int load_config(char const *const filename)
{
	FILE * pFile;

	fopen_s(&pFile, filename, "rt");

	if (pFile == nullptr)
	{
		fprintf(stderr, "\nError. config file %s not found\n", filename);
		system("pause");
		exit(-1);
	}

	_fseeki64(pFile, 0, SEEK_END);
	__int64 const size = _ftelli64(pFile);
	_fseeki64(pFile, 0, SEEK_SET);
	char *json_ = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, size + 1);
	if (json_ == nullptr)
	{
		fprintf(stderr, "\nError allocating memory\n");
		system("pause");
		exit(-1);
	}
	fread_s(json_, size, 1, size - 1, pFile);
	fclose(pFile);

	Document document;	// Default template parameter uses UTF8 and MemoryPoolAllocator.
	if (document.Parse<0>(json_).HasParseError()) {
		fprintf(stderr, "\nJSON format error (offset %u) check miner.conf\n%s\n", (unsigned)document.GetErrorOffset(), GetParseError_En(document.GetParseError())); //(offset %s  %s", (unsigned)document.GetErrorOffset(), (char*)document.GetParseError());
		system("pause");
		exit(-1);
	}

	if (document.IsObject())
	{	// Document is a JSON value represents the root of DOM. Root can be either an object or array.

		if (document.HasMember("UseLog") && (document["UseLog"].IsBool()))	use_log = document["UseLog"].GetBool();

		Log_init();

		if (document.HasMember("Mode") && document["Mode"].IsString())
		{
			Log("\nMode: ");
			if (strcmp(document["Mode"].GetString(), "solo") == 0) miner_mode = 0;
			else miner_mode = 1;
			Log_u(miner_mode);
		}

		Log("\nServer: ");
		if (document.HasMember("Server") && document["Server"].IsString())	nodeaddr = document["Server"].GetString();//strcpy_s(nodeaddr, document["Server"].GetString());
		Log(nodeaddr.c_str());

		Log("\nPort: ");
		if (document.HasMember("Port"))
		{
			if (document["Port"].IsString())	nodeport = document["Port"].GetString();
			else if (document["Port"].IsUint())	nodeport = std::to_string(document["Port"].GetUint()); //_itoa_s(document["Port"].GetUint(), nodeport, INET_ADDRSTRLEN-1, 10);
			Log(nodeport.c_str());
		}

		if (document.HasMember("Paths") && document["Paths"].IsArray()) {
			const Value& Paths = document["Paths"];			// Using a reference for consecutive access is handy and faster.
			for (SizeType i = 0; i < Paths.Size(); i++)
			{
				Log("\nPath: ");
				paths_dir.push_back(Paths[i].GetString());
				Log((char*)paths_dir[i].c_str());
			}
		}

		Log("\nCacheSize: ");
		if (document.HasMember("CacheSize") && (document["CacheSize"].IsUint64())) {
			cache_size1 = document["CacheSize"].GetUint64();
			readChunkSize = cache_size1; //sensible default
		}
		Log_u(cache_size1);

		Log("\nCacheSize2: ");
		if (document.HasMember("CacheSize2") && (document["CacheSize2"].IsUint64())) cache_size2 = document["CacheSize2"].GetUint64();
		Log_u(cache_size2);

		Log("\nReadChunkSize: ");
		if (document.HasMember("ReadChunkSize") && (document["ReadChunkSize"].IsUint64())) readChunkSize = document["ReadChunkSize"].GetUint64();
		Log_u(readChunkSize);

		Log("\nUseHDDWakeUp: ");
		if (document.HasMember("UseHDDWakeUp") && (document["UseHDDWakeUp"].IsBool())) use_wakeup = document["UseHDDWakeUp"].GetBool();
		Log_u(use_wakeup);

		Log("\nhddWakeUpTimer: ");
		if (document.HasMember("hddWakeUpTimer") && (document["hddWakeUpTimer"].IsUint())) hddWakeUpTimer = document["hddWakeUpTimer"].GetUint();
		Log_u((size_t)hddWakeUpTimer);

		Log("\nbfsTOCOffset: ");
		if (document.HasMember("bfsTOCOffset") && (document["bfsTOCOffset"].IsUint())) bfsTOCOffset = document["bfsTOCOffset"].GetUint();
		Log_u((size_t)bfsTOCOffset);

		Log("\nSendInterval: ");
		if (document.HasMember("SendInterval") && (document["SendInterval"].IsUint())) send_interval = (size_t)document["SendInterval"].GetUint();
		Log_u(send_interval);

		Log("\nUpdateInterval: ");
		if (document.HasMember("UpdateInterval") && (document["UpdateInterval"].IsUint())) update_interval = (size_t)document["UpdateInterval"].GetUint();
		Log_u(update_interval);

		Log("\nDebug: ");
		if (document.HasMember("Debug") && (document["Debug"].IsBool()))	use_debug = document["Debug"].GetBool();
		Log_u(use_debug);

		Log("\nUpdater address: ");
		if (document.HasMember("UpdaterAddr") && document["UpdaterAddr"].IsString()) updateraddr = document["UpdaterAddr"].GetString(); //strcpy_s(updateraddr, document["UpdaterAddr"].GetString());
		Log(updateraddr.c_str());

		Log("\nUpdater port: ");
		if (document.HasMember("UpdaterPort"))
		{
			if (document["UpdaterPort"].IsString())	updaterport = document["UpdaterPort"].GetString();
			else if (document["UpdaterPort"].IsUint())	 updaterport = std::to_string(document["UpdaterPort"].GetUint());
		}
		Log(updaterport.c_str());

		Log("\nInfo address: ");
		if (document.HasMember("InfoAddr") && document["InfoAddr"].IsString())	infoaddr = document["InfoAddr"].GetString();
		else infoaddr = updateraddr;
		Log(infoaddr.c_str());

		Log("\nInfo port: ");
		if (document.HasMember("InfoPort"))
		{
			if (document["InfoPort"].IsString())	infoport = document["InfoPort"].GetString();
			else if (document["InfoPort"].IsUint())	infoport = std::to_string(document["InfoPort"].GetUint());
		}
		else infoport = updaterport;
		Log(infoport.c_str());

		Log("\nEnableProxy: ");
		if (document.HasMember("EnableProxy") && (document["EnableProxy"].IsBool())) enable_proxy = document["EnableProxy"].GetBool();
		Log_u(enable_proxy);

		Log("\nProxyPort: ");
		if (document.HasMember("ProxyPort"))
		{
			if (document["ProxyPort"].IsString())	proxyport = document["ProxyPort"].GetString();
			else if (document["ProxyPort"].IsUint())	proxyport = std::to_string(document["ProxyPort"].GetUint());
		}
		Log(proxyport.c_str());

		Log("\nShowWinner: ");
		if (document.HasMember("ShowWinner") && (document["ShowWinner"].IsBool()))	show_winner = document["ShowWinner"].GetBool();
		Log_u(show_winner);

		Log("\nTargetDeadline: ");
		if (document.HasMember("TargetDeadline") && (document["TargetDeadline"].IsInt64()))	my_target_deadline = document["TargetDeadline"].GetUint64();
		Log_llu(my_target_deadline);

		Log("\nUseBoost: ");
		if (document.HasMember("UseBoost") && (document["UseBoost"].IsBool())) use_boost = document["UseBoost"].GetBool();
		Log_u(use_boost);

		Log("\nWinSizeX: ");
		if (document.HasMember("WinSizeX") && (document["WinSizeX"].IsUint())) win_size_x = (short)document["WinSizeX"].GetUint();
		Log_u(win_size_x);

		Log("\nWinSizeY: ");
		if (document.HasMember("WinSizeY") && (document["WinSizeY"].IsUint())) win_size_y = (short)document["WinSizeY"].GetUint();
		Log_u(win_size_y);

		Log("\nPOC2StartBlock: ");
		if (document.HasMember("POC2StartBlock") && (document["POC2StartBlock"].IsUint64())) POC2StartBlock = document["POC2StartBlock"].GetUint64();
		Log_llu(POC2StartBlock);

#ifdef GPU_ON_C
		Log("\nGPU_Platform: ");
		if (document.HasMember("GPU_Platform") && (document["GPU_Platform"].IsInt())) gpu_devices.use_gpu_platform = (size_t)document["GPU_Platform"].GetUint();
		Log_llu(gpu_devices.use_gpu_platform);

		Log("\nGPU_Device: ");
		if (document.HasMember("GPU_Device") && (document["GPU_Device"].IsInt())) gpu_devices.use_gpu_device = (size_t)document["GPU_Device"].GetUint();
		Log_llu(gpu_devices.use_gpu_device);
#endif	

	}

	Log("\n=== Config loaded ===");
	HeapFree(hHeap, 0, json_);
	return 1;
}


void GetCPUInfo(void)
{
	ULONGLONG  TotalMemoryInKilobytes = 0;

	bm_wprintw("CPU support: ");
	if (InstructionSet::AES())   bm_wprintw(" AES ", 0);
	if (InstructionSet::SSE())   bm_wprintw(" SSE ", 0);
	if (InstructionSet::SSE2())  bm_wprintw(" SSE2 ", 0);
	if (InstructionSet::SSE3())  bm_wprintw(" SSE3 ", 0);
	if (InstructionSet::SSE42()) bm_wprintw(" SSE4.2 ", 0);
	if (InstructionSet::AVX())   bm_wprintw(" AVX ", 0);
	if (InstructionSet::AVX2())  bm_wprintw(" AVX2 ", 0);
	if (InstructionSet::AVX512F())  bm_wprintw(" AVX512F ", 0);

#ifndef __AVX__
	// Checking for AVX requires 3 things:
	// 1) CPUID indicates that the OS uses XSAVE and XRSTORE instructions (allowing saving YMM registers on context switch)
	// 2) CPUID indicates support for AVX
	// 3) XGETBV indicates the AVX registers will be saved and restored on context switch
	bool avxSupported = false;
	int cpuInfo[4];
	__cpuid(cpuInfo, 1);

	bool osUsesXSAVE_XRSTORE = cpuInfo[2] & (1 << 27) || false;
	bool cpuAVXSuport = cpuInfo[2] & (1 << 28) || false;

	if (osUsesXSAVE_XRSTORE && cpuAVXSuport)
	{
		// Check if the OS will save the YMM registers
		unsigned long long xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
		avxSupported = (xcrFeatureMask & 0x6) == 0x6;
	}
	if (avxSupported)	bm_wprintw("     [recomend use AVX]", 0);
#endif
	if (InstructionSet::AVX2()) bm_wprintw("     [recomend use AVX2]", 0);
	if (InstructionSet::AVX512F()) bm_wprintw("     [recomend use AVX512F]", 0);
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	bm_wprintw("\n%s", InstructionSet::Vendor().c_str(), 0);
	bm_wprintw(" %s  [%u cores]", InstructionSet::Brand().c_str(), sysinfo.dwNumberOfProcessors);

	if (GetPhysicallyInstalledSystemMemory(&TotalMemoryInKilobytes))
		bm_wprintw("\nRAM: %llu Mb", (unsigned long long)TotalMemoryInKilobytes / 1024, 0);

	bm_wprintw("\n", 0);
}


void GetPass(char const *const p_strFolderPath)
{
	FILE * pFile;
	unsigned char * buffer;
	size_t len_pass;
	char * filename = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, MAX_PATH);
	if (filename == nullptr) ShowMemErrorExit();
	sprintf_s(filename, MAX_PATH, "%s%s", p_strFolderPath, "passphrases.txt");

	//  printf_s("\npass from: %s\n",filename);
	fopen_s(&pFile, filename, "rt");
	if (pFile == nullptr)
	{
		bm_wattron(12);
		bm_wprintw("Error: passphrases.txt not found. File is needed for solo mining.\n", 0);
		refreshMain();
		bm_wattroff(12);
		system("pause");
		exit(-1);
	}
	HeapFree(hHeap, 0, filename);
	_fseeki64(pFile, 0, SEEK_END);
	size_t const lSize = _ftelli64(pFile);
	_fseeki64(pFile, 0, SEEK_SET);

	buffer = (unsigned char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, lSize + 1);
	if (buffer == nullptr) ShowMemErrorExit();

	len_pass = fread(buffer, 1, lSize, pFile);
	fclose(pFile);

	pass = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, lSize * 3);
	if (pass == nullptr) ShowMemErrorExit();

	for (size_t i = 0, j = 0; i<len_pass; i++, j++)
	{
		if ((buffer[i] == '\n') || (buffer[i] == '\r') || (buffer[i] == '\t')) j--; // Пропускаем символы, переделать buffer[i] < 20
		else
			if (buffer[i] == ' ') pass[j] = '+';
			else
				if (isalnum(buffer[i]) == 0)
				{
					sprintf_s(pass + j, lSize * 3, "%%%x", (unsigned char)buffer[i]);
					j = j + 2;
				}
				else memcpy(&pass[j], &buffer[i], 1);
	}
	//printf_s("\n%s\n",pass);
	HeapFree(hHeap, 0, buffer);
}



size_t GetFiles(const std::string &str, std::vector <t_files> *p_files)
{
	HANDLE hFile = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATAA   FindFileData;
	size_t count = 0;
	std::vector<std::string> path;
	size_t first = 0;
	size_t last = 0;

	//parse path info
	do {
		//check for sequential paths and process path by path
		last = str.find("+", first);
		if (last == -1) last = str.length();
		std::string str2(str.substr(first, last - first));
		//check if path ends with backslash and append if not
		//but dont append backslash if path starts with backslash (BFS)
		if ((str2.rfind("\\") < str2.length() - 1 && str2.find("\\") != 0)) str2 = str2 + "\\";  //dont append BFS
		path.push_back(str2); 
		first = last + 1;
	} while (last != str.length());

	//read all path
	for (auto iter = path.begin(); iter != path.end(); ++iter)
	{
		//check if BFS
		if (((std::string)*iter).find("\\\\.") == 0)
		{
			//load bfstoc
			if (!LoadBFSTOC(*path.begin()))
			{
				continue;
			}
			//iterate files
			bool stop = false;
			for (int i = 0; i < 72; i++) {
				if (stop) break;
				//check status
				switch (bfsTOC.plotFiles[i].status) {
				//no more files in TOC
				case 0:
					stop = true;
					break;
				//finished plotfile
				case 1:
					p_files->push_back({
						*path.begin(),
						"FILE_"+std::to_string(i),
						(unsigned long long)bfsTOC.plotFiles[i].nonces * 4096 *64,
						bfsTOC.id, bfsTOC.plotFiles[i].startNonce, bfsTOC.plotFiles[i].nonces, bfsTOC.diskspace/64, bfsTOC.plotFiles[i].startPos, true, true
						});
					count++;
					break;
				//plotting in progress
				case 3:
					break;
				//other file status not supported for mining at the moment
				default:
					break;
				}
			}
		}
		else 
		{
			hFile = FindFirstFileA(LPCSTR((*iter + "*").c_str()), &FindFileData);
			if (INVALID_HANDLE_VALUE != hFile)
			{
				do
				{
					if (FILE_ATTRIBUTE_DIRECTORY & FindFileData.dwFileAttributes) continue; //Skip directories
					char* ekey = strstr(FindFileData.cFileName, "_");
					if (ekey != nullptr)
					{
						char* estart = strstr(ekey + 1, "_");
						if (estart != nullptr)
						{
							char* enonces = strstr(estart + 1, "_");
							if (enonces != nullptr)
							{
								unsigned long long key, nonce, nonces, stagger;
								if (sscanf_s(FindFileData.cFileName, "%llu_%llu_%llu_%llu", &key, &nonce, &nonces, &stagger) == 4)
								{
									bool p2 = false;
									p_files->push_back({
										*iter,
										FindFileData.cFileName,
										(((static_cast<ULONGLONG>(FindFileData.nFileSizeHigh) << (sizeof(FindFileData.nFileSizeLow) * 8)) | FindFileData.nFileSizeLow)),
										key, nonce, nonces, stagger, 0, p2, false
										});
									count++;
									continue;
								}

							}
							//POC2 FILE
							unsigned long long key, nonce, nonces;
							if (sscanf_s(FindFileData.cFileName, "%llu_%llu_%llu", &key, &nonce, &nonces) == 3)
							{
								bool p2 = true;
								p_files->push_back({
									*iter,
									FindFileData.cFileName,
									(((static_cast<ULONGLONG>(FindFileData.nFileSizeHigh) << (sizeof(FindFileData.nFileSizeLow) * 8)) | FindFileData.nFileSizeLow)),
									key, nonce, nonces, nonces, 0, p2, false
									});
								count++;
							}

						}
					}
				} while (FindNextFileA(hFile, &FindFileData));
				FindClose(hFile);
			}
		}
	}
	return count;
}


int main(int argc, char **argv) {
	//init


	hHeap = GetProcessHeap();
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

	//initialize time 
	LARGE_INTEGER li;
	__int64 start_threads_time, end_threads_time, curr_time;
	QueryPerformanceFrequency(&li);
	double pcFreq = double(li.QuadPart);

	std::thread proxy;
	std::vector<std::thread> generator;

	InitializeCriticalSection(&sessionsLock);
	InitializeCriticalSection(&bestsLock);
	InitializeCriticalSection(&sharesLock);

	char tbuffer[9];
	unsigned long long bytesRead = 0;
	FILE * pFileStat;

	shares.reserve(20);
	bests.reserve(4);
	sessions.reserve(20);


	//get application path
	size_t const cwdsz = GetCurrentDirectoryA(0, 0);
	p_minerPath = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, cwdsz + 2);
	if (p_minerPath == nullptr)
	{
		fprintf(stderr, "\nError allocating memory\n");
		system("pause");
		exit(-1);
	}
	GetCurrentDirectoryA(DWORD(cwdsz), LPSTR(p_minerPath));
	strcat_s(p_minerPath, cwdsz + 2, "\\");


	char* conf_filename = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, MAX_PATH);
	if (conf_filename == nullptr)
	{
		fprintf(stderr, "\nError allocating memory\n");
		system("pause");
		exit(-1);
	}

	//config-file: check -config flag or default to miner.conf
	if ((argc >= 2) && (strcmp(argv[1], "-config") == 0)) {
		if (strstr(argv[2], ":\\")) sprintf_s(conf_filename, MAX_PATH, "%s", argv[2]);
		else sprintf_s(conf_filename, MAX_PATH, "%s%s", p_minerPath, argv[2]);
	}
	else sprintf_s(conf_filename, MAX_PATH, "%s%s", p_minerPath, "miner.conf");

	//load config
	load_config(conf_filename);
	HeapFree(hHeap, 0, conf_filename);

	Log("\nMiner path: "); Log(p_minerPath);

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD max_coord = GetLargestConsoleWindowSize(hConsole);
	if (win_size_x > max_coord.X) win_size_x = max_coord.X;
	if (win_size_y > max_coord.Y) win_size_y = max_coord.Y;

	COORD coord;
	coord.X = win_size_x;
	coord.Y = win_size_y;

	SMALL_RECT Rect;
	Rect.Top = 0;
	Rect.Left = 0;
	Rect.Bottom = coord.Y - 1;
	Rect.Right = coord.X - 1;

	SetConsoleScreenBufferSize(hConsole, coord);
	SetConsoleWindowInfo(hConsole, TRUE, &Rect);

	RECT wSize;
	GetWindowRect(GetConsoleWindow(), &wSize);
	MoveWindow(GetConsoleWindow(), 0, 0, wSize.right - wSize.left, wSize.bottom - wSize.top, true);

	bm_init();
	bm_wattron(12);
	bm_wprintw("\nBURST miner, %s", version, 0);
	bm_wattroff(12);
	bm_wattron(4);
	bm_wprintw("\nProgramming: dcct (Linux) & Blago (Windows)\n", 0);
	bm_wprintw("POC2 mod: Quibus & Johnny (5/2018)\n", 0);
	bm_wattroff(4);

	GetCPUInfo();

	refreshMain();
	refreshProgress();

	if (miner_mode == 0) GetPass(p_minerPath);

	// адрес и порт сервера
	Log("\nSearching servers...");
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		bm_wprintw("WSAStartup failed\n", 0);
		exit(-1);
	}

	char* updaterip = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
	if (updaterip == nullptr) ShowMemErrorExit();
	char* nodeip = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
	if (nodeip == nullptr) ShowMemErrorExit();
	char* infoip = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
	if (infoip == nullptr) ShowMemErrorExit();	bm_wattron(11);

	hostname_to_ip(nodeaddr.c_str(), nodeip);
	bm_wprintw("Pool address    %s (ip %s:%s)\n", nodeaddr.c_str(), nodeip, nodeport.c_str(), 0);

	if (updateraddr.length() > 3) hostname_to_ip(updateraddr.c_str(), updaterip);
	bm_wprintw("Updater address %s (ip %s:%s)\n", updateraddr.c_str(), updaterip, updaterport.c_str(), 0);
	if (show_winner) {
		if (infoaddr.length() > 3) hostname_to_ip(infoaddr.c_str(), infoip);
		bm_wprintw("Info address    %s (ip %s:%s)\n", infoaddr.c_str(), infoip, infoport.c_str(), 0);
	}
	bm_wattroff(11);
	HeapFree(hHeap, 0, updaterip);
	HeapFree(hHeap, 0, nodeip);


	// обнуляем сигнатуру
	RtlSecureZeroMemory(oldSignature, 33);
	RtlSecureZeroMemory(signature, 33);

	// Инфа по файлам
	bm_wattron(15);
	bm_wprintw("Using plots:\n", 0);
	bm_wattroff(15);

	std::vector<t_files> all_files;
	total_size = 0;
	for (auto iter = paths_dir.begin(); iter != paths_dir.end(); ++iter) {
		std::vector<t_files> files;
		GetFiles(*iter, &files);

		unsigned long long tot_size = 0;
		for (auto it = files.begin(); it != files.end(); ++it) {
			tot_size += it->Size;
			all_files.push_back(*it);
		}
		bm_wprintw("%s\tfiles: %2Iu\t size: %4llu Gb\n", (char*)iter->c_str(), (unsigned)files.size(), tot_size / 1024 / 1024 / 1024, 0);
		total_size += tot_size;
	}
	bm_wattron(15);
	bm_wprintw("TOTAL: %llu Gb\n", total_size / 1024 / 1024 / 1024, 0);
	bm_wattroff(15);

	if (total_size == 0) {
		bm_wattron(12);
		bm_wprintw("\n Plot files not found...please check the \"PATHS\" parameter in your config file.\n Press any key for exit...");
		bm_wattroff(12);
		refreshMain();
		system("pause");
		exit(0);
	}

	// Check overlapped plots
	for (size_t cx = 0; cx < all_files.size(); cx++) {
		for (size_t cy = cx + 1; cy < all_files.size(); cy++) {
			if (all_files[cy].Key == all_files[cx].Key)
				if (all_files[cy].StartNonce >= all_files[cx].StartNonce) {
					if (all_files[cy].StartNonce < all_files[cx].StartNonce + all_files[cx].Nonces) {
						bm_wattron(12);
						bm_wprintw("\nWARNING: %s%s and \n%s%s are overlapped\n", all_files[cx].Path.c_str(), all_files[cx].Name.c_str(), all_files[cy].Path.c_str(), all_files[cy].Name.c_str(), 0);
						bm_wattroff(12);
					}
				}
				else
					if (all_files[cy].StartNonce + all_files[cy].Nonces > all_files[cx].StartNonce) {
						bm_wattron(12);
						bm_wprintw("\nWARNING: %s%s and \n%s%s are overlapped\n", all_files[cx].Path.c_str(), all_files[cx].Name.c_str(), all_files[cy].Path.c_str(), all_files[cy].Name.c_str(), 0);
						bm_wattroff(12);
					}
		}
	}
	//all_files.~vector();   // ???

	// Run Proxy
	if (enable_proxy)
	{
		proxy = std::thread(proxy_i);
		bm_wattron(25);
		bm_wprintw("Proxy thread started\n", 0);
		bm_wattroff(25);
	}

	// Run updater;
	std::thread updater(updater_i);
	Log("\nUpdater thread started");

	Log("\nUpdate mining info");
	while (height == 0)
	{
		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	};


	// Main loop
	// Create Shabal Contexts
#ifdef __AVX512F__
	simd512_mshabal_init(&global_512, 256);
	//create minimal context
	global_512_fast.out_size = global_512.out_size;
	for (int j = 0;j<704;j++)
		global_512_fast.state[j] = global_512.state[j];
	global_512_fast.Whigh = global_512.Whigh;
	global_512_fast.Wlow = global_512.Wlow;
#else
	#ifdef __AVX2__
		simd256_mshabal_init(&global_256, 256);
		//create minimal context
		global_256_fast.out_size = global_256.out_size;
		for (int j = 0;j<352;j++)
			global_256_fast.state[j] = global_256.state[j];
		global_256_fast.Whigh = global_256.Whigh;
		global_256_fast.Wlow = global_256.Wlow;
	
	#else
		#ifdef __AVX__
			simd128_mshabal_init(&global_128, 256);
			//create minimal context
			global_128_fast.out_size = global_128.out_size;
			for (int j = 0;j<176;j++)
			global_128_fast.state[j] = global_128.state[j];
			global_128_fast.Whigh = global_128.Whigh;
			global_128_fast.Wlow = global_128.Wlow;
		
		#else
			//disable for non SSE!
			simd128_mshabal_init(&global_128, 256);
			//create minimal context
			global_128_fast.out_size = global_128.out_size;
			for (int j = 0;j<176;j++)
				global_128_fast.state[j] = global_128.state[j];
				global_128_fast.Whigh = global_128.Whigh;
				global_128_fast.Wlow = global_128.Wlow;
	
		#endif
	#endif 
#endif 
	//context for signature calculation
	sph_shabal256_init(&global_32);
	memcpy(&local_32, &global_32, sizeof(global_32));

	for (; !exit_flag;)
	{
		worker.clear();
		worker_progress.clear();
		stopThreads = 0;
		done = false;

		char scoopgen[40];
		memmove(scoopgen, signature, 32);
		const char *mov = (char*)&height;
		scoopgen[32] = mov[7]; scoopgen[33] = mov[6]; scoopgen[34] = mov[5]; scoopgen[35] = mov[4]; scoopgen[36] = mov[3]; scoopgen[37] = mov[2]; scoopgen[38] = mov[1]; scoopgen[39] = mov[0];

		sph_shabal256(&local_32, (const unsigned char*)(const unsigned char*)scoopgen, 40);
		char xcache[32];
		sph_shabal256_close(&local_32, xcache);

		scoop = (((unsigned char)xcache[31]) + 256 * (unsigned char)xcache[30]) % 4096;

		deadline = 0;



		Log("\n------------------------    New block: "); Log_llu(height);

		_strtime_s(tbuffer);
		bm_wattron(25);
		bm_wprintw("\n%s New block %llu, baseTarget %llu, netDiff %llu Tb, POC%i      \n", tbuffer, height, baseTarget, 4398046511104 / 240 / baseTarget, POC2 ? 2 : 1, 0);
		bm_wattron(25);
		if (miner_mode == 0)
		{
			unsigned long long sat_total_size = 0;
			for (auto It = satellite_size.begin(); It != satellite_size.end(); ++It) sat_total_size += It->second;
			bm_wprintw("*** Chance to find a block: %.5f%%  (%llu Gb)\n", ((double)((sat_total_size * 1024 + total_size / 1024 / 1024) * 100 * 60)*(double)baseTarget) / 1152921504606846976, sat_total_size + total_size / 1024 / 1024 / 1024, 0);
		}

		EnterCriticalSection(&sessionsLock);
		for (auto it = sessions.begin(); it != sessions.end(); ++it) closesocket(it->Socket);
		sessions.clear();
		LeaveCriticalSection(&sessionsLock);

		EnterCriticalSection(&sharesLock);
		shares.clear();
		LeaveCriticalSection(&sharesLock);

		EnterCriticalSection(&bestsLock);
		bests.clear();
		LeaveCriticalSection(&bestsLock);

		if ((targetDeadlineInfo > 0) && (targetDeadlineInfo < my_target_deadline)) {
			Log("\nUpdate targetDeadline: "); Log_llu(targetDeadlineInfo);
		}
		else targetDeadlineInfo = my_target_deadline;


		// Run Sender
		std::thread sender(send_i);

		// Run Threads
		QueryPerformanceCounter((LARGE_INTEGER*)&start_threads_time);
		double threads_speed = 0;

		for (size_t i = 0; i < paths_dir.size(); i++)
		{
			worker_progress.push_back({ i, 0, true });
			worker.push_back(std::thread(work_i, i));
		}


		memmove(oldSignature, signature, 32);
		unsigned long long old_baseTarget = baseTarget;
		unsigned long long old_height = height;
		clearProgress();


		// Wait until signature changed or exit
		while ((memcmp(signature, oldSignature, 32) == 0) && !exit_flag)
		{
			switch (bm_wgetchMain())
			{
			case 'q':
				exit_flag = true;
				break;
			case 'r':
				bm_wattron(15);
				bm_wprintw("Recommended size for this block: %llu Gb\n", (4398046511104 / baseTarget) * 1024 / targetDeadlineInfo);
				bm_wattroff(15);
				break;
			case 'c':
				bm_wprintw("*** Chance to find a block: %.5f%%  (%llu Gb)\n", ((double)((total_size / 1024 / 1024) * 100 * 60)*(double)baseTarget) / 1152921504606846976, total_size / 1024 / 1024 / 1024, 0);
				break;
			}
			boxProgress();
			bytesRead = 0;

			int threads_runing = 0;
			for (auto it = worker_progress.begin(); it != worker_progress.end(); ++it)
			{
				bytesRead += it->Reads_bytes;
				threads_runing += it->isAlive;
			}

			if (threads_runing)
			{
				QueryPerformanceCounter((LARGE_INTEGER*)&end_threads_time);
				threads_speed = (double)(bytesRead / (1024 * 1024)) / ((double)(end_threads_time - start_threads_time) / pcFreq);
			}
			else {
				/*
				if (can_generate == 1)
				{
				Log("\nStart Generator. ");
				for (size_t i = 0; i < std::thread::hardware_concurrency()-1; i++)	generator.push_back(std::thread(generator_i, i));
				can_generate = 2;
				}
				*/
				//Work Done! Cleanup / Prepare
				if (!done) {
					//Display total round time
					if (use_debug)
					{
						QueryPerformanceCounter((LARGE_INTEGER*)&end_threads_time);
						double thread_time = (double)(end_threads_time - start_threads_time) / pcFreq;
						char tbuffer[9];
						_strtime_s(tbuffer);
						bm_wattron(7);
						bm_wprintw("%s Total round time: %.1f sec\n", tbuffer, thread_time, 0);
						bm_wattroff(7);
					}
					//prepare
					memcpy(&local_32, &global_32, sizeof(global_32));
				}
				if (use_wakeup)
				{
					QueryPerformanceCounter((LARGE_INTEGER*)&curr_time);
					if ((curr_time - end_threads_time) / pcFreq > hddWakeUpTimer)
					{
						std::vector<t_files> tmp_files;
						for (size_t i = 0; i < paths_dir.size(); i++)		GetFiles(paths_dir[i], &tmp_files);
						if (use_debug)
						{
							char tbuffer[9];
							_strtime_s(tbuffer);
							bm_wattron(7);
							bm_wprintw("%s HDD, WAKE UP !\n", tbuffer, 0);
							bm_wattroff(7);
						}
						end_threads_time = curr_time;
					}
				}
				done = true;
			}


			bm_wmoveP();
			bm_wattronP(14);
			if (deadline == 0)
				bm_wprintwP("%3llu%% %6llu GB (%.2f MB/s). no deadline            Connection: %3u%%", (bytesRead * 4096 * 100 / total_size), (bytesRead / (256 * 1024)), threads_speed, network_quality, 0);
			else
				bm_wprintwP("%3llu%% %6llu GB (%.2f MB/s). Deadline =%10llu   Connection: %3u%%", (bytesRead * 4096 * 100 / total_size), (bytesRead / (256 * 1024)), threads_speed, deadline, network_quality, 0);
			bm_wattroffP(14);

			refreshMain();
			refreshProgress();

			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(39));
		}

		stopThreads = 1;   // Tell all threads to stop

		for (auto it = worker.begin(); it != worker.end(); ++it)
		{
			Log("\nInterrupt thread. ");
			if (it->joinable()) it->join();
		}

		Log("\nInterrupt Sender. ");
		if (sender.joinable()) sender.join();

		//TODO outsource as task
		fopen_s(&pFileStat, "stat.csv", "a+t");
		if (pFileStat != nullptr)
		{
			fprintf(pFileStat, "%llu;%llu;%llu\n", old_height, old_baseTarget, deadline);
			fclose(pFileStat);
		}
		//prepare for next round if not yet done
		if (!done) memcpy(&local_32, &global_32, sizeof(global_32));
		//show winner of last round
		if (show_winner && !exit_flag) {
			if (showWinner.joinable()) showWinner.join();
			showWinner = std::thread(ShowWinner, old_height);
		}

	}

	if (pass != nullptr) HeapFree(hHeap, 0, pass);
	if (updater.joinable()) updater.join();
	Log("\nUpdater stopped");
	if (enable_proxy) proxy.join();
	worker.~vector();
	worker_progress.~vector();
	paths_dir.~vector();
	bests.~vector();
	shares.~vector();
	sessions.~vector();
	DeleteCriticalSection(&sessionsLock);
	DeleteCriticalSection(&sharesLock);
	DeleteCriticalSection(&bestsLock);
	HeapFree(hHeap, 0, p_minerPath);

	WSACleanup();
	Log("\nexit");
	fclose(fp_Log);
	return 0;
}
