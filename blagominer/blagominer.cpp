// blagominer.cpp
#include "stdafx.h"
#include "blagominer.h"

// Initialize static member data
std::map<size_t, std::thread> worker;	        // worker threads
const InstructionSet::InstructionSet_Internal InstructionSet::CPU_Rep;

std::shared_ptr<t_coin_info> burst = std::make_shared<t_coin_info>();
std::shared_ptr<t_coin_info> bhd = std::make_shared<t_coin_info>();
t_logging loggingConfig = {};

std::vector<std::shared_ptr<t_coin_info>> coins;

bool exit_flag = false;
char *p_minerPath = nullptr;
char *pass = nullptr;
unsigned long long total_size = 0;
bool POC2 = false;
volatile int stopThreads = 0;
bool use_wakeup = false;						// wakeup HDDs if true
unsigned int hddWakeUpTimer = 180;              // HDD wakeup timer in seconds

bool use_debug = false;

bool done = false;
HANDLE hHeap;

 CRITICAL_SECTION sessionsLock;
 CRITICAL_SECTION bestsLock;
 CRITICAL_SECTION sharesLock;
std::map<size_t, t_worker_progress> worker_progress;
std::vector<t_shares> shares;
std::vector<t_best> bests;
std::vector<t_session> sessions;
std::vector<std::string> paths_dir; // ïóòè

sph_shabal_context  local_32;

bool newMiningInfoReceived = false;
char currentSignature[33];
Coins currentCoin;
unsigned long long currentHeight = 0;
unsigned long long currentBaseTarget = 0;
unsigned long long currentTargetDeadlineInfo = 0;

void init_mining_info() {

	burst->mining = std::make_shared<t_mining_info>();
	burst->coin = BURST;
	burst->locks = std::make_shared<t_locks>();
	burst->mining->miner_mode = 0;
	burst->mining->priority = 0;
	burst->mining->interrupted = false;
	burst->mining->baseTarget = 0;
	burst->mining->height = 0;
	burst->mining->deadline = 0;
	burst->mining->scoop = 0;
	burst->mining->enable = true;
	burst->mining->show_winner = false;
	burst->mining->my_target_deadline = MAXDWORD; // 4294967295;
	burst->mining->POC2StartBlock = 502000;
	burst->mining->dirs = std::vector<std::shared_ptr<t_directory_info>>();

	bhd->mining = std::make_shared<t_mining_info>();
	bhd->coin = BHD;
	bhd->locks = std::make_shared<t_locks>();
	bhd->mining->miner_mode = 0;
	bhd->mining->priority = 1;
	bhd->mining->interrupted = false;
	bhd->mining->baseTarget = 0;
	bhd->mining->height = 0;
	bhd->mining->deadline = 0;
	bhd->mining->scoop = 0;
	bhd->mining->enable = false;
	bhd->mining->show_winner = false;
	bhd->mining->my_target_deadline = MAXDWORD; // 4294967295;
	bhd->mining->POC2StartBlock = 0;
	bhd->mining->dirs = std::vector<std::shared_ptr<t_directory_info>>();
}

void init_logging_config() {
	loggingConfig.logAllGetMiningInfos = false;
}

void resetDirs(std::shared_ptr<t_coin_info> coinInfo) {
	Log("Resetting directories for %s.", coinNames[coinInfo->coin]);
	for (auto& directory : coinInfo->mining->dirs) {
		directory->done = false;
		for (auto& file : directory->files) {
			file.done = false;
		}
	}
}

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

		if (document.HasMember("Paths") && document["Paths"].IsArray()) {
			const Value& Paths = document["Paths"];			// Using a reference for consecutive access is handy and faster.
			for (SizeType i = 0; i < Paths.Size(); i++)
			{
				paths_dir.push_back(Paths[i].GetString());
				Log("Path: %s", (char*)paths_dir[i].c_str());
			}
		}

		if (document.HasMember("Burst") && document["Burst"].IsObject())
		{
			
			Log("### Loading configuration for Burstcoin ###");
			
			const Value& settingsBurst = document["Burst"];

			if (settingsBurst.HasMember("Enable") && (settingsBurst["Enable"].IsBool()))	burst->mining->enable = settingsBurst["Enable"].GetBool();
			Log("Enable: %d", burst->mining->enable);

			if (burst->mining->enable) {
				coins.push_back(burst);
				burst->mining->dirs = {};
				for (auto &directory : paths_dir) {
					burst->mining->dirs.push_back(std::make_shared<t_directory_info>(directory, false, std::vector<t_files>()));
				}

				if (settingsBurst.HasMember("Priority") && (settingsBurst["Priority"].IsUint())) {
					burst->mining->priority = settingsBurst["Priority"].GetUint();
				}
				Log("Priority %zu", burst->mining->priority);

				if (settingsBurst.HasMember("Mode") && settingsBurst["Mode"].IsString())
				{
					if (strcmp(settingsBurst["Mode"].GetString(), "solo") == 0) burst->mining->miner_mode = 0;
					else burst->mining->miner_mode = 1;
					Log("Mode: %zu", burst->mining->miner_mode);
				}

				if (settingsBurst.HasMember("Server") && settingsBurst["Server"].IsString())	burst->network->nodeaddr = settingsBurst["Server"].GetString();//strcpy_s(nodeaddr, document["Server"].GetString());
				Log("Server: %s", burst->network->nodeaddr.c_str());

				if (settingsBurst.HasMember("Port"))
				{
					if (settingsBurst["Port"].IsString())	burst->network->nodeport = settingsBurst["Port"].GetString();
					else if (settingsBurst["Port"].IsUint())	burst->network->nodeport = std::to_string(settingsBurst["Port"].GetUint()); //_itoa_s(document["Port"].GetUint(), nodeport, INET_ADDRSTRLEN-1, 10);
					Log("Port %s", burst->network->nodeport.c_str());
				}

				if (settingsBurst.HasMember("UpdaterAddr") && settingsBurst["UpdaterAddr"].IsString()) burst->network->updateraddr = settingsBurst["UpdaterAddr"].GetString(); //strcpy_s(updateraddr, document["UpdaterAddr"].GetString());
				Log("Updater address: %s", burst->network->updateraddr.c_str());

				if (settingsBurst.HasMember("UpdaterPort"))
				{
					if (settingsBurst["UpdaterPort"].IsString())	burst->network->updaterport = settingsBurst["UpdaterPort"].GetString();
					else if (settingsBurst["UpdaterPort"].IsUint())	 burst->network->updaterport = std::to_string(settingsBurst["UpdaterPort"].GetUint());
				}
				Log("Updater port: %s", burst->network->updaterport.c_str());

				if (settingsBurst.HasMember("InfoAddr") && settingsBurst["InfoAddr"].IsString())	burst->network->infoaddr = settingsBurst["InfoAddr"].GetString();
				else burst->network->infoaddr = burst->network->updateraddr;
				Log("Info address: %s", burst->network->infoaddr.c_str());

				if (settingsBurst.HasMember("InfoPort"))
				{
					if (settingsBurst["InfoPort"].IsString())	burst->network->infoport = settingsBurst["InfoPort"].GetString();
					else if (settingsBurst["InfoPort"].IsUint())	burst->network->infoport = std::to_string(settingsBurst["InfoPort"].GetUint());
				}
				else burst->network->infoport = burst->network->updaterport;
				Log("Info port: %s", burst->network->infoport.c_str());

				if (settingsBurst.HasMember("EnableProxy") && (settingsBurst["EnableProxy"].IsBool())) burst->network->enable_proxy = settingsBurst["EnableProxy"].GetBool();
				Log("EnableProxy: %d", burst->network->enable_proxy);

				if (settingsBurst.HasMember("ProxyPort"))
				{
					if (settingsBurst["ProxyPort"].IsString())	burst->network->proxyport = settingsBurst["ProxyPort"].GetString();
					else if (settingsBurst["ProxyPort"].IsUint())	burst->network->proxyport = std::to_string(settingsBurst["ProxyPort"].GetUint());
				}
				Log("ProxyPort: %s", burst->network->proxyport.c_str());

				if (settingsBurst.HasMember("SendInterval") && (settingsBurst["SendInterval"].IsUint())) burst->network->send_interval = (size_t)settingsBurst["SendInterval"].GetUint();
				Log("SendInterval: %zu", burst->network->send_interval);

				if (settingsBurst.HasMember("UpdateInterval") && (settingsBurst["UpdateInterval"].IsUint())) burst->network->update_interval = (size_t)settingsBurst["UpdateInterval"].GetUint();
				Log("UpdateInterval: %zu", burst->network->update_interval);

				if (settingsBurst.HasMember("TargetDeadline") && (settingsBurst["TargetDeadline"].IsInt64()))	burst->mining->my_target_deadline = settingsBurst["TargetDeadline"].GetUint64();
				Log("TargetDeadline: %llu", burst->mining->my_target_deadline);

				if (settingsBurst.HasMember("POC2StartBlock") && (settingsBurst["POC2StartBlock"].IsUint64())) burst->mining->POC2StartBlock = settingsBurst["POC2StartBlock"].GetUint64();
				Log("POC2StartBlock: %llu", burst->mining->POC2StartBlock);

				if (settingsBurst.HasMember("ShowWinner") && (settingsBurst["ShowWinner"].IsBool()))	burst->mining->show_winner = settingsBurst["ShowWinner"].GetBool();
				Log("ShowWinner:  %d", burst->mining->show_winner);
			}
		}


		if (document.HasMember("BHD") && document["BHD"].IsObject())
		{
			Log("### Loading configuration for Bitcoin HD ###");
			
			const Value& settingsBhd = document["BHD"];

			if (settingsBhd.HasMember("Enable") && (settingsBhd["Enable"].IsBool()))	bhd->mining->enable = settingsBhd["Enable"].GetBool();
			Log("Enable: %d", bhd->mining->enable);

			if (bhd->mining->enable) {
				bhd->mining->dirs = {};
				for (auto directory : paths_dir) {
					bhd->mining->dirs.push_back(std::make_shared<t_directory_info>(directory, false, std::vector<t_files>()));
				}

				if (settingsBhd.HasMember("Priority") && (settingsBhd["Priority"].IsUint())) {
					bhd->mining->priority = settingsBhd["Priority"].GetUint();
				}
				Log("Priority: %zu", bhd->mining->priority);

				if (burst->mining->enable && bhd->mining->priority >= burst->mining->priority) {
					coins.push_back(bhd);
				}
				else {
					coins.insert(coins.begin(), bhd);
				}

				if (settingsBhd.HasMember("Mode") && settingsBhd["Mode"].IsString())
				{
					if (strcmp(settingsBhd["Mode"].GetString(), "solo") == 0) bhd->mining->miner_mode = 0;
					else bhd->mining->miner_mode = 1;
					Log("Mode: %zu", bhd->mining->miner_mode);
				}

				if (settingsBhd.HasMember("Server") && settingsBhd["Server"].IsString())	bhd->network->nodeaddr = settingsBhd["Server"].GetString();//strcpy_s(nodeaddr, document["Server"].GetString());
				Log("Server: %s", bhd->network->nodeaddr.c_str());

				if (settingsBhd.HasMember("Port"))
				{
					if (settingsBhd["Port"].IsString())	bhd->network->nodeport = settingsBhd["Port"].GetString();
					else if (settingsBhd["Port"].IsUint())	bhd->network->nodeport = std::to_string(settingsBhd["Port"].GetUint()); //_itoa_s(document["Port"].GetUint(), nodeport, INET_ADDRSTRLEN-1, 10);
					Log("Port: %s", bhd->network->nodeport.c_str());
				}

				if (settingsBhd.HasMember("UpdaterAddr") && settingsBhd["UpdaterAddr"].IsString()) bhd->network->updateraddr = settingsBhd["UpdaterAddr"].GetString(); //strcpy_s(updateraddr, document["UpdaterAddr"].GetString());
				Log("Updater address: %s", bhd->network->updateraddr.c_str());

				if (settingsBhd.HasMember("UpdaterPort"))
				{
					if (settingsBhd["UpdaterPort"].IsString())	bhd->network->updaterport = settingsBhd["UpdaterPort"].GetString();
					else if (settingsBhd["UpdaterPort"].IsUint())	 bhd->network->updaterport = std::to_string(settingsBhd["UpdaterPort"].GetUint());
				}
				Log("Updater port: %s", bhd->network->updaterport.c_str());

				if (settingsBhd.HasMember("InfoAddr") && settingsBhd["InfoAddr"].IsString())	bhd->network->infoaddr = settingsBhd["InfoAddr"].GetString();
				else bhd->network->infoaddr = bhd->network->updateraddr;
				Log("Info address: %s", bhd->network->infoaddr.c_str());

				if (settingsBhd.HasMember("InfoPort"))
				{
					if (settingsBhd["InfoPort"].IsString())	bhd->network->infoport = settingsBhd["InfoPort"].GetString();
					else if (settingsBhd["InfoPort"].IsUint())	bhd->network->infoport = std::to_string(settingsBhd["InfoPort"].GetUint());
				}
				else bhd->network->infoport = bhd->network->updaterport;
				Log("Info port: ", bhd->network->infoport.c_str());

				if (settingsBhd.HasMember("EnableProxy") && (settingsBhd["EnableProxy"].IsBool())) bhd->network->enable_proxy = settingsBhd["EnableProxy"].GetBool();
				Log("EnableProxy: %d", bhd->network->enable_proxy);

				if (settingsBhd.HasMember("ProxyPort"))
				{
					if (settingsBhd["ProxyPort"].IsString())	bhd->network->proxyport = settingsBhd["ProxyPort"].GetString();
					else if (settingsBhd["ProxyPort"].IsUint())	bhd->network->proxyport = std::to_string(settingsBhd["ProxyPort"].GetUint());
				}
				Log("ProxyPort: %s", bhd->network->proxyport.c_str());

				if (settingsBhd.HasMember("SendInterval") && (settingsBhd["SendInterval"].IsUint())) bhd->network->send_interval = (size_t)settingsBhd["SendInterval"].GetUint();
				Log("SendInterval: %zu", bhd->network->send_interval);

				if (settingsBhd.HasMember("UpdateInterval") && (settingsBhd["UpdateInterval"].IsUint())) bhd->network->update_interval = (size_t)settingsBhd["UpdateInterval"].GetUint();
				Log("UpdateInterval: %zu", bhd->network->update_interval);

				if (settingsBhd.HasMember("TargetDeadline") && (settingsBhd["TargetDeadline"].IsInt64()))	bhd->mining->my_target_deadline = settingsBhd["TargetDeadline"].GetUint64();
				Log("TargetDeadline: %llu", bhd->mining->my_target_deadline);

				if (settingsBhd.HasMember("POC2StartBlock") && (settingsBhd["POC2StartBlock"].IsUint64())) bhd->mining->POC2StartBlock = settingsBhd["POC2StartBlock"].GetUint64();
				Log("POC2StartBlock: %llu", bhd->mining->POC2StartBlock);
			}
		}

		if (document.HasMember("Logging") && document["Logging"].IsObject())
		{
			Log("### Loading configuration for Logging ###");

			const Value& logging = document["Logging"];

			if (logging.HasMember("logAllGetMiningInfos") && (logging["logAllGetMiningInfos"].IsBool()))	loggingConfig.logAllGetMiningInfos = logging["logAllGetMiningInfos"].GetBool();
			Log("logAllGetMiningInfos: %d", loggingConfig.logAllGetMiningInfos);
		}
				
		Log("### Loading general configuration ###");

		if (document.HasMember("CacheSize") && (document["CacheSize"].IsUint64())) {
			cache_size1 = document["CacheSize"].GetUint64();
			readChunkSize = cache_size1; //sensible default
		}
		Log("CacheSize: %zu",  cache_size1);

		if (document.HasMember("CacheSize2") && (document["CacheSize2"].IsUint64())) cache_size2 = document["CacheSize2"].GetUint64();
		Log("CacheSize2: %zu", cache_size2);

		if (document.HasMember("ReadChunkSize") && (document["ReadChunkSize"].IsUint64())) readChunkSize = document["ReadChunkSize"].GetUint64();
		Log("ReadChunkSize: %zu", readChunkSize);

		if (document.HasMember("UseHDDWakeUp") && (document["UseHDDWakeUp"].IsBool())) use_wakeup = document["UseHDDWakeUp"].GetBool();
		Log("UseHDDWakeUp: %d", use_wakeup);

		if (document.HasMember("ShowCorruptedPlotFiles") && (document["ShowCorruptedPlotFiles"].IsBool())) showCorruptedPlotFiles = document["ShowCorruptedPlotFiles"].GetBool();
		Log("ShowCorruptedPlotFiles: %d", showCorruptedPlotFiles);

		if (document.HasMember("hddWakeUpTimer") && (document["hddWakeUpTimer"].IsUint())) hddWakeUpTimer = document["hddWakeUpTimer"].GetUint();
		Log("hddWakeUpTimer: %u", hddWakeUpTimer);

		if (document.HasMember("bfsTOCOffset") && (document["bfsTOCOffset"].IsUint())) bfsTOCOffset = document["bfsTOCOffset"].GetUint();
		Log("bfsTOCOffset: %u", bfsTOCOffset);

		if (document.HasMember("Debug") && (document["Debug"].IsBool()))	use_debug = document["Debug"].GetBool();
		Log("Debug: %d", use_debug);
		
		if (document.HasMember("UseBoost") && (document["UseBoost"].IsBool())) use_boost = document["UseBoost"].GetBool();
		Log("UseBoost: %d", use_boost);

		if (document.HasMember("WinSizeX") && (document["WinSizeX"].IsUint())) win_size_x = (short)document["WinSizeX"].GetUint();
		if (win_size_x < 80) win_size_x = 80;
		Log("WinSizeX: %hi", win_size_x);

		if (document.HasMember("WinSizeY") && (document["WinSizeY"].IsUint())) win_size_y = (short)document["WinSizeY"].GetUint();
		Log("WinSizeY: %hi", win_size_y);

#ifdef GPU_ON_C
		if (document.HasMember("GPU_Platform") && (document["GPU_Platform"].IsInt())) gpu_devices.use_gpu_platform = (size_t)document["GPU_Platform"].GetUint();
		Log("GPU_Platform: %u", gpu_devices.use_gpu_platform);

		if (document.HasMember("GPU_Device") && (document["GPU_Device"].IsInt())) gpu_devices.use_gpu_device = (size_t)document["GPU_Device"].GetUint();
		Log("GPU_Device: %u", gpu_devices.use_gpu_device);
#endif	

	}

	Log("=== Config loaded ===");
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
						false,
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
										false,
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
									false,
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

unsigned int calcScoop() {
	char scoopgen[40];
	memmove(scoopgen, currentSignature, 32);
	const char *mov = (char*)&currentHeight;
	scoopgen[32] = mov[7];
	scoopgen[33] = mov[6];
	scoopgen[34] = mov[5];
	scoopgen[35] = mov[4];
	scoopgen[36] = mov[3];
	scoopgen[37] = mov[2];
	scoopgen[38] = mov[1];
	scoopgen[39] = mov[0];

	sph_shabal256(&local_32, (const unsigned char*)(const unsigned char*)scoopgen, 40);
	char xcache[32];
	sph_shabal256_close(&local_32, xcache);

	return (((unsigned char)xcache[31]) + 256 * (unsigned char)xcache[30]) % 4096;
}

void insertIntoQueue(std::vector<std::shared_ptr<t_coin_info>>& queue, std::shared_ptr<t_coin_info> coin) {
	bool inserted = false;
	for (auto it = queue.begin(); it != queue.end(); ++it) {
		if (coin->mining->priority < (*it)->mining->priority) {
			Log("Adding %s to the queue before %s.", coinNames[coin->coin], coinNames[(*it)->coin]);
			queue.insert(it, coin);
			inserted = true;
			break;
		}
		if (coin == (*it)) {
			Log("Coin %s already in queue. No action needed", coinNames[coin->coin]);
			inserted = true;
			break;
		}
	}
	if (!inserted) {
		Log("Adding %s to the end of the queue.", coinNames[coin->coin]);
		queue.push_back(coin);
	}
}

bool hasSignatureChanged(const std::vector<std::shared_ptr<t_coin_info>>& coins,
	std::vector<std::shared_ptr<t_coin_info>>& elems) {
	bool ret = false;
	for (auto& pt : coins) {
		if (pt->mining->enable && signaturesDiffer(pt)) {
			Log("Signature for %s changed.", coinNames[pt->coin]);
			// Setting interrupted to false in case the coin with changed signature has been
			// scheduled for continuing.
			pt->mining->interrupted = false;
			updateOldSignature(pt);
			insertIntoQueue(elems, pt);
			ret = true;
		}
	}
	return ret;
}

bool needToInterruptMining(const std::vector<std::shared_ptr<t_coin_info>>& coins,
	std::shared_ptr<t_coin_info > coin,
	std::vector<std::shared_ptr<t_coin_info>>& elems) {
	if (hasSignatureChanged(coins, elems)) {
		// Checking only the first element, since it has already the highest priority (but lowest value).
		if (elems.front()->mining->priority <= coin->mining->priority) {
			if (!done) {
				Log("Interrupting current mining progress. %s has a higher or equal priority than %s.",
					coinNames[elems.front()->coin], coinNames[coin->coin]);
			}
			return true;
		}
		else if (!done) {
			char tbuffer[9];
			_strtime_s(tbuffer);
			bm_wattron(5);
			bm_wprintw("\n%s Adding %s block %llu to the end of the queue.\n", tbuffer, coinNames[elems.front()->coin], elems.front()->mining->height, 0);
			bm_wattron(5);
		}
	}
	return false;
}

void updateGlobals(std::shared_ptr<t_coin_info> coin) {
	char* sig = getSignature(coin);
	memmove(currentSignature, sig, 32);
	delete[] sig;

	currentCoin = coin->coin;
	currentHeight = coin->mining->height;
	currentBaseTarget = coin->mining->baseTarget;
}

unsigned long long getPlotFilesSize(std::vector<std::string>& directories, bool log, std::vector<t_files>& all_files) {
	unsigned long long size = 0;
	for (auto iter = directories.begin(); iter != directories.end(); ++iter) {
		std::vector<t_files> files;
		GetFiles(*iter, &files);

		unsigned long long tot_size = 0;
		for (auto it = files.begin(); it != files.end(); ++it) {
			tot_size += it->Size;
			all_files.push_back(*it);
		}
		if (log) {
			bm_wprintw("%s\tfiles: %2Iu\t size: %4llu Gb\n", (char*)iter->c_str(), (unsigned)files.size(), tot_size / 1024 / 1024 / 1024, 0);
		}
		size += tot_size;
	}
	return size;
}

unsigned long long getPlotFilesSize(std::vector<std::string>& directories, bool log) {
	std::vector<t_files> dump;
	return getPlotFilesSize(directories, log, dump);
}

unsigned long long getPlotFilesSize(std::vector<std::shared_ptr<t_directory_info>> dirs) {
	unsigned long long size = 0;
	for (auto &dir : dirs) {
		for (auto &f : dir->files) {
			if (!f.done) {
				size += f.Size;
			}
		}
	}
	return size;
}

/*
	Partially taken from https://sourceforge.net/p/dosbox/code-0/HEAD/tree/dosbox/trunk/src/debug/debug_win32.cpp#l24
*/
static void resizeConsole(SHORT xSize, SHORT ySize) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi; // Hold Current Console Buffer Info 
	BOOL bSuccess;
	SMALL_RECT srWindowRect;         // Hold the New Console Size 
	COORD coordScreen;

	bSuccess = GetConsoleScreenBufferInfo(hConsole, &csbi);

	// Get the Largest Size we can size the Console Window to 
	coordScreen = GetLargestConsoleWindowSize(hConsole);

	// Define the New Console Window Size and Scroll Position 
	srWindowRect.Right = (SHORT)(min(xSize, coordScreen.X) - 1);
	srWindowRect.Bottom = (SHORT)(min(ySize, coordScreen.Y) - 1);
	srWindowRect.Left = srWindowRect.Top = (SHORT)0;

	// Define the New Console Buffer Size    
	coordScreen.X = xSize;
	coordScreen.Y = ySize;

	// If the Current Buffer is Larger than what we want, Resize the 
	// Console Window First, then the Buffer 
	if ((DWORD)csbi.dwSize.X * csbi.dwSize.Y > (DWORD)xSize * ySize)
	{
		bSuccess = SetConsoleWindowInfo(hConsole, TRUE, &srWindowRect);
		bSuccess = SetConsoleScreenBufferSize(hConsole, coordScreen);
	}

	// If the Current Buffer is Smaller than what we want, Resize the 
	// Buffer First, then the Console Window 
	if ((DWORD)csbi.dwSize.X * csbi.dwSize.Y < (DWORD)xSize * ySize)
	{
		bSuccess = SetConsoleScreenBufferSize(hConsole, coordScreen);
		bSuccess = SetConsoleWindowInfo(hConsole, TRUE, &srWindowRect);
	}


	// Get the monitor that is displaying the window
	HMONITOR monitor = MonitorFromWindow(GetConsoleWindow(), MONITOR_DEFAULTTONEAREST);

	// Get the monitor's offset in virtual-screen coordinates
	MONITORINFO monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFO);
	GetMonitorInfoA(monitor, &monitorInfo);
		
	RECT wSize;
	GetWindowRect(GetConsoleWindow(), &wSize);
	// Move window to top
	MoveWindow(GetConsoleWindow(), wSize.left, monitorInfo.rcWork.top, wSize.right - wSize.left, wSize.bottom - wSize.top, true);

	return;
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

	std::thread proxyBurst;
	std::thread proxyBhd;
	std::vector<std::thread> generator;

	InitializeCriticalSection(&sessionsLock);
	InitializeCriticalSection(&bestsLock);
	InitializeCriticalSection(&sharesLock);

	char tbuffer[9];
	unsigned long long bytesRead = 0;

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
	
	// Initialize configuration.
	init_mining_info();
	init_network_info();

	//load config
	load_config(conf_filename);
	HeapFree(hHeap, 0, conf_filename);

	Csv_Init();

	Log("Miner path: %s", p_minerPath);

	resizeConsole(win_size_x, win_size_y);
	
	bm_init();
	bm_wattron(12);
	bm_wprintw("\nBURST/BHD miner, %s", version, 0);
	bm_wattroff(12);
	bm_wattron(4);
	bm_wprintw("\nProgramming: dcct (Linux) & Blago (Windows)\n", 0);
	bm_wprintw("POC2 mod: Quibus & Johnny (5/2018)\n", 0);
	bm_wprintw("Dual mining mod: andz (1/2019)\n", 0);
	bm_wattroff(4);

	GetCPUInfo();

	refreshMain();
	refreshProgress();

	if ((burst->mining->enable && burst->mining->miner_mode == 0) || (bhd->mining->enable && bhd->mining->miner_mode == 0)) GetPass(p_minerPath);

	// адрес и порт сервера
	Log("Searching servers...");
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		bm_wprintw("WSAStartup failed\n", 0);
		exit(-1);
	}

	if (burst->mining->enable) {
		char* updateripBurst = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
		if (updateripBurst == nullptr) ShowMemErrorExit();
		char* nodeipBurst = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
		if (nodeipBurst == nullptr) ShowMemErrorExit();
		char* infoipBurst = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
		if (infoipBurst == nullptr) ShowMemErrorExit();	bm_wattron(11);

		hostname_to_ip(burst->network->nodeaddr.c_str(), nodeipBurst);
		bm_wprintw("BURST pool address    %s (ip %s:%s)\n", burst->network->nodeaddr.c_str(), nodeipBurst, burst->network->nodeport.c_str(), 0);

		if (burst->network->updateraddr.length() > 3) hostname_to_ip(burst->network->updateraddr.c_str(), updateripBurst);
		bm_wprintw("BURST updater address %s (ip %s:%s)\n", burst->network->updateraddr.c_str(), updateripBurst, burst->network->updaterport.c_str(), 0);

		if (burst->mining->show_winner) {
			if (burst->network->infoaddr.length() > 3) hostname_to_ip(burst->network->infoaddr.c_str(), infoipBurst);
			bm_wprintw("BURST Info address    %s (ip %s:%s)\n", burst->network->infoaddr.c_str(), infoipBurst, burst->network->infoport.c_str(), 0);
		}
		
		HeapFree(hHeap, 0, updateripBurst);
		HeapFree(hHeap, 0, nodeipBurst);
	}

	if (bhd->mining->enable) {
		char* updateripBhd = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
		if (updateripBhd == nullptr) ShowMemErrorExit();
		char* nodeipBhd = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
		if (nodeipBhd == nullptr) ShowMemErrorExit();
		char* infoipBhd = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
		if (infoipBhd == nullptr) ShowMemErrorExit();	bm_wattron(11);

		hostname_to_ip(bhd->network->nodeaddr.c_str(), nodeipBhd);
		bm_wprintw("BHD pool address    %s (ip %s:%s)\n", bhd->network->nodeaddr.c_str(), nodeipBhd, bhd->network->nodeport.c_str(), 0);

		if (bhd->network->updateraddr.length() > 3) hostname_to_ip(bhd->network->updateraddr.c_str(), updateripBhd);
		bm_wprintw("BHD updater address %s (ip %s:%s)\n", bhd->network->updateraddr.c_str(), updateripBhd, bhd->network->updaterport.c_str(), 0);
		
		HeapFree(hHeap, 0, updateripBhd);
		HeapFree(hHeap, 0, nodeipBhd);
	}
	
	bm_wattroff(11);
	
	// обнуляем сигнатуру
	RtlSecureZeroMemory(currentSignature, 33);
	if (burst->mining->enable) {
		RtlSecureZeroMemory(burst->mining->oldSignature, 33);
		RtlSecureZeroMemory(burst->mining->signature, 33);
	}
	if(bhd->mining->enable) {
		RtlSecureZeroMemory(bhd->mining->oldSignature, 33);
		RtlSecureZeroMemory(bhd->mining->signature, 33);
	}

	// Инфа по файлам
	bm_wattron(15);
	bm_wprintw("Using plots:\n", 0);
	bm_wattroff(15);

	std::vector<t_files> all_files;
	total_size = getPlotFilesSize(paths_dir, true, all_files);
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
	if (burst->mining->enable && burst->network->enable_proxy)
	{
		proxyBurst = std::thread(proxy_i, burst);
		bm_wattron(25);
		bm_wprintw("Burstcoin proxy thread started\n", 0);
		bm_wattroff(25);
	}

	if (burst->mining->enable && burst->network->enable_proxy)
	{
		proxyBhd = std::thread(proxy_i, bhd);
		bm_wattron(25);
		bm_wprintw("Bitcoin HD proxy thread started\n", 0);
		bm_wattroff(25);
	}

	// Run updater;
	std::thread updaterBurst;
	if (burst->mining->enable)
	{
		updaterBurst = std::thread(updater_i, burst);
		Log("BURST updater thread started");
	}
	std::thread updaterBhd;
	if (bhd->mining->enable)
	{
		updaterBhd = std::thread(updater_i, bhd);
		Log("BHD updater thread started");
	}	

	Log("Update mining info");
	while ((burst->mining->enable && getHeight(burst) == 0) || (bhd->mining->enable && getHeight(bhd) == 0))
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

	std::vector<std::shared_ptr<t_coin_info>> queue = coins;
	std::shared_ptr<t_coin_info> miningCoin;

	if (burst->mining->enable) {
		updateOldSignature(burst);
	}
	if (bhd->mining->enable) {
		updateOldSignature(bhd);
	}

	for (; !exit_flag && !coins.empty();)
	{
		worker.clear();
		worker_progress.clear();
		stopThreads = 0;
		done = false;
		int oldThreadsRunning = -1;
		double thread_time;

		std::string out = "Coin queue: ";
		for (auto& c : queue) {
			out += coinNames[c->coin];
			if (c != queue.back())	out += ", "; else out += ".";
		}
		Log(out.c_str());
		
		miningCoin = queue.front();
		queue.erase(queue.begin());

		updateGlobals(miningCoin);

		miningCoin->mining->scoop = calcScoop();
		miningCoin->mining->deadline = 0;
		
		_strtime_s(tbuffer);
		if (miningCoin->mining->interrupted) {
			Log("------------------------    Continuing %s block: %llu", coinNames[miningCoin->coin], currentHeight);
			bm_wattron(5);
			bm_wprintw("\n%s Continuing %s block %llu, baseTarget %llu, netDiff %llu Tb, POC%i\n", tbuffer, coinNames[miningCoin->coin], currentHeight, currentBaseTarget, 4398046511104 / 240 / currentBaseTarget, POC2 ? 2 : 1, 0);
			bm_wattron(5);
		}
		else {
			Log("------------------------    New %s block: %llu", coinNames[miningCoin->coin], currentHeight);
			bm_wattron(25);
			bm_wprintw("\n%s New %s block %llu, baseTarget %llu, netDiff %llu Tb, POC%i\n", tbuffer, coinNames[miningCoin->coin], currentHeight, currentBaseTarget, 4398046511104 / 240 / currentBaseTarget, POC2 ? 2 : 1, 0);
			bm_wattron(25);
		}
				
		if (miningCoin->mining->miner_mode == 0)
		{
			unsigned long long sat_total_size = 0;
			for (auto It = satellite_size.begin(); It != satellite_size.end(); ++It) sat_total_size += It->second;
			bm_wprintw("*** Chance to find a block: %.5f%%  (%llu Gb)\n", ((double)((sat_total_size * 1024 + total_size / 1024 / 1024) * 100 * 60)*(double)currentBaseTarget) / 1152921504606846976, sat_total_size + total_size / 1024 / 1024 / 1024, 0);
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

		const unsigned long long tdi = getTargetDeadlineInfo(miningCoin);
		Log("targetDeadlineInfo: %llu", tdi);
		Log("my_target_deadline: %llu", tdi);
		if ((tdi > 0) && (tdi < miningCoin->mining->my_target_deadline)) {
			Log("Target deadline from pool is lower than deadline set in the configuration. Updating targetDeadline: %llu", tdi);
		}
		else {
			setTargetDeadlineInfo(miningCoin, miningCoin->mining->my_target_deadline);
			Log("Using target deadline from configuration: %llu", miningCoin->mining->my_target_deadline);
		}


		// Run Sender
		std::thread sender(send_i, miningCoin, currentHeight, currentBaseTarget);

		// Run Threads
		QueryPerformanceCounter((LARGE_INTEGER*)&start_threads_time);
		double threads_speed = 0;

		if (!miningCoin->mining->interrupted) {
			resetDirs(miningCoin);
		}

		std::vector<std::string> roundDirectories;
		for (size_t i = 0; i < miningCoin->mining->dirs.size(); i++)
		{
			if (miningCoin->mining->dirs.at(i)->done) {
				// This directory has already been processed. Skipping.
				Log("Skipping directory %s", miningCoin->mining->dirs.at(i)->dir.c_str());
				continue;
			}
			worker_progress[i] = { i, 0, true };
			worker[i] = std::thread(work_i, miningCoin, miningCoin->mining->dirs.at(i), i);
			roundDirectories.push_back(miningCoin->mining->dirs.at(i)->dir);
		}
		
		unsigned long long round_size = 0;
		
		if (miningCoin->mining->interrupted) {
			round_size = getPlotFilesSize(miningCoin->mining->dirs);
		}
		else {
			round_size = getPlotFilesSize(roundDirectories, false);
		}		
		
		unsigned long long old_baseTarget = currentBaseTarget;
		unsigned long long old_height = currentHeight;
		clearProgress();

		miningCoin->mining->interrupted = true;
		Log("Directories in this round: %zu", roundDirectories.size());
		Log("Round size: %llu GB", round_size / 1024 / 1024 / 1024);

		// Wait until signature changed or exit
		while ((!newMiningInfoReceived || !needToInterruptMining(coins, miningCoin, queue)) && !exit_flag)
		{
			switch (bm_wgetchMain())
			{
			case 'q':
				exit_flag = true;
				break;
			case 'r':
				bm_wattron(15);
				bm_wprintw("Recommended size for this block: %llu Gb\n", (4398046511104 / currentBaseTarget) * 1024 / currentTargetDeadlineInfo);
				bm_wattroff(15);
				break;
			case 'c':
				bm_wprintw("*** Chance to find a block: %.5f%%  (%llu Gb)\n", ((double)((total_size / 1024 / 1024) * 100 * 60)*(double)currentBaseTarget) / 1152921504606846976, total_size / 1024 / 1024 / 1024, 0);
				break;
			}
			boxProgress();
			bytesRead = 0;

			int threads_running = 0;
			for (auto it = worker_progress.begin(); it != worker_progress.end(); ++it)
			{
				bytesRead += it->second.Reads_bytes;
				threads_running += it->second.isAlive;
			}
			if (threads_running != oldThreadsRunning) {
				Log("threads_running: %i", threads_running);
			}
			oldThreadsRunning = threads_running;

			if (threads_running)
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
					Log("Round done.");
					miningCoin->mining->interrupted = false;
					//Display total round time
					QueryPerformanceCounter((LARGE_INTEGER*)&end_threads_time);
					thread_time = (double)(end_threads_time - start_threads_time) / pcFreq;
					Log("Total round time: %.1f seconds", thread_time);
					if (use_debug)
					{
						char tbuffer[9];
						_strtime_s(tbuffer);
						bm_wattron(7);
						bm_wprintw("%s Total round time: %.1f sec\n", tbuffer, thread_time, 0);
						bm_wattroff(7);
					}
					//prepare
					memcpy(&local_32, &global_32, sizeof(global_32));
				}
				else if (!queue.empty()) {
					Log("Next coin in queue.");
					break;
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
			if (burst->mining->enable && bhd->mining->enable) {
				if (miningCoin->mining->deadline == 0)
					bm_wprintwP("%3llu%% %6llu GB (%.2f MB/s). no deadline            Connection: %3u%%/%3u%%", (bytesRead * 4096 * 100 / round_size), (bytesRead / (256 * 1024)), threads_speed, burst->network->network_quality, bhd->network->network_quality, 0);
				else
					bm_wprintwP("%3llu%% %6llu GB (%.2f MB/s). Deadline =%10llu   Connection: %3u%%/%3u%%", (bytesRead * 4096 * 100 / round_size), (bytesRead / (256 * 1024)), threads_speed, miningCoin->mining->deadline, miningCoin->network->network_quality, bhd->network->network_quality, 0);
			}
			else {
				if (miningCoin->mining->deadline == 0)
					bm_wprintwP("%3llu%% %6llu GB (%.2f MB/s). no deadline            Connection: %3u%%", (bytesRead * 4096 * 100 / round_size), (bytesRead / (256 * 1024)), threads_speed, miningCoin->network->network_quality, 0);
				else
					bm_wprintwP("%3llu%% %6llu GB (%.2f MB/s). Deadline =%10llu   Connection: %3u%%", (bytesRead * 4096 * 100 / round_size), (bytesRead / (256 * 1024)), threads_speed, miningCoin->mining->deadline, miningCoin->network->network_quality, 0);
			}
			bm_wattroffP(14);
			
			printFileStats();
			refreshMain();
			refreshProgress();

			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(39));
		}

		stopThreads = 1;   // Tell all threads to stop

		Log("Waiting for worker threads to shut down.");
		for (auto it = worker.begin(); it != worker.end(); ++it)
		{
			if (it->second.joinable()) {
				it->second.join();
			}
		}

		//Check if mining has been interrupted and there is no new block. If so, add current coin to the end of the queue.
		if (!done) {
			for (auto& dir : miningCoin->mining->dirs) {
				if (!dir->done) {
					Log("This directory has not been completed: %s", dir->dir.c_str());
					bool newBlock = false;
					for (auto& coin : queue) {
						if (coin->coin == miningCoin->coin) {
							// There is a new block for the coin currently being mined.
							Log("Mining %s has been interrupted by a new block.", coinNames[miningCoin->coin]);
							newBlock = true;
							break;
						}
					}
					if (newBlock) {
						break;
					}
					else {
						Log("Mining %s has been interrupted by a coin with higher priority.", coinNames[miningCoin->coin]);
						_strtime_s(tbuffer);
						bm_wattron(8);
						bm_wprintw("\n%s Mining of %s has been interrupted by another coin.\n", tbuffer, coinNames[miningCoin->coin], 0);
						bm_wattroff(8);
						// Queuing the interrupted coin.
						insertIntoQueue(queue, miningCoin);
						break;
					}
				}
			}
		}
		else {
			Log("New block, no mining has been interrupted.");
		}

		Log("Interrupt Sender. ");
		if (sender.joinable()) sender.join();
		std::thread{ Csv_Submitted,  miningCoin->coin, old_height, old_baseTarget, 4398046511104 / 240 / old_baseTarget, thread_time, true, miningCoin->mining->deadline }.detach();
		
		//prepare for next round if not yet done
		if (!done) memcpy(&local_32, &global_32, sizeof(global_32));
		//show winner of last round
		if (miningCoin->coin == BURST && miningCoin->mining->show_winner && !exit_flag) {
			if (showWinnerBurst.joinable()) showWinnerBurst.join();
			showWinnerBurst = std::thread(ShowWinner, miningCoin, old_height);
		}

	}

	if (pass != nullptr) HeapFree(hHeap, 0, pass);
	if (burst->mining->enable && updaterBurst.joinable()) updaterBurst.join();
	if (bhd->mining->enable && updaterBhd.joinable()) updaterBhd.join();
	Log("Updater stopped");
	if (burst->mining->enable && burst->network->enable_proxy && proxyBurst.joinable()) proxyBurst.join();
	if (bhd->mining->enable && bhd->network->enable_proxy && proxyBhd.joinable()) proxyBhd.join();
	worker.~map();
	worker_progress.~map();
	paths_dir.~vector();
	bests.~vector();
	shares.~vector();
	sessions.~vector();
	DeleteCriticalSection(&sessionsLock);
	DeleteCriticalSection(&sharesLock);
	DeleteCriticalSection(&bestsLock);
	HeapFree(hHeap, 0, p_minerPath);

	WSACleanup();
	Log("exit");
	Log_end();
	return 0;
}
