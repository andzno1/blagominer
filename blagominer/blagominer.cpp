// blagominer.cpp
#include "stdafx.h"
#include "blagominer.h"

bool exitHandled = false;

// Initialize static member data
std::map<size_t, std::thread> worker;	        // worker threads

std::thread proxyBurst;
std::thread proxyBhd;
std::thread updateChecker;
std::thread proxyOnlyBurst;
std::thread proxyOnlyBhd;
std::thread updaterBurst;
std::thread updaterBhd;

const InstructionSet::InstructionSet_Internal InstructionSet::CPU_Rep;

std::shared_ptr<t_coin_info> burst = std::make_shared<t_coin_info>();
std::shared_ptr<t_coin_info> bhd = std::make_shared<t_coin_info>();
t_logging loggingConfig = {};

std::vector<std::shared_ptr<t_coin_info>> coins;

char *p_minerPath = nullptr;
char *pass = nullptr;
unsigned long long total_size = 0;
bool POC2 = false;
volatile bool stopThreads = false;
bool use_wakeup = false;						// wakeup HDDs if true
unsigned int hddWakeUpTimer = 180;              // HDD wakeup timer in seconds

bool use_debug = false;

bool proxyOnly = false;

 std::map<size_t, t_worker_progress> worker_progress;

std::vector<std::string> paths_dir; // ïóòè

sph_shabal_context  local_32;

void init_mining_info() {

	burst->mining = std::make_shared<t_mining_info>();
	burst->coin = BURST;
	burst->locks = std::make_shared<t_locks>();
	burst->mining->miner_mode = 0;
	burst->mining->priority = 0;
	burst->mining->state = DONE;
	burst->mining->baseTarget = 0;
	burst->mining->height = 0;
	burst->mining->deadline = 0;
	burst->mining->scoop = 0;
	burst->mining->enable = true;
	burst->mining->my_target_deadline = MAXDWORD; // 4294967295;
	burst->mining->POC2StartBlock = 502000;
	burst->mining->dirs = std::vector<std::shared_ptr<t_directory_info>>();

	bhd->mining = std::make_shared<t_mining_info>();
	bhd->coin = BHD;
	bhd->locks = std::make_shared<t_locks>();
	bhd->mining->miner_mode = 1;
	bhd->mining->priority = 1;
	bhd->mining->state = DONE;
	bhd->mining->baseTarget = 0;
	bhd->mining->height = 0;
	bhd->mining->deadline = 0;
	bhd->mining->scoop = 0;
	bhd->mining->enable = false;
	bhd->mining->my_target_deadline = MAXDWORD; // 4294967295;
	bhd->mining->POC2StartBlock = 0;
	bhd->mining->dirs = std::vector<std::shared_ptr<t_directory_info>>();
}

void init_logging_config() {
	loggingConfig.enableLogging = true;
	loggingConfig.enableCsv = true;
	loggingConfig.logAllGetMiningInfos = false;
}

void resetDirs(std::shared_ptr<t_coin_info> coinInfo) {
	Log(L"Resetting directories for %s.", coinNames[coinInfo->coin]);
	for (auto& directory : coinInfo->mining->dirs) {
		directory->done = false;
		for (auto& file : directory->files) {
			file.done = false;
		}
	}
}

bool allDirsDone(std::shared_ptr<t_coin_info> coinInfo) {
	for (auto& directory : coinInfo->mining->dirs) {
		if (!directory->done) {
			return false;
		}
	}
	return true;
}

int load_config(char const *const filename)
{
	FILE * pFile;

	fopen_s(&pFile, filename, "rt");

	if (pFile == nullptr)
	{
		fprintf(stderr, "\nError. config file %s not found\n", filename);
		system("pause > nul");
		exit(-1);
	}

	_fseeki64(pFile, 0, SEEK_END);
	__int64 const size = _ftelli64(pFile);
	_fseeki64(pFile, 0, SEEK_SET);
	char *json_ = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, size + 1);
	if (json_ == nullptr)
	{
		fprintf(stderr, "\nError allocating memory\n");
		system("pause > nul");
		exit(-1);
	}
	fread_s(json_, size, 1, size - 1, pFile);
	fclose(pFile);

	Document document;	// Default template parameter uses UTF8 and MemoryPoolAllocator.
	if (document.Parse<kParseCommentsFlag>(json_).HasParseError()) {
		fprintf(stderr, "\nJSON format error (offset %u) check miner.conf\n%s\n", (unsigned)document.GetErrorOffset(), GetParseError_En(document.GetParseError())); //(offset %s  %s", (unsigned)document.GetErrorOffset(), (char*)document.GetParseError());
		system("pause > nul");
		exit(-1);
	}

	if (document.IsObject())
	{	// Document is a JSON value represents the root of DOM. Root can be either an object or array.

		if (document.HasMember("Logging") && document["Logging"].IsObject())
		{
			Log(L"### Loading configuration for Logging ###");

			const Value& logging = document["Logging"];

			if (logging.HasMember("logAllGetMiningInfos") && (logging["logAllGetMiningInfos"].IsBool()))	loggingConfig.logAllGetMiningInfos = logging["logAllGetMiningInfos"].GetBool();
			
			if (logging.HasMember("UseLog") && (logging["UseLog"].IsBool()))	loggingConfig.enableLogging = logging["UseLog"].GetBool();
			
			if (logging.HasMember("EnableCsv") && (logging["EnableCsv"].IsBool()))	loggingConfig.enableCsv = logging["EnableCsv"].GetBool();
		}
		Log(L"logAllGetMiningInfos: %d", loggingConfig.logAllGetMiningInfos);
		Log(L"UseLog: %d", loggingConfig.enableLogging);
		Log(L"EnableCsv: %d", loggingConfig.enableCsv);

		Log_init();

		if (document.HasMember("Paths") && document["Paths"].IsArray()) {
			const Value& Paths = document["Paths"];			// Using a reference for consecutive access is handy and faster.
			for (SizeType i = 0; i < Paths.Size(); i++)
			{
				paths_dir.push_back(Paths[i].GetString());
				Log(L"Path: %S", paths_dir[i].c_str());
			}
		}

		if (document.HasMember("Burst") && document["Burst"].IsObject())
		{
			
			Log(L"### Loading configuration for Burstcoin ###");
			
			const Value& settingsBurst = document["Burst"];

			if (settingsBurst.HasMember("EnableMining") && (settingsBurst["EnableMining"].IsBool()))	burst->mining->enable = settingsBurst["EnableMining"].GetBool();
			Log(L"EnableMining: %d", burst->mining->enable);

			if (settingsBurst.HasMember("EnableProxy") && (settingsBurst["EnableProxy"].IsBool())) burst->network->enable_proxy = settingsBurst["EnableProxy"].GetBool();
			Log(L"EnableProxy: %d", burst->network->enable_proxy);

			if (burst->mining->enable) {
				coins.push_back(burst);
				burst->mining->dirs = {};
				for (auto &directory : paths_dir) {
					burst->mining->dirs.push_back(std::make_shared<t_directory_info>(directory, false, std::vector<t_files>()));
				}

				if (settingsBurst.HasMember("Priority") && (settingsBurst["Priority"].IsUint())) {
					burst->mining->priority = settingsBurst["Priority"].GetUint();
				}
				Log(L"Priority %zu", burst->mining->priority);
			}

			if (burst->network->enable_proxy) {
				if (settingsBurst.HasMember("ProxyPort"))
				{
					if (settingsBurst["ProxyPort"].IsString())	burst->network->proxyport = settingsBurst["ProxyPort"].GetString();
					else if (settingsBurst["ProxyPort"].IsUint())	burst->network->proxyport = std::to_string(settingsBurst["ProxyPort"].GetUint());
				}
				Log(L"ProxyPort: %S", burst->network->proxyport.c_str());

				if (settingsBurst.HasMember("ProxyUpdateInterval") && (settingsBurst["ProxyUpdateInterval"].IsUint())) {
					burst->network->proxy_update_interval = (size_t)settingsBurst["ProxyUpdateInterval"].GetUint();
				}
				Log(L"ProxyUpdateInterval: %zu", burst->network->proxy_update_interval);

			}

			if (burst->mining->enable || burst->network->enable_proxy) {

				if (settingsBurst.HasMember("Mode") && settingsBurst["Mode"].IsString())
				{
					if (strcmp(settingsBurst["Mode"].GetString(), "solo") == 0) burst->mining->miner_mode = 0;
					else burst->mining->miner_mode = 1;
					Log(L"Mode: %zu", burst->mining->miner_mode);
				}

				if (settingsBurst.HasMember("Server") && settingsBurst["Server"].IsString())	burst->network->nodeaddr = settingsBurst["Server"].GetString();//strcpy_s(nodeaddr, document["Server"].GetString());
				Log(L"Server: %S", burst->network->nodeaddr.c_str());

				if (settingsBurst.HasMember("Port"))
				{
					if (settingsBurst["Port"].IsString())	burst->network->nodeport = settingsBurst["Port"].GetString();
					else if (settingsBurst["Port"].IsUint())	burst->network->nodeport = std::to_string(settingsBurst["Port"].GetUint()); //_itoa_s(document["Port"].GetUint(), nodeport, INET_ADDRSTRLEN-1, 10);
					Log(L"Port %S", burst->network->nodeport.c_str());
				}

				if (settingsBurst.HasMember("SubmitTimeout") && (settingsBurst["SubmitTimeout"].IsUint())) {
					burst->network->submitTimeout = (size_t)settingsBurst["SubmitTimeout"].GetUint();
				}
				Log(L"SubmitTimeout: %zu", burst->network->submitTimeout);

				if (settingsBurst.HasMember("UpdaterAddr") && settingsBurst["UpdaterAddr"].IsString()) burst->network->updateraddr = settingsBurst["UpdaterAddr"].GetString(); //strcpy_s(updateraddr, document["UpdaterAddr"].GetString());
				Log(L"Updater address: %S", burst->network->updateraddr.c_str());

				if (settingsBurst.HasMember("UpdaterPort"))
				{
					if (settingsBurst["UpdaterPort"].IsString())	burst->network->updaterport = settingsBurst["UpdaterPort"].GetString();
					else if (settingsBurst["UpdaterPort"].IsUint())	 burst->network->updaterport = std::to_string(settingsBurst["UpdaterPort"].GetUint());
				}
				Log(L"Updater port: %S", burst->network->updaterport.c_str());

				if (settingsBurst.HasMember("SendInterval") && (settingsBurst["SendInterval"].IsUint())) burst->network->send_interval = (size_t)settingsBurst["SendInterval"].GetUint();
				Log(L"SendInterval: %zu", burst->network->send_interval);

				if (settingsBurst.HasMember("UpdateInterval") && (settingsBurst["UpdateInterval"].IsUint())) burst->network->update_interval = (size_t)settingsBurst["UpdateInterval"].GetUint();
				Log(L"UpdateInterval: %zu", burst->network->update_interval);

				if (settingsBurst.HasMember("TargetDeadline") && (settingsBurst["TargetDeadline"].IsInt64()))	burst->mining->my_target_deadline = settingsBurst["TargetDeadline"].GetUint64();
				Log(L"TargetDeadline: %llu", burst->mining->my_target_deadline);
				if (burst->mining->my_target_deadline == 0) {
					Log(L"TargetDeadline has to be >0.");
					fprintf(stderr, "TargetDeadline should be greater than 0. Please check your configuration file.");
					system("pause > nul");
					exit(-1);
				}

				if (settingsBurst.HasMember("POC2StartBlock") && (settingsBurst["POC2StartBlock"].IsUint64())) burst->mining->POC2StartBlock = settingsBurst["POC2StartBlock"].GetUint64();
				Log(L"POC2StartBlock: %llu", burst->mining->POC2StartBlock);
			}
		}


		if (document.HasMember("BHD") && document["BHD"].IsObject())
		{
			Log(L"### Loading configuration for Bitcoin HD ###");
			
			const Value& settingsBhd = document["BHD"];

			if (settingsBhd.HasMember("EnableMining") && (settingsBhd["EnableMining"].IsBool()))	bhd->mining->enable = settingsBhd["EnableMining"].GetBool();
			Log(L"EnableMining: %d", bhd->mining->enable);

			if (settingsBhd.HasMember("EnableProxy") && (settingsBhd["EnableProxy"].IsBool())) bhd->network->enable_proxy = settingsBhd["EnableProxy"].GetBool();
			Log(L"EnableProxy: %d", bhd->network->enable_proxy);

			if (bhd->mining->enable) {
				bhd->mining->dirs = {};
				for (auto directory : paths_dir) {
					bhd->mining->dirs.push_back(std::make_shared<t_directory_info>(directory, false, std::vector<t_files>()));
				}

				if (settingsBhd.HasMember("Priority") && (settingsBhd["Priority"].IsUint())) {
					bhd->mining->priority = settingsBhd["Priority"].GetUint();
				}
				Log(L"Priority: %zu", bhd->mining->priority);

				if ((burst->mining->enable) && bhd->mining->priority >= burst->mining->priority) {
					coins.push_back(bhd);
				}
				else {
					coins.insert(coins.begin(), bhd);
				}
			}

			if (bhd->network->enable_proxy) {
				if (settingsBhd.HasMember("ProxyPort"))
				{
					if (settingsBhd["ProxyPort"].IsString())	bhd->network->proxyport = settingsBhd["ProxyPort"].GetString();
					else if (settingsBhd["ProxyPort"].IsUint())	bhd->network->proxyport = std::to_string(settingsBhd["ProxyPort"].GetUint());
				}
				Log(L"ProxyPort: %S", bhd->network->proxyport.c_str());

				if (settingsBhd.HasMember("ProxyUpdateInterval") && (settingsBhd["ProxyUpdateInterval"].IsUint())) {
					bhd->network->proxy_update_interval = (size_t)settingsBhd["ProxyUpdateInterval"].GetUint();
				}
				Log(L"ProxyUpdateInterval: %zu", bhd->network->proxy_update_interval);
			}

			if (bhd->mining->enable || bhd->network->enable_proxy) {
				if (settingsBhd.HasMember("Server") && settingsBhd["Server"].IsString())	bhd->network->nodeaddr = settingsBhd["Server"].GetString();//strcpy_s(nodeaddr, document["Server"].GetString());
				Log(L"Server: %S", bhd->network->nodeaddr.c_str());

				if (settingsBhd.HasMember("Port"))
				{
					if (settingsBhd["Port"].IsString())	bhd->network->nodeport = settingsBhd["Port"].GetString();
					else if (settingsBhd["Port"].IsUint())	bhd->network->nodeport = std::to_string(settingsBhd["Port"].GetUint()); //_itoa_s(document["Port"].GetUint(), nodeport, INET_ADDRSTRLEN-1, 10);
					Log(L"Port: %S", bhd->network->nodeport.c_str());
				}

				if (settingsBhd.HasMember("SubmitTimeout") && (settingsBhd["SubmitTimeout"].IsUint())) {
					bhd->network->submitTimeout = (size_t)settingsBhd["SubmitTimeout"].GetUint();
				}
				Log(L"SubmitTimeout: %zu", bhd->network->submitTimeout);

				if (settingsBhd.HasMember("UpdaterAddr") && settingsBhd["UpdaterAddr"].IsString()) bhd->network->updateraddr = settingsBhd["UpdaterAddr"].GetString(); //strcpy_s(updateraddr, document["UpdaterAddr"].GetString());
				Log(L"Updater address: %S", bhd->network->updateraddr.c_str());

				if (settingsBhd.HasMember("UpdaterPort"))
				{
					if (settingsBhd["UpdaterPort"].IsString())	bhd->network->updaterport = settingsBhd["UpdaterPort"].GetString();
					else if (settingsBhd["UpdaterPort"].IsUint())	 bhd->network->updaterport = std::to_string(settingsBhd["UpdaterPort"].GetUint());
				}
				Log(L"Updater port: %S", bhd->network->updaterport.c_str());

				if (settingsBhd.HasMember("SendInterval") && (settingsBhd["SendInterval"].IsUint())) bhd->network->send_interval = (size_t)settingsBhd["SendInterval"].GetUint();
				Log(L"SendInterval: %zu", bhd->network->send_interval);

				if (settingsBhd.HasMember("UpdateInterval") && (settingsBhd["UpdateInterval"].IsUint())) bhd->network->update_interval = (size_t)settingsBhd["UpdateInterval"].GetUint();
				Log(L"UpdateInterval: %zu", bhd->network->update_interval);

				if (settingsBhd.HasMember("TargetDeadline") && (settingsBhd["TargetDeadline"].IsInt64()))	bhd->mining->my_target_deadline = settingsBhd["TargetDeadline"].GetUint64();
				Log(L"TargetDeadline: %llu", bhd->mining->my_target_deadline);
				if (bhd->mining->my_target_deadline == 0) {
					Log(L"TargetDeadline has to be >0.");
					fprintf(stderr, "TargetDeadline should be greater than 0. Please check your configuration file.");
					system("pause > nul");
					exit(-1);
				}
			}
		}
				
		Log(L"### Loading general configuration ###");

		if (document.HasMember("CacheSize") && (document["CacheSize"].IsUint64())) {
			cache_size1 = document["CacheSize"].GetUint64();
			readChunkSize = cache_size1; //sensible default
		}
		Log(L"CacheSize: %zu",  cache_size1);

		if (document.HasMember("CacheSize2") && (document["CacheSize2"].IsUint64())) cache_size2 = document["CacheSize2"].GetUint64();
		Log(L"CacheSize2: %zu", cache_size2);

		if (document.HasMember("ReadChunkSize") && (document["ReadChunkSize"].IsUint64())) readChunkSize = document["ReadChunkSize"].GetUint64();
		Log(L"ReadChunkSize: %zu", readChunkSize);

		if (document.HasMember("UseHDDWakeUp") && (document["UseHDDWakeUp"].IsBool())) use_wakeup = document["UseHDDWakeUp"].GetBool();
		Log(L"UseHDDWakeUp: %d", use_wakeup);

		if (document.HasMember("ShowCorruptedPlotFiles") && (document["ShowCorruptedPlotFiles"].IsBool())) showCorruptedPlotFiles = document["ShowCorruptedPlotFiles"].GetBool();
		Log(L"ShowCorruptedPlotFiles: %d", showCorruptedPlotFiles);

		if (document.HasMember("IgnoreSuspectedFastBlocks") && (document["IgnoreSuspectedFastBlocks"].IsBool()))
			ignoreSuspectedFastBlocks = document["IgnoreSuspectedFastBlocks"].GetBool();
		Log(L"IgnoreSuspectedFastBlocks: %d", ignoreSuspectedFastBlocks);

		if (document.HasMember("hddWakeUpTimer") && (document["hddWakeUpTimer"].IsUint())) hddWakeUpTimer = document["hddWakeUpTimer"].GetUint();
		Log(L"hddWakeUpTimer: %u", hddWakeUpTimer);

		if (document.HasMember("bfsTOCOffset") && (document["bfsTOCOffset"].IsUint())) bfsTOCOffset = document["bfsTOCOffset"].GetUint();
		Log(L"bfsTOCOffset: %u", bfsTOCOffset);

		if (document.HasMember("Debug") && (document["Debug"].IsBool()))	use_debug = document["Debug"].GetBool();
		Log(L"Debug: %d", use_debug);

		if (document.HasMember("CheckForUpdateIntervalInDays") && (document["CheckForUpdateIntervalInDays"].IsDouble())) {
			checkForUpdateInterval = document["CheckForUpdateIntervalInDays"].GetDouble();
		}
		else if (document.HasMember("CheckForUpdateIntervalInDays") && (document["CheckForUpdateIntervalInDays"].IsInt64())) {
			checkForUpdateInterval = (double) document["CheckForUpdateIntervalInDays"].GetInt64();
		}

		if (checkForUpdateInterval < 0) {
			checkForUpdateInterval = 0;
		}
		Log(L"CheckForUpdateIntervalInDays: %f", checkForUpdateInterval);
				
		if (document.HasMember("UseBoost") && (document["UseBoost"].IsBool())) use_boost = document["UseBoost"].GetBool();
		Log(L"UseBoost: %d", use_boost);

		if (document.HasMember("WinSizeX") && (document["WinSizeX"].IsUint())) win_size_x = (short)document["WinSizeX"].GetUint();
		if (win_size_x < 96) win_size_x = 96;
		Log(L"WinSizeX: %hi", win_size_x);

		if (document.HasMember("WinSizeY") && (document["WinSizeY"].IsUint())) win_size_y = (short)document["WinSizeY"].GetUint();
		if (win_size_y < 20) win_size_y = 20;
		Log(L"WinSizeY: %hi", win_size_y);

		if (document.HasMember("LockWindowSize") && (document["LockWindowSize"].IsBool())) lockWindowSize = document["LockWindowSize"].GetBool();
		Log(L"LockWindowSize: %d", lockWindowSize);

#ifdef GPU_ON_C
		if (document.HasMember("GPU_Platform") && (document["GPU_Platform"].IsInt())) gpu_devices.use_gpu_platform = (size_t)document["GPU_Platform"].GetUint();
		Log(L"GPU_Platform: %u", gpu_devices.use_gpu_platform);

		if (document.HasMember("GPU_Device") && (document["GPU_Device"].IsInt())) gpu_devices.use_gpu_device = (size_t)document["GPU_Device"].GetUint();
		Log(L"GPU_Device: %u", gpu_devices.use_gpu_device);
#endif	

	}

	Log(L"=== Config loaded ===");
	if (json_ != nullptr) {
		HeapFree(hHeap, 0, json_);
	}
	return 1;
}


void GetCPUInfo(void)
{
	ULONGLONG  TotalMemoryInKilobytes = 0;

	printToConsole(-1, false, false, false, false, L"CPU support: ");
	if (InstructionSet::AES())   printToConsole(-1, false, false, false, false, L" AES ");
	if (InstructionSet::SSE())   printToConsole(-1, false, false, false, false, L" SSE ");
	if (InstructionSet::SSE2())  printToConsole(-1, false, false, false, false, L" SSE2 ");
	if (InstructionSet::SSE3())  printToConsole(-1, false, false, false, false, L" SSE3 ");
	if (InstructionSet::SSE42()) printToConsole(-1, false, false, false, false, L" SSE4.2 ");
	if (InstructionSet::AVX())   printToConsole(-1, false, false, false, false, L" AVX ");
	if (InstructionSet::AVX2())  printToConsole(-1, false, false, false, false, L" AVX2 ");
	if (InstructionSet::AVX512F())  printToConsole(-1, false, false, false, false, L" AVX512F ");

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
	if (avxSupported)	printToConsole(-1, false, false, false, false, L"     [recomend use AVX]", 0);
#endif
	if (InstructionSet::AVX2()) printToConsole(-1, false, false, false, false, L"     [recomend use AVX2]", 0);
	if (InstructionSet::AVX512F()) printToConsole(-1, false, false, false, false, L"     [recomend use AVX512F]", 0);
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	printToConsole(-1, false, true, false, false, L"%S", InstructionSet::Vendor().c_str());
	printToConsole(-1, false, false, false, false, L" %S [%u cores]", InstructionSet::Brand().c_str(), sysinfo.dwNumberOfProcessors);

	if (GetPhysicallyInstalledSystemMemory(&TotalMemoryInKilobytes))
		printToConsole(-1, false, true, false, false, L"RAM: %llu Mb", (unsigned long long)TotalMemoryInKilobytes / 1024, 0);

	printToConsole(-1, false, false, true, false, L"");
}


void GetPass(char const *const p_strFolderPath)
{
	Log(L"Reading passphrase.");
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
		printToConsole(12, false, false, true, false, L"Error: passphrases.txt not found. File is needed for solo mining.");
		system("pause > nul");
		exit(-1);
	}
	if (filename != nullptr) {
		HeapFree(hHeap, 0, filename);
	}
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
	if (buffer != nullptr) {
		HeapFree(hHeap, 0, buffer);
	}
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

unsigned int calcScoop(std::shared_ptr<t_coin_info> coin) {
	Log(L"Calculating scoop for %s", coinNames[coin->coin]);
	char scoopgen[40];
	memmove(scoopgen, coin->mining->signature, 32);
	const char *mov = (char*)&coin->mining->currentHeight;
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

void updateCurrentMiningInfo(std::shared_ptr<t_coin_info> coin) {
	const wchar_t* coinName = coinNames[coin->coin];
	Log(L"Updating mining information for %s.", coinName);
	char* sig = getSignature(coin);
	memmove(coin->mining->currentSignature, sig, 32);
	delete[] sig;

	updateCurrentStrSignature(coin);
	coin->mining->currentHeight = coin->mining->height;
	coin->mining->currentBaseTarget = coin->mining->baseTarget;

	if (coin->mining->enable) {
		coin->mining->scoop = calcScoop(coin);
	}
}

void insertIntoQueue(std::vector<std::shared_ptr<t_coin_info>>& currentQueue, std::shared_ptr<t_coin_info> newCoin,
	std::shared_ptr<t_coin_info > coinCurrentlyMining) {
	bool inserted = false;
	for (auto it = currentQueue.begin(); it != currentQueue.end(); ++it) {
		if (newCoin->mining->priority < (*it)->mining->priority) {
			Log(L"Adding %s to the queue before %s.", coinNames[newCoin->coin], coinNames[(*it)->coin]);
			currentQueue.insert(it, newCoin);
			inserted = true;
			break;
		}
		if (newCoin == (*it)) {
			Log(L"Coin %s already in queue. No action needed", coinNames[newCoin->coin]);
			inserted = true;
			if (coinCurrentlyMining && coinCurrentlyMining->mining->state == MINING) {
				printToConsole(5, true, false, false, true, L"[#%s|%s|Info    ] New block has been added to the queue.",
					toWStr(newCoin->mining->height, 7).c_str(), toWStr(coinNames[newCoin->coin], 10).c_str(), 0);
			}
			break;
		}
	}
	if (!inserted) {
		Log(L"Adding %s to the end of the queue.", coinNames[newCoin->coin]);
		if (coinCurrentlyMining && coinCurrentlyMining->mining->state == MINING &&
			newCoin->coin != coinCurrentlyMining->coin &&
			newCoin->mining->priority >= coinCurrentlyMining->mining->priority) {
			printToConsole(5, true, false, false, true, L"[#%s|%s|Info    ] New block has been added to the end of the queue.",
				toWStr(newCoin->mining->height, 7).c_str(), toWStr(coinNames[newCoin->coin], 10).c_str(), 0);
		}
		currentQueue.push_back(newCoin);
	}
	else {
		Log(L"insertIntoQueue: did nothing.");
	}
}


void newRound(std::shared_ptr<t_coin_info > coinCurrentlyMining) {
	const wchar_t* coinName = coinNames[coinCurrentlyMining->coin];
	Log(L"New round for %s.", coinName);
	EnterCriticalSection(&coinCurrentlyMining->locks->sessionsLock);
	for (auto it = coinCurrentlyMining->network->sessions.begin(); it != coinCurrentlyMining->network->sessions.end(); ++it) {
		closesocket((*it)->Socket);
	}
	coinCurrentlyMining->network->sessions.clear();
	LeaveCriticalSection(&coinCurrentlyMining->locks->sessionsLock);

	EnterCriticalSection(&coinCurrentlyMining->locks->sharesLock);
	coinCurrentlyMining->mining->shares.clear();
	LeaveCriticalSection(&coinCurrentlyMining->locks->sharesLock);

	EnterCriticalSection(&coinCurrentlyMining->locks->bestsLock);
	coinCurrentlyMining->mining->bests.clear();
	LeaveCriticalSection(&coinCurrentlyMining->locks->bestsLock);

	updateCurrentMiningInfo(coinCurrentlyMining);
	coinCurrentlyMining->mining->deadline = 0;

	if (coinCurrentlyMining->mining->enable && coinCurrentlyMining->mining->state == QUEUED) {
		resetDirs(coinCurrentlyMining);
	}

	// We need only one instance of sender
	if (!coinCurrentlyMining->network->sender.joinable()) {
		coinCurrentlyMining->network->sender = std::thread(send_i, coinCurrentlyMining);
	}
	// We need only one instance of confirmer
	if (!coinCurrentlyMining->network->confirmer.joinable()) {
		coinCurrentlyMining->network->confirmer = std::thread(confirm_i, coinCurrentlyMining);
	}
}

void handleProxyOnly(std::shared_ptr<t_coin_info> coin) {
	Log(L"Starting proxy only handler for %s.", coinNames[coin->coin]);
	while (!exit_flag) {
		if (signaturesDiffer(coin)) {
			Log(L"Signature for %s changed.", coinNames[coin->coin]);
			Log(L"Won't add %s to the queue. Proxy only.", coinNames[coin->coin]);
			updateOldSignature(coin);
			printToConsole(5, true, true, false, true, L"[#%s|%s|Info    ] New block.",
				toWStr(coin->mining->height, 7).c_str(), toWStr(coinNames[coin->coin], 10).c_str(), 0);
			
			if (coin->mining->currentBaseTarget != 0) {
				std::thread{ Csv_Submitted,  coin->coin, coin->mining->currentHeight,
					coin->mining->currentBaseTarget, 4398046511104 / 240 / coin->mining->currentBaseTarget,
					0, true, coin->mining->deadline }.detach();
			}
			newRound(coin);
		}
		else {
			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}
}

bool getNewMiningInfo(const std::vector<std::shared_ptr<t_coin_info>>& allCoins,
	std::shared_ptr<t_coin_info > coinCurrentlyMining,
	std::vector<std::shared_ptr<t_coin_info>>& currentQueue) {
	bool newInfoAvailable = false;
	for (auto& pt : allCoins) {
		if (signaturesDiffer(pt)) {
			Log(L"Signature for %s changed.", coinNames[pt->coin]);
			updateOldSignature(pt);
			if (pt->mining->enable) {
				// Setting interrupted to false in case the coin with changed signature has been
				// scheduled for continuing.
				pt->mining->state = QUEUED;
				Log(L"Inserting %s into queue.", coinNames[pt->coin]);
				insertIntoQueue(currentQueue, pt, coinCurrentlyMining);
				newInfoAvailable = true;
			}
		}
	}
	return newInfoAvailable;
}

bool needToInterruptMining(const std::vector<std::shared_ptr<t_coin_info>>& allCoins,
	std::shared_ptr<t_coin_info > coinCurrentlyMining,
	std::vector<std::shared_ptr<t_coin_info>>& currentQueue) {
	if (getNewMiningInfo(allCoins, coinCurrentlyMining, currentQueue)) {
		if (currentQueue.empty()) {
			Log(L"CRITICAL ERROR: Current queue is empty. This should not happen.");
			return false;
		}
		// Checking only the first element, since it has already the highest priority (but lowest value).
		if (currentQueue.front()->mining->enable) {
			if (currentQueue.front()->mining->priority < coinCurrentlyMining->mining->priority) {
				if (coinCurrentlyMining->mining->state == MINING) {
					Log(L"Interrupting current mining progress. %s has a higher priority than %s.",
						coinNames[currentQueue.front()->coin], coinNames[coinCurrentlyMining->coin]);
				}
				return true;
			}
			else {
				for (auto& pt : currentQueue) {
					if (pt->coin == coinCurrentlyMining->coin) {
						Log(L"Interrupting current mining progress. New %s block.", coinNames[coinCurrentlyMining->coin]);
						return true;
					}
				}
			}
		}
	}
	return false;
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
			printToConsole(-1, false, false, true, false, L"%S\tfiles: %4u\t size: %7llu GiB",
				(char*)iter->c_str(), (unsigned)files.size(), tot_size / 1024 / 1024 / 1024, 0);
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

void handleReturn(BOOL success) {
	if (!success) {
		Log(L"FAILED with error %lu", GetLastError());
	}
}

static void resizeConsole(SHORT newColumns, SHORT newRows) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi; // Hold Current Console Buffer Info 
	BOOL bSuccess;
	SMALL_RECT newWindowRect;         // Hold the New Console Size 
	COORD currentWindowSize;
	
	Log(L"GetConsoleScreenBufferInfo");
	bSuccess = GetConsoleScreenBufferInfo(hConsole, &csbi);
	handleReturn(bSuccess);
	currentWindowSize.X = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	currentWindowSize.Y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	
	Log(L"Current buffer size csbi.dwSize X: %hi, Y: %hi", csbi.dwSize.X, csbi.dwSize.Y);
	Log(L"csbi.dwMaximumWindowSize X: %hi, Y: %hi", csbi.dwMaximumWindowSize.X, csbi.dwMaximumWindowSize.Y);
	Log(L"currentWindowSize X: %hi, Y: %hi", currentWindowSize.X, currentWindowSize.Y);

	// Get the Largest Size we can size the Console Window to 
	COORD largestWindowSize = GetLargestConsoleWindowSize(hConsole);
	Log(L"largestWindowSize X: %hi, Y: %hi", largestWindowSize.X, largestWindowSize.Y);

	// Define the New Console Window Size and Scroll Position 
	newWindowRect.Right = min(newColumns, largestWindowSize.X) - 1;
	newWindowRect.Bottom = min(newRows, largestWindowSize.Y) - 1;
	newWindowRect.Left = newWindowRect.Top = (SHORT)0;

	Log(L"newWindowRect b: %hi, l: %hi, r: %hi, t: %hi", newWindowRect.Bottom, newWindowRect.Left, newWindowRect.Right, newWindowRect.Top);

	// Define the New Console Buffer Size
	COORD newBufferSize;
	newBufferSize.X = min(newColumns, largestWindowSize.X);
	newBufferSize.Y = min(newRows, largestWindowSize.Y);

	Log(L"Resizing buffer (x: %hi, y: %hi).", newBufferSize.X, newBufferSize.Y);
	Log(L"Resizing window (x: %hi, y: %hi).", newWindowRect.Right - newWindowRect.Left, newWindowRect.Bottom - newWindowRect.Top);


	/*
		Information from https://docs.microsoft.com/en-us/windows/console/window-and-screen-buffer-size

		To change a screen buffer's size, use the SetConsoleScreenBufferSize function. This function
		fails if either dimension of the specified size is less than the corresponding dimension of the
		console's window.

		To change the size or location of a screen buffer's window, use the SetConsoleWindowInfo function.
		This function fails if the specified window-corner coordinates exceed the limits of the console
		screen buffer or the screen. Changing the window size of the active screen buffer changes the
		size of the console window displayed on the screen.	
	
	*/
	while (true) {
		if (currentWindowSize.X > newBufferSize.X || currentWindowSize.Y > newBufferSize.Y) {
			Log(L"Current window size is larger than the new buffer size. Resizing window first.");
			Log(L"SetConsoleWindowInfo srWindowRect b: %hi, l: %hi, r: %hi, t: %hi", newWindowRect.Bottom, newWindowRect.Left, newWindowRect.Right, newWindowRect.Top);
			bSuccess = SetConsoleWindowInfo(hConsole, TRUE, &newWindowRect);
			handleReturn(bSuccess);
			//std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
			Log(L"SetConsoleScreenBufferSize coordScreen X: %hi, Y: %hi", newBufferSize.X, newBufferSize.Y);
			bSuccess = SetConsoleScreenBufferSize(hConsole, newBufferSize);
			handleReturn(bSuccess);
		}
		else {
			Log(L"SetConsoleScreenBufferSize coordScreen X: %hi, Y: %hi", newBufferSize.X, newBufferSize.Y);
			bSuccess = SetConsoleScreenBufferSize(hConsole, newBufferSize);
			handleReturn(bSuccess);
			//std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
			Log(L"SetConsoleWindowInfo srWindowRect b: %hi, l: %hi, r: %hi, t: %hi", newWindowRect.Bottom, newWindowRect.Left, newWindowRect.Right, newWindowRect.Top);
			bSuccess = SetConsoleWindowInfo(hConsole, TRUE, &newWindowRect);
			handleReturn(bSuccess);
		}

		HWND consoleWindow = GetConsoleWindow();

		// Get the monitor that is displaying the window
		HMONITOR monitor = MonitorFromWindow(consoleWindow, MONITOR_DEFAULTTONEAREST);

		// Get the monitor's offset in virtual-screen coordinates
		MONITORINFO monitorInfo;
		monitorInfo.cbSize = sizeof(MONITORINFO);
		GetMonitorInfoA(monitor, &monitorInfo);

		RECT wSize;
		GetWindowRect(consoleWindow, &wSize);
		Log(L"Window Rect wSize b: %hi, l: %hi, r: %hi, t: %hi", wSize.bottom, wSize.left, wSize.right, wSize.top);
		// Move window to top
		Log(L"MoveWindow X: %ld, Y: %ld, w: %ld, h: %ld", wSize.left, monitorInfo.rcWork.top, wSize.right - wSize.left, wSize.bottom - wSize.top);
		bSuccess = MoveWindow(consoleWindow, wSize.left, monitorInfo.rcWork.top, wSize.right - wSize.left, wSize.bottom - wSize.top, true);
		handleReturn(bSuccess);

		if (lockWindowSize) {
			//Prevent resizing. Source: https://stackoverflow.com/a/47359526
			SetWindowLong(consoleWindow, GWL_STYLE, GetWindowLong(consoleWindow, GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);
		}

		Log(L"GetConsoleScreenBufferInfo");
		bSuccess = GetConsoleScreenBufferInfo(hConsole, &csbi);
		handleReturn(bSuccess);
		currentWindowSize.X = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		currentWindowSize.Y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
		Log(L"New buffer size csbi.dwSize X: %hi, Y: %hi", csbi.dwSize.X, csbi.dwSize.Y);
		Log(L"New window size X: %hi, Y: %hi", currentWindowSize.X, currentWindowSize.Y);

		Log(L"Hiding scroll bars.");
		bSuccess = ShowScrollBar(consoleWindow, SB_BOTH, FALSE);
		handleReturn(bSuccess);

		if (currentWindowSize.X != newBufferSize.X || currentWindowSize.Y != newBufferSize.Y) {
			Log(L"Failed to resize window. Retrying.");
		}
		else {
			break;
		}

	}

	return;
}

void closeMiner() {
	if (exitHandled) {
		return;
	}
	Log(L"Closing miner.");
	exitHandled = true;
	
	Log(L"Waiting for worker threads to shut down.");
	for (auto it = worker.begin(); it != worker.end(); ++it)
	{
		if (it->second.joinable()) {
			it->second.join();
		}
	}
	Log(L"All worker threads shut down.");
	
	if (updaterBurst.joinable()) updaterBurst.join();
	if (updaterBhd.joinable()) updaterBhd.join();
	if (proxyBurst.joinable()) proxyBurst.join();
	if (proxyBhd.joinable()) proxyBhd.join();
	if (proxyOnlyBurst.joinable()) proxyOnlyBurst.join();
	if (proxyOnlyBhd.joinable()) proxyOnlyBhd.join();
	if (updateChecker.joinable()) updateChecker.join();
	if (burst->network->sender.joinable()) burst->network->sender.join();
	if (bhd->network->sender.joinable()) bhd->network->sender.join();
	
	EnterCriticalSection(&burst->locks->sessionsLock);
	for (auto it = burst->network->sessions.begin(); it != burst->network->sessions.end(); ++it) {
		closesocket((*it)->Socket);
	}
	burst->network->sessions.clear();
	LeaveCriticalSection(&burst->locks->sessionsLock);

	EnterCriticalSection(&bhd->locks->sessionsLock);
	for (auto it = bhd->network->sessions.begin(); it != bhd->network->sessions.end(); ++it) {
		closesocket((*it)->Socket);
	}
	bhd->network->sessions.clear();
	LeaveCriticalSection(&bhd->locks->sessionsLock);
	if (pass != nullptr) HeapFree(hHeap, 0, pass);
		
	if (burst->mining->enable || burst->network->enable_proxy) {
		DeleteCriticalSection(&burst->locks->sessionsLock);
		DeleteCriticalSection(&burst->locks->sharesLock);
		DeleteCriticalSection(&burst->locks->bestsLock);
	}
	if (bhd->mining->enable || bhd->network->enable_proxy) {
		DeleteCriticalSection(&bhd->locks->sessionsLock);
		DeleteCriticalSection(&bhd->locks->sharesLock);
		DeleteCriticalSection(&bhd->locks->bestsLock);
	}
	if (p_minerPath != nullptr) {
		HeapFree(hHeap, 0, p_minerPath);
	}

	WSACleanup();
	Log(L"exit");
	Log_end();
	bm_end();

	worker.~map();
	worker_progress.~map();
	paths_dir.~vector();
	burst->mining->bests.~vector();
	burst->mining->shares.~vector();
	burst->network->sessions.~vector();
	bhd->mining->bests.~vector();
	bhd->mining->shares.~vector();
	bhd->network->sessions.~vector();
}

BOOL WINAPI OnConsoleClose(DWORD dwCtrlType)
{
	if (dwCtrlType == CTRL_CLOSE_EVENT) {
		exit_flag = true;
		closeMiner();
		return TRUE;
	}
	return FALSE;
}


int main(int argc, char **argv) {
	//init
	SetConsoleCtrlHandler(OnConsoleClose, TRUE);
	atexit(closeMiner);
	hHeap = GetProcessHeap();
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

	//initialize time 
	LARGE_INTEGER li;
	__int64 start_threads_time, end_threads_time, curr_time;
	QueryPerformanceFrequency(&li);
	double pcFreq = double(li.QuadPart);

	std::vector<std::thread> generator;

	unsigned long long bytesRead = 0;

	//get application path
	size_t const cwdsz = GetCurrentDirectoryA(0, 0);
	p_minerPath = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, cwdsz + 2);
	if (p_minerPath == nullptr)
	{
		fprintf(stderr, "\nError allocating memory\n");
		system("pause > nul");
		exit(-1);
	}
	GetCurrentDirectoryA(DWORD(cwdsz), LPSTR(p_minerPath));
	strcat_s(p_minerPath, cwdsz + 2, "\\");


	char* conf_filename = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, MAX_PATH);
	if (conf_filename == nullptr)
	{
		fprintf(stderr, "\nError allocating memory\n");
		system("pause > nul");
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

	init_logging_config();

	//load config
	load_config(conf_filename);
	if (conf_filename != nullptr) {
		HeapFree(hHeap, 0, conf_filename);
	}

	if (burst->network->enable_proxy && bhd->network->enable_proxy &&
		burst->network->proxyport == bhd->network->proxyport) {
		fprintf(stderr, "\nError. Ports for multiple proxies should not be the same. Please check your configuration.\n");
		system("pause > nul");
		exit(-1);
	}

	Csv_Init();

	Log(L"Miner path: %S", p_minerPath);

	resizeConsole(win_size_x, win_size_y);
	
	bm_init();
	printToConsole(12, false, false, true, false, L"BURST/BHD miner, %s", version.c_str());
	printToConsole(4, false, false, true, false, L"Programming: dcct (Linux) & Blago (Windows)");
	printToConsole(4, false, false, true, false, L"POC2 mod: Quibus & Johnny (5/2018)");
	printToConsole(4, false, false, true, false, L"Dual mining mod: andz (2/2019)");

	GetCPUInfo();

	if (!burst->mining->enable && !burst->network->enable_proxy && !bhd->mining->enable && !bhd->network->enable_proxy) {
		printToConsole(12, false, false, true, false, L"Mining and proxies are disabled for all coins. Please check your configuration.");
		system("pause > nul");
		exit(-1);
	}

	if ((burst->mining->enable && burst->mining->miner_mode == 0)) GetPass(p_minerPath);

	// адрес и порт сервера
	Log(L"Searching servers...");
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printToConsole(-1, false, false, true, false, L"WSAStartup failed");
		system("pause > nul");
		exit(-1);
	}

	if (burst->mining->enable || burst->network->enable_proxy) {

		InitializeCriticalSection(&burst->locks->sessionsLock);
		InitializeCriticalSection(&burst->locks->bestsLock);
		InitializeCriticalSection(&burst->locks->sharesLock);
		burst->mining->shares.reserve(20);
		burst->mining->bests.reserve(4);
		burst->network->sessions.reserve(20);

		char* updateripBurst = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
		if (updateripBurst == nullptr) ShowMemErrorExit();
		char* nodeipBurst = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
		if (nodeipBurst == nullptr) ShowMemErrorExit();
		
		hostname_to_ip(burst->network->nodeaddr.c_str(), nodeipBurst);
		printToConsole(-1, false, false, true, false, L"BURST pool address    %S (ip %S:%S)",
			burst->network->nodeaddr.c_str(), nodeipBurst, burst->network->nodeport.c_str());

		if (burst->network->updateraddr.length() > 3) hostname_to_ip(burst->network->updateraddr.c_str(), updateripBurst);
		printToConsole(-1, false, false, true, false, L"BURST updater address %S (ip %S:%S)",
			burst->network->updateraddr.c_str(), updateripBurst, burst->network->updaterport.c_str());

		if (updateripBurst != nullptr) {
			HeapFree(hHeap, 0, updateripBurst);
		}
		if (nodeipBurst != nullptr) {
			HeapFree(hHeap, 0, nodeipBurst);
		}

		RtlSecureZeroMemory(burst->mining->oldSignature, 33);
		RtlSecureZeroMemory(burst->mining->signature, 33);
		RtlSecureZeroMemory(burst->mining->currentSignature, 33);
		RtlSecureZeroMemory(burst->mining->str_signature, 65);
		RtlSecureZeroMemory(burst->mining->current_str_signature, 65);
	}

	if (bhd->mining->enable || bhd->network->enable_proxy) {
		InitializeCriticalSection(&bhd->locks->sessionsLock);
		InitializeCriticalSection(&bhd->locks->bestsLock);
		InitializeCriticalSection(&bhd->locks->sharesLock);
		bhd->mining->shares.reserve(20);
		bhd->mining->bests.reserve(4);
		bhd->network->sessions.reserve(20);

		char* updateripBhd = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
		if (updateripBhd == nullptr) ShowMemErrorExit();
		char* nodeipBhd = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 50);
		if (nodeipBhd == nullptr) ShowMemErrorExit();
		
		hostname_to_ip(bhd->network->nodeaddr.c_str(), nodeipBhd);
		printToConsole(-1, false, false, true, false, L"BHD pool address    %S (ip %S:%S)",
			bhd->network->nodeaddr.c_str(), nodeipBhd, bhd->network->nodeport.c_str());

		if (bhd->network->updateraddr.length() > 3) hostname_to_ip(bhd->network->updateraddr.c_str(), updateripBhd);
		printToConsole(-1, false, false, true, false, L"BHD updater address %S (ip %S:%S)",
			bhd->network->updateraddr.c_str(), updateripBhd, bhd->network->updaterport.c_str());
		
		if (updateripBhd != nullptr) {
			HeapFree(hHeap, 0, updateripBhd);
		}
		if (nodeipBhd != nullptr) {
			HeapFree(hHeap, 0, nodeipBhd);
		}

		RtlSecureZeroMemory(bhd->mining->oldSignature, 33);
		RtlSecureZeroMemory(bhd->mining->signature, 33);
		RtlSecureZeroMemory(bhd->mining->currentSignature, 33);
		RtlSecureZeroMemory(bhd->mining->str_signature, 65);
		RtlSecureZeroMemory(bhd->mining->current_str_signature, 65);
	}
	
	// Инфа по файлам
	printToConsole(15, false, false, true, false, L"Using plots:");
	
	std::vector<t_files> all_files;
	total_size = getPlotFilesSize(paths_dir, true, all_files);
	printToConsole(15, false, false, true, false, L"TOTAL: %llu GiB (%llu TiB)",
		total_size / 1024 / 1024 / 1024, total_size / 1024 / 1024 / 1024 / 1024);
	
	if (total_size == 0 && (burst->mining->enable || bhd->mining->enable)) {
		printToConsole(12, false, true, true, false,
			L"Plot files not found...please check the \"PATHS\" parameter in your config file.");
		system("pause > nul");
		exit(-1);
	}
	else if (total_size == 0) {
		printToConsole(12, false, true, true, false, L"\nNo plot files found.");
	}

	// Check overlapped plots
	for (size_t cx = 0; cx < all_files.size(); cx++) {
		for (size_t cy = cx + 1; cy < all_files.size(); cy++) {
			if (all_files[cy].Key == all_files[cx].Key)
				if (all_files[cy].StartNonce >= all_files[cx].StartNonce) {
					if (all_files[cy].StartNonce < all_files[cx].StartNonce + all_files[cx].Nonces) {
						printToConsole(12, false, true, true, false, L"WARNING: %S%S and \n%S%S are overlapped",
							all_files[cx].Path.c_str(), all_files[cx].Name.c_str(), all_files[cy].Path.c_str(), all_files[cy].Name.c_str());
					}
				}
				else
					if (all_files[cy].StartNonce + all_files[cy].Nonces > all_files[cx].StartNonce) {
						printToConsole(12, false, true, true, false, L"WARNING: %S%S and \n%S%S are overlapped",
							all_files[cx].Path.c_str(), all_files[cx].Name.c_str(), all_files[cy].Path.c_str(), all_files[cy].Name.c_str());
					}
		}
	}
	//all_files.~vector();   // ???

	if (burst->network->submitTimeout < 1000) {
		printToConsole(8, false, true, false, true, L"Timeout for Burstcoin deadline submissions is set to %u ms, which is a low value.", burst->network->submitTimeout);
	}
	if (bhd->network->submitTimeout < 1000) {
		printToConsole(8, false, true, false, true, L"Timeout for Bitcoin HD deadline submissions is set to %u ms, which is a low value.", bhd->network->submitTimeout);
	}

	proxyOnly = !burst->mining->enable && !bhd->mining->enable &&
		(burst->network->enable_proxy || bhd->network->enable_proxy);
	if (proxyOnly) {
		Log(L"Running as proxy only.");
		printToConsole(8, false, true, false, true, L"Running as proxy only.");
	}

	// Run Proxy
	if (burst->network->enable_proxy)
	{
		proxyBurst = std::thread(proxy_i, burst);
		printToConsole(25, false, false, false, true, L"Burstcoin proxy thread started");
	}

	if (bhd->network->enable_proxy)
	{
		proxyBhd = std::thread(proxy_i, bhd);
		printToConsole(25, false, false, false, true, L"Bitcoin HD proxy thread started");
	}

	if (checkForUpdateInterval > 0) {
		updateChecker = std::thread(checkForUpdate);
	}

	// Run updater;
	if (burst->mining->enable || burst->network->enable_proxy)
	{
		updaterBurst = std::thread(updater_i, burst);
		Log(L"BURST updater thread started");
	}
	if (bhd->mining->enable || bhd->network->enable_proxy)
	{
		updaterBhd = std::thread(updater_i, bhd);
		Log(L"BHD updater thread started");
	}	

	std::vector<std::shared_ptr<t_coin_info>> queue;

	if (!burst->mining->enable && burst->network->enable_proxy) {
		proxyOnlyBurst = std::thread(handleProxyOnly, burst);
	}
	if (!bhd->mining->enable && bhd->network->enable_proxy) {
		proxyOnlyBhd = std::thread(handleProxyOnly, bhd);
	}

	if (proxyOnly) {
		const std::wstring trailingSpace = L"                                                                         ";
		while (!exit_flag)
		{
			
			switch (bm_wgetchMain())
			{
			case 'q':
				exit_flag = true;
				break;
			}
			
			if (burst->network->enable_proxy && bhd->network->enable_proxy) {
				printToProgress(L"%sConnection: %3i%%|%3i%%",
					trailingSpace.c_str(), getNetworkQuality(burst), getNetworkQuality(bhd));
			}
			else if (burst->network->enable_proxy) {
				printToProgress(L"%sConnection:      %3i%%",
					trailingSpace.c_str(), getNetworkQuality(burst));
			}
			else if (bhd->network->enable_proxy) {
				printToProgress(L"%sConnection:      %3i%%",
					trailingSpace.c_str(), getNetworkQuality(bhd));
			}

			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}
	else {
		Log(L"Update mining info");
		// Waiting for mining information
		bool firstDataAvailable = false;
		while (!firstDataAvailable) {
			for (auto& c : coins) {
				if (c->mining->enable && getHeight(c) != 0) {
					firstDataAvailable = getNewMiningInfo(coins, nullptr, queue);
					break;
				}
			}
			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}


		Log(L"Burst height: %llu", getHeight(burst));
		Log(L"BHD height: %llu", getHeight(bhd));

		// Main loop
		// Create Shabal Contexts
#ifdef __AVX512F__
		simd512_mshabal_init(&global_512, 256);
		//create minimal context
		global_512_fast.out_size = global_512.out_size;
		for (int j = 0; j < 704; j++)
			global_512_fast.state[j] = global_512.state[j];
		global_512_fast.Whigh = global_512.Whigh;
		global_512_fast.Wlow = global_512.Wlow;
#else
#ifdef __AVX2__
		simd256_mshabal_init(&global_256, 256);
		//create minimal context
		global_256_fast.out_size = global_256.out_size;
		for (int j = 0; j < 352; j++)
			global_256_fast.state[j] = global_256.state[j];
		global_256_fast.Whigh = global_256.Whigh;
		global_256_fast.Wlow = global_256.Wlow;

#else
#ifdef __AVX__
		simd128_mshabal_init(&global_128, 256);
		//create minimal context
		global_128_fast.out_size = global_128.out_size;
		for (int j = 0; j < 176; j++)
			global_128_fast.state[j] = global_128.state[j];
		global_128_fast.Whigh = global_128.Whigh;
		global_128_fast.Wlow = global_128.Wlow;

#else
	//disable for non SSE!
		simd128_mshabal_init(&global_128, 256);
		//create minimal context
		global_128_fast.out_size = global_128.out_size;
		for (int j = 0; j < 176; j++)
			global_128_fast.state[j] = global_128.state[j];
		global_128_fast.Whigh = global_128.Whigh;
		global_128_fast.Wlow = global_128.Wlow;

#endif
#endif 
#endif 
		//context for signature calculation
		sph_shabal256_init(&global_32);
		memcpy(&local_32, &global_32, sizeof(global_32));

		std::shared_ptr<t_coin_info> miningCoin;
		while (!exit_flag)
		{
			worker.clear();
			worker_progress.clear();
			stopThreads = false;
			int oldThreadsRunning = -1;
			double thread_time;

			std::wstring out = L"Coin queue: ";
			for (auto& c : queue) {
				out += std::wstring((wchar_t*) coinNames[c->coin]) + L" (" + std::to_wstring(c->mining->height) + L")";
				if (c != queue.back())	out += L", "; else out += L".";
			}
			Log(out.c_str());

			miningCoin = queue.front();
			setnewMiningInfoReceived(miningCoin, false);
			queue.erase(queue.begin());

			newRound(miningCoin);

			if (miningCoin->mining->enable && miningCoin->mining->state == INTERRUPTED) {
				Log(L"------------------------    Continuing %s block: %llu", coinNames[miningCoin->coin], miningCoin->mining->currentHeight);
				printToConsole(5, true, true, false, true, L"[#%s|%s|Continue] Base Target %s %c Net Diff %s TiB %c PoC%i",
					toWStr(miningCoin->mining->currentHeight, 7).c_str(),
					toWStr(coinNames[miningCoin->coin], 10).c_str(),
					toWStr(miningCoin->mining->currentBaseTarget, 7).c_str(), sepChar,
					toWStr(4398046511104 / 240 / miningCoin->mining->currentBaseTarget, 8).c_str(), sepChar,
					POC2 ? 2 : 1);
			}
			else if (miningCoin->mining->enable) {
				Log(L"------------------------    New %s block: %llu", coinNames[miningCoin->coin], miningCoin->mining->currentHeight);
				printToConsole(25, true, true, false, true, L"[#%s|%s|Start   ] Base Target %s %c Net Diff %s TiB %c PoC%i",
					toWStr(miningCoin->mining->currentHeight, 7).c_str(),
					toWStr(coinNames[miningCoin->coin], 10).c_str(),
					toWStr(miningCoin->mining->currentBaseTarget, 7).c_str(), sepChar,
					toWStr(4398046511104 / 240 / miningCoin->mining->currentBaseTarget, 8).c_str(), sepChar,
					POC2 ? 2 : 1);
			}

			QueryPerformanceCounter((LARGE_INTEGER*)&start_threads_time);
			double threads_speed = 0;

			// Run worker threads
			std::vector<std::string> roundDirectories;
			for (size_t i = 0; i < miningCoin->mining->dirs.size(); i++)
			{
				if (miningCoin->mining->dirs.at(i)->done) {
					// This directory has already been processed. Skipping.
					Log(L"Skipping directory %S", miningCoin->mining->dirs.at(i)->dir.c_str());
					continue;
				}
				worker_progress[i] = { i, 0, true };
				worker[i] = std::thread(work_i, miningCoin, miningCoin->mining->dirs.at(i), i);
				roundDirectories.push_back(miningCoin->mining->dirs.at(i)->dir);
			}

			unsigned long long round_size = 0;

			if (miningCoin->mining->state == INTERRUPTED) {
				round_size = getPlotFilesSize(miningCoin->mining->dirs);
			}
			else {
				round_size = getPlotFilesSize(roundDirectories, false);
			}

			miningCoin->mining->state = MINING;
			Log(L"Directories in this round: %zu", roundDirectories.size());
			Log(L"Round size: %llu GB", round_size / 1024 / 1024 / 1024);

			// Wait until signature changed or exit
			while (!exit_flag && (!haveReceivedNewMiningInfo(coins) || !needToInterruptMining(coins, miningCoin, queue)))
			{
				switch (bm_wgetchMain())
				{
				case 'q':
					exit_flag = true;
					break;
				case 'f':
					resetFileStats();
					break;
				}
				bytesRead = 0;

				if (exit_flag) {
					Log(L"Exitting miner.");
					break;
				}

				int threads_running = 0;
				for (auto it = worker_progress.begin(); it != worker_progress.end(); ++it)
				{
					bytesRead += it->second.Reads_bytes;
					threads_running += it->second.isAlive;
				}
				if (threads_running != oldThreadsRunning) {
					Log(L"threads_running: %i", threads_running);
				}
				oldThreadsRunning = threads_running;

				if (threads_running)
				{
					QueryPerformanceCounter((LARGE_INTEGER*)&end_threads_time);
					threads_speed = (double)(bytesRead / (1024 * 1024)) / ((double)(end_threads_time - start_threads_time) / pcFreq);
					thread_time = (double)(end_threads_time - start_threads_time) / pcFreq;
				}
				else {
					/*
					if (can_generate == 1)
					{
					Log(L"\nStart Generator. ");
					for (size_t i = 0; i < std::thread::hardware_concurrency()-1; i++)	generator.push_back(std::thread(generator_i, i));
					can_generate = 2;
					}
					*/
					//Work Done! Cleanup / Prepare
					if (miningCoin->mining->state == MINING) {
						Log(L"Round done.");
						Log(L"Bytes read: %llu", bytesRead);
						miningCoin->mining->state = DONE;
						//Display total round time
						QueryPerformanceCounter((LARGE_INTEGER*)&end_threads_time);
						thread_time = (double)(end_threads_time - start_threads_time) / pcFreq;

						if (miningCoin->mining->enable) {
							Log(L"Total round time: %.1f seconds", thread_time);
							if (use_debug)
							{
								printToConsole(7, true, false, true, false, L"Total round time: %.1f sec", thread_time);
							}
						}
						//prepare
						memcpy(&local_32, &global_32, sizeof(global_32));
					}
					else if (!queue.empty()) {
						Log(L"Next coin in queue.");
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
								printToConsole(7, true, false, true, false, L"HDD, WAKE UP !");
							}
							end_threads_time = curr_time;
						}
					}
				}

				if (miningCoin->mining->enable && round_size > 0) {
					if (burst->mining->enable && bhd->mining->enable) {
						printToProgress(L"%3llu%% %c %11.2f TiB %c %4.0f s %c %6.0f MiB/s %c Deadline: %s %c Connection: %3i%%|%3i%%",
							(bytesRead * 4096 * 100 / round_size), sepChar,
							(((double)bytesRead) / (256 * 1024 * 1024)), sepChar,
							thread_time, sepChar,
							threads_speed, sepChar,
							(miningCoin->mining->deadline == 0) ? L"          -" : toWStr(miningCoin->mining->deadline, 11).c_str(), sepChar,
							getNetworkQuality(burst),
							getNetworkQuality(bhd));
					}
					else {
						printToProgress(L"%3llu%% %c %11.2f TiB %c %4.0f s %c %6.0f MiB/s %c Deadline: %s %c Connection:      %3i%%",
							(bytesRead * 4096 * 100 / round_size), sepChar,
							(((double)bytesRead) / (256 * 1024 * 1024)), sepChar,
							thread_time, sepChar,
							threads_speed, sepChar,
							(miningCoin->mining->deadline == 0) ? L"          -" : toWStr(miningCoin->mining->deadline, 11).c_str(), sepChar,
							getNetworkQuality(miningCoin));
					}
				}
				else {
					if (burst->mining->enable && bhd->mining->enable) {
						printToProgress(L"                                                                         Connection: %3i%%|%3i%%",
							getNetworkQuality(burst), getNetworkQuality(bhd), 0);
					}
					else {
						printToProgress(L"                                                                         Connection:      %3i%%",
							getNetworkQuality(miningCoin), 0);
					}
				}
				
				printFileStats();
				
				std::this_thread::yield();
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}

			Log(L"Interrupting worker threads.");
			stopThreads = true;   // Tell all threads to stop
			
			Log(L"Waiting for worker threads to shut down.");
			for (auto it = worker.begin(); it != worker.end(); ++it)
			{
				if (it->second.joinable()) {
					it->second.join();
				}
			}
			Log(L"All worker threads shut down.");

			if (miningCoin->mining->state == MINING) {
				// Checking if all directories are done for the rare case that workers finished mining
				// in the same loop iteration as they have been interrupted. In that case mining has been
				// finished.
				if (allDirsDone(miningCoin)) {
					Log(L"Round done.");
					Log(L"Bytes read: %llu", bytesRead);
					miningCoin->mining->state = DONE;
					//Display total round time
					QueryPerformanceCounter((LARGE_INTEGER*)&end_threads_time);
					thread_time = (double)(end_threads_time - start_threads_time) / pcFreq;

					if (miningCoin->mining->enable) {
						Log(L"Total round time: %.1f seconds", thread_time);
						if (use_debug)
						{
							printToConsole(7, true, false, true, false, L"Total round time: %.1f sec", thread_time);
						}
					}
					//prepare
					memcpy(&local_32, &global_32, sizeof(global_32));
				}
				else {
					miningCoin->mining->state = INTERRUPTED;
					Log(L"Mining %s has been interrupted by a coin with higher priority.", coinNames[miningCoin->coin]);
					printToConsole(8, true, false, false, true, L"[#%s|%s|Info    ] Mining has been interrupted by another coin.",
						toWStr(miningCoin->mining->currentHeight, 7).c_str(), toWStr(coinNames[miningCoin->coin], 10).c_str());
					// Queuing the interrupted coin.
					insertIntoQueue(queue, miningCoin, miningCoin);
				}
			}
			else if (!exit_flag) {
				Log(L"New block, no mining has been interrupted.");
			}

			std::thread{ Csv_Submitted,  miningCoin->coin, miningCoin->mining->currentHeight, miningCoin->mining->currentBaseTarget, 4398046511104 / 240 / miningCoin->mining->currentBaseTarget, thread_time, miningCoin->mining->state == DONE, miningCoin->mining->deadline }.detach();

			//prepare for next round if not yet done
			if (!exit_flag && miningCoin->mining->state != DONE) memcpy(&local_32, &global_32, sizeof(global_32));
		}
	}
	closeMiner();
	return 0;
}
