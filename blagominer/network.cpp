#include "stdafx.h"
#include "network.h"

#include <curl/curl.h>

std::map <u_long, unsigned long long> satellite_size; // Ñòðóêòóðà ñ îáúåìàìè ïëîòîâ ñàòåëëèòîâ

void init_network_info() {

	burst->network = std::make_shared<t_network_info>();
	burst->network->nodeaddr = "localhost";
	burst->network->nodeport = "8125";
	burst->network->submitTimeout = 1000;
	burst->network->updateraddr = "localhost";
	burst->network->updaterport = "8125";
	burst->network->enable_proxy = false;
	burst->network->proxyport = "8125";
	burst->network->send_interval = 100;
	burst->network->update_interval = 1000;
	burst->network->proxy_update_interval = 500;
	burst->network->network_quality = -1;
	
	bhd->network = std::make_shared<t_network_info>();
	bhd->network->nodeaddr = "localhost";
	bhd->network->nodeport = "8732";
	bhd->network->submitTimeout = 1000;
	bhd->network->updateraddr = "localhost";
	bhd->network->updaterport = "8732";
	bhd->network->enable_proxy = false;
	bhd->network->proxyport = "8732";
	bhd->network->send_interval = 100;
	bhd->network->update_interval = 1000;
	bhd->network->proxy_update_interval = 500;
	bhd->network->network_quality = -1;
}

void hostname_to_ip(char const *const  in_addr, char* out_addr)
{
	struct addrinfo *result = nullptr;
	struct addrinfo *ptr = nullptr;
	struct addrinfo hints;
	DWORD dwRetval;
	struct sockaddr_in  *sockaddr_ipv4;

	RtlSecureZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	dwRetval = getaddrinfo(in_addr, NULL, &hints, &result);
	if (dwRetval != 0) {
		Log(L" getaddrinfo failed with error: %llu", dwRetval);
		WSACleanup();
		exit(-1);
	}
	for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {

		if (ptr->ai_family == AF_INET)
		{
			sockaddr_ipv4 = (struct sockaddr_in *) ptr->ai_addr;
			char str[INET_ADDRSTRLEN];
			inet_ntop(hints.ai_family, &(sockaddr_ipv4->sin_addr), str, INET_ADDRSTRLEN);
			//strcpy(out_addr, inet_ntoa(sockaddr_ipv4->sin_addr));
			strcpy_s(out_addr, 50, str);
			Log(L"Address: %S defined as %S", in_addr, out_addr);
		}
	}
	freeaddrinfo(result);
}


void proxy_i(std::shared_ptr<t_coin_info> coinInfo)
{
	const wchar_t* proxyName = coinNames[coinInfo->coin];
	int iResult;
	size_t const buffer_size = 1000;
	char* buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, buffer_size);
	if (buffer == nullptr) ShowMemErrorExit();
	char* tmp_buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, buffer_size);
	if (tmp_buffer == nullptr) ShowMemErrorExit();
	SOCKET ServerSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;
	struct addrinfo *result = nullptr;
	struct addrinfo hints;

	RtlSecureZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(nullptr, coinInfo->network->proxyport.c_str(), &hints, &result);
	if (iResult != 0) {
		printToConsole(12, true, false, true, false, L"PROXY %s: getaddrinfo failed with error: %i", proxyName, iResult);
	}

	ServerSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ServerSocket == INVALID_SOCKET) {
		printToConsole(12, true, false, true, false, L"PROXY %s: socket failed with error: %i", proxyName, WSAGetLastError());
		freeaddrinfo(result);
	}

	iResult = bind(ServerSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printToConsole(12, true, false, true, false, L"PROXY %s: bind failed with error: %i", proxyName, WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ServerSocket);
	}
	freeaddrinfo(result);
	BOOL l = TRUE;
	iResult = ioctlsocket(ServerSocket, FIONBIO, (unsigned long*)&l);
	if (iResult == SOCKET_ERROR)
	{
		Log(L"Proxy %s: ! Error ioctlsocket's: %i", proxyName, WSAGetLastError());
		printToConsole(12, true, false, true, false, L"PROXY %s: ioctlsocket failed: %i", proxyName, WSAGetLastError());
	}

	iResult = listen(ServerSocket, 8);
	if (iResult == SOCKET_ERROR) {
		printToConsole(12, true, false, true, false, L"PROXY %s: listen failed with error: %i", proxyName, WSAGetLastError());
		closesocket(ServerSocket);
	}
	Log(L"Proxy %s thread started", proxyName);

	while (!exit_flag)
	{
		struct sockaddr_in client_socket_address;
		int iAddrSize = sizeof(struct sockaddr_in);
		ClientSocket = accept(ServerSocket, (struct sockaddr *)&client_socket_address, (socklen_t*)&iAddrSize);

		char client_address_str[INET_ADDRSTRLEN];
		inet_ntop(hints.ai_family, &(client_socket_address.sin_addr), client_address_str, INET_ADDRSTRLEN);

		if (ClientSocket == INVALID_SOCKET)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				Log(L"Proxy %s:! Error Proxy's accept: %i", proxyName, WSAGetLastError());
				printToConsole(12, true, false, true, false, L"PROXY %s: can't accept. Error: %i", proxyName, WSAGetLastError());
			}
		}
		else
		{
			RtlSecureZeroMemory(buffer, buffer_size);
			do {
				RtlSecureZeroMemory(tmp_buffer, buffer_size);
				iResult = recv(ClientSocket, tmp_buffer, (int)(buffer_size - 1), 0);
				strcat_s(buffer, buffer_size, tmp_buffer);
			} while (iResult > 0);

			unsigned long long get_accountId = 0;
			unsigned long long get_nonce = 0;
			unsigned long long get_deadline = 0;
			unsigned long long get_totalsize = 0;
			// locate HTTP header
			char *find = strstr(buffer, "\r\n\r\n");
			if (find != nullptr)
			{
				const unsigned long long targetDeadlineInfo = getTargetDeadlineInfo(coinInfo);
				if (strstr(buffer, "submitNonce") != nullptr)
				{

					char *startaccountId = strstr(buffer, "accountId=");
					if (startaccountId != nullptr)
					{
						startaccountId = strpbrk(startaccountId, "0123456789");
						char *endaccountId = strpbrk(startaccountId, "& }\"");

						char *startnonce = strstr(buffer, "nonce=");
						char *startdl = strstr(buffer, "deadline=");
						char *starttotalsize = strstr(buffer, "X-Capacity");
						if ((startnonce != nullptr) && (startdl != nullptr))
						{
							startnonce = strpbrk(startnonce, "0123456789");
							char *endnonce = strpbrk(startnonce, "& }\"");
							startdl = strpbrk(startdl, "0123456789");
							char *enddl = strpbrk(startdl, "& }\"");

							endaccountId[0] = 0;
							endnonce[0] = 0;
							enddl[0] = 0;

							get_accountId = _strtoui64(startaccountId, 0, 10);
							get_nonce = _strtoui64(startnonce, 0, 10);
							get_deadline = _strtoui64(startdl, 0, 10);

							if (starttotalsize != nullptr)
							{
								starttotalsize = strpbrk(starttotalsize, "0123456789");
								char *endtotalsize = strpbrk(starttotalsize, "& }\"");
								endtotalsize[0] = 0;
								get_totalsize = _strtoui64(starttotalsize, 0, 10);
								satellite_size.insert(std::pair <u_long, unsigned long long>(client_socket_address.sin_addr.S_un.S_addr, get_totalsize));
							}
							EnterCriticalSection(&coinInfo->locks->sharesLock);
							coinInfo->mining->shares.push_back(std::make_shared<t_shares>(
								client_address_str, get_accountId, get_deadline, get_nonce, get_deadline, coinInfo->mining->currentHeight, coinInfo->mining->currentBaseTarget));
							LeaveCriticalSection(&coinInfo->locks->sharesLock);

							printToConsole(2, true, false, true, false, L"[%20llu|%-10s|Proxy ] DL found     : %s {%S}", get_accountId, proxyName,
								toWStr(get_deadline / coinInfo->mining->currentBaseTarget, 11).c_str(), client_address_str);
							Log(L"Proxy %s: received DL %llu from %S", proxyName, get_deadline / coinInfo->mining->currentBaseTarget, client_address_str);
							
							//Подтверждаем
							RtlSecureZeroMemory(buffer, buffer_size);
							size_t acc = Get_index_acc(get_accountId, coinInfo, targetDeadlineInfo);
							int bytes = sprintf_s(buffer, buffer_size, "HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n{\"result\": \"proxy\",\"accountId\": %llu,\"deadline\": %llu,\"targetDeadline\": %llu}", get_accountId, get_deadline / coinInfo->mining->currentBaseTarget, coinInfo->mining->bests[acc].targetDeadline);
							iResult = send(ClientSocket, buffer, bytes, 0);
							if (iResult == SOCKET_ERROR)
							{
								Log(L"Proxy %s: ! Error sending to client: %i", proxyName, WSAGetLastError());
								printToConsole(12, true, false, true, false, L"PROXY %s: failed sending to client: %i", proxyName, WSAGetLastError());
							}
							else
							{
								if (use_debug)
								{
									printToConsole(9, true, false, true, false, L"[%20llu|%-10s|Proxy ] DL confirmed to             %S",
										get_accountId, proxyName, client_address_str);
								}
								Log(L"Proxy %s: sent confirmation to %S", proxyName, client_address_str);
							}
						}
					}
				}
				else
				{
					if (strstr(buffer, "getMiningInfo") != nullptr)
					{
						char* str_signature = getCurrentStrSignature(coinInfo);

						RtlSecureZeroMemory(buffer, buffer_size);
						int bytes = sprintf_s(buffer, buffer_size, "HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n{\"baseTarget\":\"%llu\",\"height\":\"%llu\",\"generationSignature\":\"%s\",\"targetDeadline\":%llu}", coinInfo->mining->currentBaseTarget, coinInfo->mining->currentHeight, str_signature, targetDeadlineInfo);
						delete[] str_signature;
						iResult = send(ClientSocket, buffer, bytes, 0);
						if (iResult == SOCKET_ERROR)
						{
							Log(L"Proxy %s: ! Error sending to client: %i", proxyName, WSAGetLastError());
							printToConsole(12, true, false, true, false, L"PROXY %s: failed sending to client: %i", proxyName, WSAGetLastError());
						}
						else if (loggingConfig.logAllGetMiningInfos)
						{
							Log(L"Proxy %s: sent update to %S: %S", proxyName, client_address_str, buffer);
						}
					}
					else
					{
						if ((strstr(buffer, "getBlocks") != nullptr) || (strstr(buffer, "getAccount") != nullptr) || (strstr(buffer, "getRewardRecipient") != nullptr))
						{
							; // ничего не делаем, не ошибка, пропускаем
						}
						else
						{
							find[0] = 0;
							//You can crash the miner when the proxy is enabled and you open the address in a browser.  wprintw("PROXY: %s\n", "Error", 0);
							printToConsole(15, true, false, true, false, L"PROXY %s: %S", proxyName, buffer);
						}
					}
				}
			}
			iResult = closesocket(ClientSocket);
		}
		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(coinInfo->network->proxy_update_interval));
	}
	if (buffer != nullptr) {
		HeapFree(hHeap, 0, buffer);
	}
	if (tmp_buffer != nullptr) {
		HeapFree(hHeap, 0, tmp_buffer);
	}
}

void decreaseNetworkQuality(std::shared_ptr<t_coin_info> coin) {
	if (coin->network->network_quality < 0) {
		coin->network->network_quality = 0;
	}
	else if (coin->network->network_quality > 0) {
		coin->network->network_quality--;
	}
}

void increaseNetworkQuality(std::shared_ptr<t_coin_info> coin) {
	if (coin->network->network_quality < 0) {
		coin->network->network_quality = 100;
	}
	else if (coin->network->network_quality < 100) {
		coin->network->network_quality++;
	}
}

void __impl__send_i__sockets(char* buffer, size_t buffer_size, std::shared_ptr<t_coin_info> coinInfo, std::vector<std::shared_ptr<t_session>>& tmpSessions, unsigned long long targetDeadlineInfo, std::shared_ptr<t_shares> share)
{
	const wchar_t* senderName = coinNames[coinInfo->coin];

	SOCKET ConnectSocket;

	int iResult = 0;

	struct addrinfo *result = nullptr;
	struct addrinfo hints;

	RtlSecureZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo(coinInfo->network->nodeaddr.c_str(), coinInfo->network->nodeport.c_str(), &hints, &result);
	if (iResult != 0) {
		decreaseNetworkQuality(coinInfo);
		printToConsole(12, true, false, true, false, L"[%20llu|%-10s|Sender] getaddrinfo failed with error: %i", share->account_id, senderName, iResult);
		return;
	}
	ConnectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ConnectSocket == INVALID_SOCKET) {
		decreaseNetworkQuality(coinInfo);
		printToConsole(12, true, false, true, false, L"SENDER %s: socket failed with error: %i", senderName, WSAGetLastError());
		freeaddrinfo(result);
		return;
	}
	setsockopt(ConnectSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&coinInfo->network->submitTimeout, sizeof(unsigned));
	iResult = connect(ConnectSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		decreaseNetworkQuality(coinInfo);
		Log(L"Sender %s:! Error Sender's connect: %i", senderName, WSAGetLastError());
		printToConsole(12, true, false, true, false, L"SENDER %s: can't connect. Error: %i", senderName, WSAGetLastError());
		freeaddrinfo(result);
		return;
	}
	else
	{
		freeaddrinfo(result);

		int bytes = 0;
		RtlSecureZeroMemory(buffer, buffer_size);
		if (coinInfo->mining->miner_mode == 0)
		{
			bytes = sprintf_s(buffer, buffer_size, "POST /burst?requestType=submitNonce&secretPhrase=%s&nonce=%llu HTTP/1.0\r\nConnection: close\r\n\r\n", pass, share->nonce);
		}
		if (coinInfo->mining->miner_mode == 1)
		{
			unsigned long long total = total_size / 1024 / 1024 / 1024;
			for (auto It = satellite_size.begin(); It != satellite_size.end(); ++It) total = total + It->second;

			char const* format = "POST /burst?requestType=submitNonce&accountId=%llu&nonce=%llu&deadline=%llu%s HTTP/1.0\r\nHost: %s:%s\r\nX-Miner: Blago %S\r\nX-Capacity: %llu\r\n%sContent-Length: 0\r\nConnection: close\r\n\r\n";
			bytes = sprintf_s(buffer, buffer_size, format, share->account_id, share->nonce, share->best, coinInfo->network->sendextraquery.c_str(), coinInfo->network->nodeaddr.c_str(), coinInfo->network->nodeport.c_str(), version.c_str(), total, coinInfo->network->sendextraheader.c_str());
		}

		// Sending to server
		iResult = send(ConnectSocket, buffer, bytes, 0);
		if (iResult == SOCKET_ERROR)
		{
			decreaseNetworkQuality(coinInfo);
			Log(L"Sender %s: ! Error deadline's sending: %i", senderName, WSAGetLastError());
			printToConsole(12, true, false, true, false, L"SENDER %s: send failed: %i", senderName, WSAGetLastError());
			return;
		}
		else
		{
			increaseNetworkQuality(coinInfo);
			printToConsole(9, true, false, true, false, L"[%20llu|%-10s|Sender] DL sent      : %s %sd %02llu:%02llu:%02llu",
				share->account_id,
				senderName,
				toWStr(share->deadline, 11).c_str(),
				toWStr(share->deadline / (24 * 60 * 60), 7).c_str(),
				(share->deadline % (24 * 60 * 60)) / (60 * 60),
				(share->deadline % (60 * 60)) / 60, 
				share->deadline % 60);

			tmpSessions.push_back(std::make_shared<t_session>(ConnectSocket, share->deadline, *share));

			Log(L"[%20llu] Sender %s: Setting bests targetDL: %10llu", share->account_id, senderName, share->deadline);
			coinInfo->mining->bests[Get_index_acc(share->account_id, coinInfo, targetDeadlineInfo)].targetDeadline = share->deadline;
			EnterCriticalSection(&coinInfo->locks->sharesLock);
			if (!coinInfo->mining->shares.empty()) {
				coinInfo->mining->shares.erase(coinInfo->mining->shares.begin());
			}
			LeaveCriticalSection(&coinInfo->locks->sharesLock);
		}
	}
}

void send_i(std::shared_ptr<t_coin_info> coinInfo)
{
	const wchar_t* senderName = coinNames[coinInfo->coin];
	Log(L"Sender %s: started thread", senderName);

	size_t const buffer_size = 1000;
	char* buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, buffer_size);
	if (buffer == nullptr) ShowMemErrorExit();

	std::vector<std::shared_ptr<t_session>> tmpSessions;
	std::vector<std::shared_ptr<t_session2>> tmpSessions2;

	while (!exit_flag) {
		std::shared_ptr<t_shares> share;

		EnterCriticalSection(&coinInfo->locks->sharesLock);
		if (!coinInfo->mining->shares.empty()) {
			share = coinInfo->mining->shares.front();
		}
		LeaveCriticalSection(&coinInfo->locks->sharesLock);

		if (share == nullptr) {
			// No more data for now

			if (!tmpSessions.empty()) {
				EnterCriticalSection(&coinInfo->locks->sessionsLock);
				for (auto& session : tmpSessions) {
					coinInfo->network->sessions.push_back(session);
				}
				LeaveCriticalSection(&coinInfo->locks->sessionsLock);
				tmpSessions.clear();
			}
			if (!tmpSessions2.empty()) {
				EnterCriticalSection(&coinInfo->locks->sessions2Lock);
				for (auto& session : tmpSessions2) {
					coinInfo->network->sessions2.push_back(session);
				}
				LeaveCriticalSection(&coinInfo->locks->sessions2Lock);
				tmpSessions2.clear();
			}

			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(coinInfo->network->send_interval));
			continue;
		}

		const unsigned long long targetDeadlineInfo = getTargetDeadlineInfo(coinInfo);
		//Гасим шару если она больше текущего targetDeadline, актуально для режима Proxy
		if (share->deadline > coinInfo->mining->bests[Get_index_acc(share->account_id, coinInfo, targetDeadlineInfo)].targetDeadline)
		{
			Log(L"[%20llu|%-10s|Sender] DL discarded : %llu > %llu",
				share->account_id, senderName, share->deadline,
				coinInfo->mining->bests[Get_index_acc(share->account_id, coinInfo, targetDeadlineInfo)].targetDeadline);
			if (use_debug)
			{
				printToConsole(2, true, false, true, false, L"[%20llu|%-10s|Sender] DL discarded : %s > %s",
					share->account_id, senderName, toWStr(share->deadline, 11).c_str(),
					toWStr(coinInfo->mining->bests[Get_index_acc(share->account_id, coinInfo, targetDeadlineInfo)].targetDeadline, 11).c_str());
			}
			EnterCriticalSection(&coinInfo->locks->sharesLock);
			if (!coinInfo->mining->shares.empty()) {
				coinInfo->mining->shares.erase(coinInfo->mining->shares.begin());
			}
			LeaveCriticalSection(&coinInfo->locks->sharesLock);
			continue;
		}

		if (share->height != coinInfo->mining->currentHeight) {
			Log(L"Sender %s: DL %llu from block %llu discarded. New block %llu",
				senderName, share->deadline, share->height, coinInfo->mining->currentHeight);
			EnterCriticalSection(&coinInfo->locks->sharesLock);
			if (!coinInfo->mining->shares.empty()) {
				coinInfo->mining->shares.erase(coinInfo->mining->shares.begin());
			}
			LeaveCriticalSection(&coinInfo->locks->sharesLock);
			continue;
		}

		__impl__send_i__sockets(buffer, buffer_size, coinInfo, tmpSessions, targetDeadlineInfo, share);
	}
	if (buffer != nullptr) {
		HeapFree(hHeap, 0, buffer);
	}
	Log(L"Sender %s: All work done, shutting down.", senderName);
}


bool __impl__confirm_i__sockets(char* buffer, size_t buffer_size, std::shared_ptr<t_coin_info> coinInfo, rapidjson::Document& output, char*& find, bool& nonJsonSuccessDetected, std::shared_ptr<t_session>& session) {
	bool failed = false;

	const wchar_t* confirmerName = coinNames[coinInfo->coin];

	SOCKET ConnectSocket;
	int iResult = 0;

	EnterCriticalSection(&coinInfo->locks->sessionsLock);
	if (!coinInfo->network->sessions.empty()) {
		session = coinInfo->network->sessions.front();
	}
	LeaveCriticalSection(&coinInfo->locks->sessionsLock);

	if (session == nullptr) {
		// No more data for now

		return true;
	}

	if (session->body.height != coinInfo->mining->currentHeight) {
		Log(L"Confirmer %s: DL %llu from block %llu discarded. New block %llu",
			confirmerName, session->deadline, session->body.height, coinInfo->mining->currentHeight);
		EnterCriticalSection(&coinInfo->locks->sessionsLock);
		if (!coinInfo->network->sessions.empty()) {
			coinInfo->network->sessions.erase(coinInfo->network->sessions.begin());
		}
		LeaveCriticalSection(&coinInfo->locks->sessionsLock);
		return true;
	}

	const unsigned long long targetDeadlineInfo = getTargetDeadlineInfo(coinInfo);
		
	ConnectSocket = session->Socket;

	// Make socket blocking
	BOOL l = FALSE;
	iResult = ioctlsocket(ConnectSocket, FIONBIO, (unsigned long*)&l);
	if (iResult == SOCKET_ERROR)
	{
		decreaseNetworkQuality(coinInfo);
		Log(L"Confirmer %s: ! Error ioctlsocket's: %i", confirmerName, WSAGetLastError());
		printToConsole(12, true, false, true, false, L"SENDER %s: ioctlsocket failed: %i", confirmerName, WSAGetLastError());
		return true;
	}
	RtlSecureZeroMemory(buffer, buffer_size);
	iResult = recv(ConnectSocket, buffer, (int)buffer_size, 0);

	if (iResult == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK) //разрыв соединения, молча переотправляем дедлайн
		{
			decreaseNetworkQuality(coinInfo);
			Log(L"Confirmer %s: ! Error getting confirmation for DL: %llu  code: %i", confirmerName, session->deadline, WSAGetLastError());
			EnterCriticalSection(&coinInfo->locks->sessionsLock);
			if (!coinInfo->network->sessions.empty()) {
				coinInfo->network->sessions.erase(coinInfo->network->sessions.begin());
			}
			LeaveCriticalSection(&coinInfo->locks->sessionsLock);
			EnterCriticalSection(&coinInfo->locks->sharesLock);
			coinInfo->mining->shares.push_back(std::make_shared<t_shares>(
				session->body.file_name,
				session->body.account_id, 
				session->body.best, 
				session->body.nonce,
				session->body.deadline,
				session->body.height,
				session->body.baseTarget));
			LeaveCriticalSection(&coinInfo->locks->sharesLock);
		}
		failed = true;
	}
	else //что-то получили от сервера
	{
		increaseNetworkQuality(coinInfo);

		//получили пустую строку, переотправляем дедлайн
		if (buffer[0] == '\0')
		{
			Log(L"Confirmer %s: zero-length message for DL: %llu", confirmerName, session->deadline);
			EnterCriticalSection(&coinInfo->locks->sharesLock);
			coinInfo->mining->shares.push_back(std::make_shared<t_shares>(
				session->body.file_name,
				session->body.account_id, 
				session->body.best, 
				session->body.nonce,
				session->body.deadline,
				session->body.height,
				session->body.baseTarget));
			LeaveCriticalSection(&coinInfo->locks->sharesLock);
			failed = true;
		}
		else //получили ответ пула
		{
			find = strstr(buffer, "{");
			if (find == nullptr)
			{
				find = strstr(buffer, "\r\n\r\n");
				if (find != nullptr) find = find + 4;
				else find = buffer;
			}

			unsigned long long ndeadline;
			unsigned long long naccountId = 0;
			unsigned long long ntargetDeadline = 0;

			rapidjson::Document& answ = output;
			if (answ.Parse<0>(find).HasParseError())
			{
				if (strstr(find, "Received share") != nullptr)
				{
					failed = false;
					nonJsonSuccessDetected = true;
				}
				else //получили нераспознанный ответ
				{
					failed = true;

					int minor_version;
					int status = 0;
					const char *msg;
					size_t msg_len;
					struct phr_header headers[12];
					size_t num_headers = sizeof(headers) / sizeof(headers[0]);
					phr_parse_response(buffer, strlen(buffer), &minor_version, &status, &msg, &msg_len, headers, &num_headers, 0);

					if (status != 0)
					{
						std::string error_str(msg, msg_len);
						printToConsole(6, true, false, true, false, L"%s: Server error: %d %S", confirmerName, status, error_str.c_str());
						Log(L"Confirmer %s: server error for DL: %llu", confirmerName, session->deadline);
						EnterCriticalSection(&coinInfo->locks->sharesLock);
						coinInfo->mining->shares.push_back(std::make_shared<t_shares>(
							session->body.file_name,
							session->body.account_id,
							session->body.best,
							session->body.nonce,
							session->body.deadline,
							session->body.height,
							session->body.baseTarget));
						LeaveCriticalSection(&coinInfo->locks->sharesLock);
					}
					else //получили непонятно что
					{
						printToConsole(7, true, false, true, false, L"%s: %S", confirmerName, buffer);
					}
				}
			}
			else
			{
				failed = false;
				nonJsonSuccessDetected = false;
			}
		}
		iResult = closesocket(ConnectSocket);
		Log(L"Confirmer %s: Close socket. Code = %i", confirmerName, WSAGetLastError());
		EnterCriticalSection(&coinInfo->locks->sessionsLock);
		if (!coinInfo->network->sessions.empty()) {
			coinInfo->network->sessions.erase(coinInfo->network->sessions.begin());
		}
		LeaveCriticalSection(&coinInfo->locks->sessionsLock);
	}
	return failed;
}

void confirm_i(std::shared_ptr<t_coin_info> coinInfo) {
	const wchar_t* confirmerName = coinNames[coinInfo->coin];
	Log(L"Confirmer %s: started thread", confirmerName);

	SOCKET ConnectSocket;
	int iResult = 0;
	size_t const buffer_size = 1000;
	char* buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, buffer_size);
	if (buffer == nullptr) ShowMemErrorExit();

	while (!exit_flag) {

		std::shared_ptr<t_session> session;

		const unsigned long long targetDeadlineInfo = getTargetDeadlineInfo(coinInfo);
		
		unsigned long long ndeadline;
		unsigned long long naccountId = 0;
		unsigned long long ntargetDeadline = 0;

		char* find;
		bool nonJsonSuccessDetected;
		rapidjson::Document answ;
		bool failedOrNoData = __impl__confirm_i__sockets(buffer, buffer_size, coinInfo, answ, find, nonJsonSuccessDetected, session);

		// burst.ninja        {"requestProcessingTime":0,"result":"success","block":216280,"deadline":304917,"deadlineString":"3 days, 12 hours, 41 mins, 57 secs","targetDeadline":304917}
		// pool.burst-team.us {"requestProcessingTime":0,"result":"success","block":227289,"deadline":867302,"deadlineString":"10 days, 55 mins, 2 secs","targetDeadline":867302}
		// proxy              {"result": "proxy","accountId": 17930413153828766298,"deadline": 1192922,"targetDeadline": 197503}
		if (!failedOrNoData && !nonJsonSuccessDetected)
		{
			if (answ.IsObject())
			{
				if (answ.HasMember("deadline")) {
					if (answ["deadline"].IsString())	ndeadline = _strtoui64(answ["deadline"].GetString(), 0, 10);
					else
						if (answ["deadline"].IsInt64()) ndeadline = answ["deadline"].GetInt64();
					Log(L"Confirmer %s: confirmed deadline: %llu", confirmerName, ndeadline);

					if (answ.HasMember("targetDeadline")) {
						if (answ["targetDeadline"].IsString())	ntargetDeadline = _strtoui64(answ["targetDeadline"].GetString(), 0, 10);
						else
							if (answ["targetDeadline"].IsInt64()) ntargetDeadline = answ["targetDeadline"].GetInt64();
					}
					if (answ.HasMember("accountId")) {
						if (answ["accountId"].IsString())	naccountId = _strtoui64(answ["accountId"].GetString(), 0, 10);
						else
							if (answ["accountId"].IsInt64()) naccountId = answ["accountId"].GetInt64();
					}

					unsigned long long days = (ndeadline) / (24 * 60 * 60);
					unsigned hours = (ndeadline % (24 * 60 * 60)) / (60 * 60);
					unsigned min = (ndeadline % (60 * 60)) / 60;
					unsigned sec = ndeadline % 60;
					if ((naccountId != 0) && (ntargetDeadline != 0))
					{
						EnterCriticalSection(&coinInfo->locks->bestsLock);
						coinInfo->mining->bests[Get_index_acc(naccountId, coinInfo, targetDeadlineInfo)].targetDeadline = ntargetDeadline;
						LeaveCriticalSection(&coinInfo->locks->bestsLock);

						printToConsole(10, true, false, true, false, L"[%20llu|%-10s|Sender] DL confirmed : %s %sd %02u:%02u:%02u",
							naccountId, confirmerName, toWStr(ndeadline, 11).c_str(), toWStr(days, 7).c_str(), hours, min, sec);
						Log(L"[%20llu] %s confirmed DL: %10llu %5llud %02u:%02u:%02u", naccountId, confirmerName, ndeadline, days, hours, min, sec);
						Log(L"[%20llu] %s set targetDL: %10llu", naccountId, confirmerName, ntargetDeadline);
						if (use_debug) {
							printToConsole(10, true, false, true, false, L"[%20llu|%-10s|Sender] Set target DL: %s",
								naccountId, toWStr(ntargetDeadline, 11).c_str());
						}
					}
					else {
						printToConsole(10, true, false, true, false, L"[%20llu|%-10s|Sender] DL confirmed : %s %sd %02u:%02u:%02u",
							session->body.account_id, confirmerName, toWStr(ndeadline, 11).c_str(), toWStr(days, 7).c_str(), hours, min, sec);
						Log(L"[%20llu] %s confirmed DL: %10llu %5llud %02u:%02u:%02u", session->body.account_id, confirmerName, ndeadline, days, hours, min, sec);
					}
					if (ndeadline < coinInfo->mining->deadline || coinInfo->mining->deadline == 0)  coinInfo->mining->deadline = ndeadline;

					if (ndeadline != session->deadline)
					{
						Log(L"Confirmer %s: Calculated and confirmed deadlines don't match. Fast block or corrupted file? Response: %S", confirmerName, find);
						std::thread{ Csv_Fail, coinInfo->coin, session->body.height, session->body.file_name, session->body.baseTarget,
							4398046511104 / 240 / session->body.baseTarget, session->body.nonce, session->deadline, ndeadline, find }.detach();
						std::thread{ increaseConflictingDeadline, coinInfo, session->body.height, session->body.file_name }.detach();
						printToConsole(6, false, false, true, false,
							L"----Fast block or corrupted file?----\n%s sent deadline:\t%llu\nServer's deadline:\t%llu \n----",
							confirmerName, session->deadline, ndeadline);
					}
				}
				else {
					if (answ.HasMember("errorDescription")) {
						Log(L"Confirmer %s: Deadline %llu sent with error: %S", confirmerName, session->deadline, find);
						std::thread{ Csv_Fail, coinInfo->coin, session->body.height, session->body.file_name, session->body.baseTarget,
								4398046511104 / 240 / session->body.baseTarget, session->body.nonce, session->deadline, 0, find }.detach();
						std::thread{ increaseConflictingDeadline, coinInfo, session->body.height, session->body.file_name }.detach();
						if (session->deadline <= targetDeadlineInfo) {
							Log(L"Confirmer %s: Deadline should have been accepted (%llu <= %llu). Fast block or corrupted file?", confirmerName, session->deadline, targetDeadlineInfo);
							printToConsole(6, false, false, true, false,
								L"----Fast block or corrupted file?----\n%s sent deadline:\t%llu\nTarget deadline:\t%llu \n----",
								confirmerName, session->deadline, targetDeadlineInfo);
						}
						if (answ["errorCode"].IsInt()) {
							printToConsole(15, true, false, true, false, L"[ERROR %i] %s: %S", answ["errorCode"].GetInt(), confirmerName, answ["errorDescription"].GetString());
							if (answ["errorCode"].GetInt() == 1004) {
								printToConsole(12, true, false, true, false, L"%s: You need change reward assignment and wait 4 blocks (~16 minutes)", confirmerName); //error 1004
							}
						}
						else if (answ["errorCode"].IsString()) {
							printToConsole(15, true, false, true, false, L"[ERROR %S] %s: %S", answ["errorCode"].GetString(), confirmerName, answ["errorDescription"].GetString());
							if (answ["errorCode"].GetString() == "1004") {
								printToConsole(12, true, false, true, false, L"%s: You need change reward assignment and wait 4 blocks (~16 minutes)", confirmerName); //error 1004
							}
						}
						else {
							printToConsole(15, true, false, true, false, L"[ERROR] %s: %S", confirmerName, answ["errorDescription"].GetString());
						}
					}
					else {
						printToConsole(15, true, false, true, false, L"%s: %S", confirmerName, find);
					}
				}
			}
		}
		else if (!failedOrNoData && nonJsonSuccessDetected)
		{
				coinInfo->mining->deadline = coinInfo->mining->bests[Get_index_acc(session->body.account_id, coinInfo, targetDeadlineInfo)].DL; //может лучше iter->deadline ?
																			// if(deadline > iter->deadline) deadline = iter->deadline;
				std::thread{ increaseMatchingDeadline, session->body.file_name }.detach();
				printToConsole(10, true, false, true, false, L"[%20llu|%-10s|Sender] DL confirmed : %s",
					session->body.account_id, confirmerName, toWStr(session->deadline, 11).c_str());
		}

		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(coinInfo->network->send_interval));
	}
	if (buffer != nullptr) {
		HeapFree(hHeap, 0, buffer);
	}
	Log(L"Confirmer %s: All work done, shutting down.", confirmerName);
}

void updater_i(std::shared_ptr<t_coin_info> coinInfo)
{
	const wchar_t* updaterName = coinNames[coinInfo->coin];
	if (coinInfo->network->updateraddr.length() <= 3) {
		Log(L"Updater %s: GMI: ERROR in UpdaterAddr", updaterName);
		exit(2);
	}
	while (!exit_flag) {
		if (pollLocal(coinInfo) && coinInfo->mining->enable && !coinInfo->mining->newMiningInfoReceived) {
			setnewMiningInfoReceived(coinInfo, true);
		}

		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(coinInfo->network->update_interval));
	}
}


bool __impl__pollLocal__sockets(std::shared_ptr<t_coin_info> coinInfo, rapidjson::Document& output, std::string& rawResponse) {
	bool failed = false;

	const wchar_t* updaterName = coinNames[coinInfo->coin];
	bool newBlock = false;
	size_t const buffer_size = 1000;
	char *buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, buffer_size);
	if (buffer == nullptr) ShowMemErrorExit();

	int iResult;
	struct addrinfo *result = nullptr;
	struct addrinfo hints;
	SOCKET UpdaterSocket = INVALID_SOCKET;

	RtlSecureZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo(coinInfo->network->updateraddr.c_str(), coinInfo->network->updaterport.c_str(), &hints, &result);
	if (iResult != 0) {
		decreaseNetworkQuality(coinInfo);
		Log(L"*! GMI %s: getaddrinfo failed with error: %i", updaterName, WSAGetLastError());
		failed = true;
	}
	else {
		UpdaterSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (UpdaterSocket == INVALID_SOCKET)
		{
			decreaseNetworkQuality(coinInfo);
			Log(L"*! GMI %s: socket function failed with error: %i", updaterName, WSAGetLastError());
			failed = true;
		}
		else {
			const unsigned t = 1000;
			setsockopt(UpdaterSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&t, sizeof(unsigned));
			//Log("*Connecting to server: %S:%S", updateraddr.c_str(), updaterport.c_str());
			iResult = connect(UpdaterSocket, result->ai_addr, (int)result->ai_addrlen);
			if (iResult == SOCKET_ERROR) {
				decreaseNetworkQuality(coinInfo);
				Log(L"*! GMI %s: connect function failed with error: %i", updaterName, WSAGetLastError());
				failed = true;
			}
			else {
				int bytes = sprintf_s(buffer, buffer_size, "POST /burst?requestType=getMiningInfo HTTP/1.0\r\nHost: %s:%s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", coinInfo->network->nodeaddr.c_str(), coinInfo->network->nodeport.c_str());
				iResult = send(UpdaterSocket, buffer, bytes, 0);
				if (iResult == SOCKET_ERROR)
				{
					decreaseNetworkQuality(coinInfo);
					Log(L"*! GMI %s: send request failed: %i", updaterName, WSAGetLastError());
					failed = true;
				}
				else {
					RtlSecureZeroMemory(buffer, buffer_size);
					size_t  pos = 0;
					iResult = 0;
					do {
						iResult = recv(UpdaterSocket, &buffer[pos], (int)(buffer_size - pos - 1), 0);
						if (iResult > 0) pos += (size_t)iResult;
					} while (iResult > 0);
					if (iResult == SOCKET_ERROR)
					{
						decreaseNetworkQuality(coinInfo);
						Log(L"*! GMI %s: get mining info failed:: %i", updaterName, WSAGetLastError());
						failed = true;
					}
					else {
						increaseNetworkQuality(coinInfo);
						if (loggingConfig.logAllGetMiningInfos) {
							Log(L"* GMI %s: Received: %S", updaterName, Log_server(buffer).c_str());
						}

						rawResponse = buffer;
						rapidjson::Document& gmi = output;

						// locate HTTP header
						char const* find = strstr(buffer, "\r\n\r\n");
						if (find == nullptr) {
							Log(L"*! GMI %s: error message from pool: %S", updaterName, Log_server(buffer).c_str());
							failed = true;
						}
						else if (loggingConfig.logAllGetMiningInfos && gmi.Parse<0>(find).HasParseError()) {
							Log(L"*! GMI %s: error parsing JSON message from pool", updaterName);
							failed = true;
						}
						else if (!loggingConfig.logAllGetMiningInfos && gmi.Parse<0>(find).HasParseError()) {
							Log(L"*! GMI %s: error parsing JSON message from pool: %S", updaterName, Log_server(buffer).c_str());
							failed = true;
						}
					}
				}
			}
			iResult = closesocket(UpdaterSocket);
		}
		freeaddrinfo(result);
	}
	if (buffer != nullptr) {
		HeapFree(hHeap, 0, buffer);
	}
	return failed;
}

struct MemoryStruct { char *memory; size_t size; };
static size_t __impl__pollLocal__curl__readcallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;

	char *ptr = !mem->memory ?
		(char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, mem->size + realsize + 1)
		: (char*)HeapReAlloc(hHeap, HEAP_ZERO_MEMORY, mem->memory, mem->size + realsize + 1);
	if (!ptr) ShowMemErrorExit();

	mem->memory = ptr;
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}
bool __impl__pollLocal__curl(std::shared_ptr<t_coin_info> coinInfo, rapidjson::Document& output, std::string& rawResponse) {
	bool failed = false;

	const wchar_t* updaterName = coinNames[coinInfo->coin];
	bool newBlock = false;
	
	// TODO: fixup: extract outside like it was before
	size_t const buffer_size = 1000;
	char *buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, buffer_size);
	if (buffer == nullptr) ShowMemErrorExit();

	struct MemoryStruct chunk;
	chunk.memory = 0;
	chunk.size = 0;

	CURL* curl = curl_easy_init();
	if (!curl) {
		failed = true;
	}
	else {
		if (false) { // coinInfo->network->SKIP_PEER_VERIFICATION
			/*
			 * If you want to connect to a site who isn't using a certificate that is
			 * signed by one of the certs in the CA bundle you have, you can skip the
			 * verification of the server's certificate. This makes the connection
			 * A LOT LESS SECURE.
			 *
			 * If you have a CA cert for the server stored someplace else than in the
			 * default bundle, then the CURLOPT_CAPATH option might come handy for
			 * you.
			 */
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		}

		if (false) { // coinInfo->network->SKIP_HOSTNAME_VERIFICATION
			/*
			 * If the site you're connecting to uses a different host name that what
			 * they have mentioned in their server certificate's commonName (or
			 * subjectAltName) fields, libcurl will refuse to connect. You can skip
			 * this check, but this will make the connection less secure.
			 */
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		}

		int bytes = sprintf_s(buffer, buffer_size, "https://%s:%s/burst?requestType=getMiningInfo", coinInfo->network->nodeaddr.c_str(), coinInfo->network->nodeport.c_str());
		curl_easy_setopt(curl, CURLOPT_URL, buffer);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ""); // wee need to send a POST but body may be left empty
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, __impl__pollLocal__curl__readcallback);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, coinInfo->network->submitTimeout);

		/* Perform the request, res will get the return code */
		CURLcode res = curl_easy_perform(curl);

		/* Check for errors */
		if (res != CURLE_OK) {
			decreaseNetworkQuality(coinInfo);
			Log(L"*! GMI %s: get mining info failed:: %S", updaterName, curl_easy_strerror(res));
			failed = true;
		}
		else {
			increaseNetworkQuality(coinInfo);
			if (loggingConfig.logAllGetMiningInfos) {
				Log(L"* GMI %s: Received: %S", updaterName, Log_server(chunk.memory).c_str());
			}

			rawResponse.assign(chunk.memory, chunk.size); // TODO: not so 'raw', that's just the response BODY with no headers
			rapidjson::Document& gmi = output;
			if (loggingConfig.logAllGetMiningInfos && gmi.Parse<0>(chunk.memory).HasParseError()) {
				Log(L"*! GMI %s: error parsing JSON message from pool", updaterName);
				failed = true;
			}
			else if (!loggingConfig.logAllGetMiningInfos && gmi.Parse<0>(chunk.memory).HasParseError()) {
				Log(L"*! GMI %s: error parsing JSON message from pool: %S", updaterName, Log_server(chunk.memory).c_str());
				failed = true;
			}
		}

		/* always cleanup */
		curl_easy_cleanup(curl);
	}

	if (chunk.memory != nullptr) {
		HeapFree(hHeap, 0, chunk.memory);
	}
	if (buffer != nullptr) {
		HeapFree(hHeap, 0, buffer);
	}
	return failed;
}

bool pollLocal(std::shared_ptr<t_coin_info> coinInfo) {
	std::string rawResponse;
	rapidjson::Document gmi;
	bool failed;

	if (coinInfo->network->usehttps)
		failed = __impl__pollLocal__curl(coinInfo, gmi, rawResponse);
	else
		failed = __impl__pollLocal__sockets(coinInfo, gmi, rawResponse);

	if (failed) return false;
	if (!gmi.IsObject()) return false;

	const wchar_t* updaterName = coinNames[coinInfo->coin];
	bool newBlock = false;

	if (gmi.HasMember("baseTarget")) {
		if (gmi["baseTarget"].IsString())	coinInfo->mining->baseTarget = _strtoui64(gmi["baseTarget"].GetString(), 0, 10);
		else
			if (gmi["baseTarget"].IsInt64())coinInfo->mining->baseTarget = gmi["baseTarget"].GetInt64();
	}

	if (gmi.HasMember("height")) {
		if (gmi["height"].IsString())	setHeight(coinInfo, _strtoui64(gmi["height"].GetString(), 0, 10));
		else
			if (gmi["height"].IsInt64()) setHeight(coinInfo, gmi["height"].GetInt64());
	}

	//POC2 determination
	if (getHeight(coinInfo) >= coinInfo->mining->POC2StartBlock) {
		POC2 = true;
	}

	if (gmi.HasMember("generationSignature")) {
		setStrSignature(coinInfo, gmi["generationSignature"].GetString());
		char sig[33];
		size_t sigLen = xstr2strr(sig, 33, gmi["generationSignature"].GetString());
		bool sigDiffer = signaturesDiffer(coinInfo, sig);
										
		if (sigLen <= 1) {
			Log(L"*! GMI %s: Node response: Error decoding generationsignature: %S", updaterName, Log_server(rawResponse.c_str()).c_str());
		}
		else if (sigDiffer) {
			newBlock = true;
			setSignature(coinInfo, sig);
			if (!loggingConfig.logAllGetMiningInfos) {
				Log(L"*! GMI %s: Received new mining info: %S", updaterName, Log_server(rawResponse.c_str()).c_str());
			}
		}
	}
	if (gmi.HasMember("targetDeadline")) {
		unsigned long long newTargetDeadlineInfo = 0;
		if (gmi["targetDeadline"].IsString()) {
			newTargetDeadlineInfo = _strtoui64(gmi["targetDeadline"].GetString(), 0, 10);
		}
		else {
			newTargetDeadlineInfo = gmi["targetDeadline"].GetInt64();
		}
		if (loggingConfig.logAllGetMiningInfos || newBlock) {
			Log(L"*! GMI %s: newTargetDeadlineInfo: %llu", updaterName, newTargetDeadlineInfo);
			Log(L"*! GMI %s: my_target_deadline: %llu", updaterName, coinInfo->mining->my_target_deadline);
		}
		if ((newTargetDeadlineInfo > 0) && (newTargetDeadlineInfo < coinInfo->mining->my_target_deadline)) {
			setTargetDeadlineInfo(coinInfo, newTargetDeadlineInfo);
			if (loggingConfig.logAllGetMiningInfos || newBlock) {
				Log(L"*! GMI %s: Target deadline from pool is lower than deadline set in the configuration. Updating targetDeadline: %llu", updaterName, newTargetDeadlineInfo);
			}
		}
		else {
			setTargetDeadlineInfo(coinInfo, coinInfo->mining->my_target_deadline);
			if (loggingConfig.logAllGetMiningInfos || newBlock) {
				Log(L"*! GMI %s: Using target deadline from configuration: %llu", updaterName, coinInfo->mining->my_target_deadline);
			}
		}
	}
	else {
		setTargetDeadlineInfo(coinInfo, coinInfo->mining->my_target_deadline);
		Log(L"*! GMI %s: No target deadline information provided. Using target deadline from configuration: %llu", updaterName, coinInfo->mining->my_target_deadline);
	}

	return newBlock;
}

// Helper routines taken from http://stackoverflow.com/questions/1557400/hex-to-char-array-in-c
int xdigit(char const digit) {
	int val;
	if ('0' <= digit && digit <= '9') val = digit - '0';
	else if ('a' <= digit && digit <= 'f') val = digit - 'a' + 10;
	else if ('A' <= digit && digit <= 'F') val = digit - 'A' + 10;
	else val = -1;
	return val;
}

size_t xstr2strr(char *buf, size_t const bufsize, const char *const in) {
	if (!in) return 0; // missing input string

	size_t inlen = (size_t)strlen(in);
	if (inlen % 2 != 0) inlen--; // hex string must even sized

	size_t i, j;
	for (i = 0; i<inlen; i++)
		if (xdigit(in[i])<0) return 0; // bad character in hex string

	if (!buf || bufsize<inlen / 2 + 1) return 0; // no buffer or too small

	for (i = 0, j = 0; i<inlen; i += 2, j++)
		buf[j] = xdigit(in[i]) * 16 + xdigit(in[i + 1]);

	buf[inlen / 2] = '\0';
	return inlen / 2 + 1;
}