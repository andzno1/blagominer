#include "stdafx.h"
#include "network.h"

std::map <u_long, unsigned long long> satellite_size; // Ñòðóêòóðà ñ îáúåìàìè ïëîòîâ ñàòåëëèòîâ
std::thread showWinnerBurst;

std::string docToString(const Document& doc) {
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	doc.Accept(writer);
	const char* output = buffer.GetString();
	return std::string(output);
}

void init_network_info() {

	burst->network = std::make_shared<t_network_info>();
	burst->network->nodeaddr = "localhost";
	burst->network->nodeport = "8125";
	burst->network->updateraddr = "localhost";
	burst->network->updaterport = "8125";
	burst->network->infoaddr = "localhost";
	burst->network->infoport = "8125";
	burst->network->enable_proxy = false;
	burst->network->proxyport = "8125";
	burst->network->send_interval = 100;
	burst->network->update_interval = 1000;
	burst->network->network_quality = -1;
	burst->network->stopSender = false;

	bhd->network = std::make_shared<t_network_info>();
	bhd->network->nodeaddr = "localhost";
	bhd->network->nodeport = "8732";
	bhd->network->updateraddr = "localhost";
	bhd->network->updaterport = "8732";
	bhd->network->infoaddr = "localhost";
	bhd->network->infoport = "8732";
	bhd->network->enable_proxy = false;
	bhd->network->proxyport = "8732";
	bhd->network->send_interval = 100;
	bhd->network->update_interval = 1000;
	bhd->network->network_quality = -1;
	bhd->network->stopSender = false;
}

char* GetJSON(std::shared_ptr<t_coin_info> coinInfo, char const *const req) {
	const unsigned BUF_SIZE = 1024;

	char *buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, BUF_SIZE);
	if (buffer == nullptr) ShowMemErrorExit();

	char *find = nullptr;
	unsigned long long msg_len = 0;
	int iResult = 0;
	struct addrinfo *result = nullptr;
	struct addrinfo hints;
	SOCKET WalletSocket = INVALID_SOCKET;

	char *json = nullptr;

	RtlSecureZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo(coinInfo->network->infoaddr.c_str(), coinInfo->network->infoport.c_str(), &hints, &result);
	if (iResult != 0) {
		bm_wattron(12);
		bm_wprintw("WINNER: Getaddrinfo failed with error: %d\n", iResult, 0);
		bm_wattroff(12);
		Log("Winner: getaddrinfo error");
	}
	else
	{
		WalletSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (WalletSocket == INVALID_SOCKET)
		{
			bm_wattron(12);
			bm_wprintw("WINNER: Socket function failed with error: %ld\n", WSAGetLastError(), 0);
			bm_wattroff(12);
			Log("Winner: Socket error: %i", WSAGetLastError());
		}
		else
		{
			unsigned t = 1000;
			setsockopt(WalletSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&t, sizeof(unsigned));
			iResult = connect(WalletSocket, result->ai_addr, (int)result->ai_addrlen);
			if (iResult == SOCKET_ERROR) {
				bm_wattron(12);
				bm_wprintw("WINNER: Connect function failed with error: %ld\n", WSAGetLastError(), 0);
				bm_wattroff(12);
				Log("Winner: Connect server error %i", WSAGetLastError());
			}
			else
			{
				iResult = send(WalletSocket, req, (int)strlen(req), 0);
				if (iResult == SOCKET_ERROR)
				{
					bm_wattron(12);
					bm_wprintw("WINNER: Send request failed: %ld\n", WSAGetLastError(), 0);
					bm_wattroff(12);
					Log("Winner: Error sending request: %i", WSAGetLastError());
				}
				else
				{
					char *tmp_buffer;
					unsigned long long msg_len = 0;
					int iReceived_size = 0;
					while ((iReceived_size = recv(WalletSocket, buffer + msg_len, BUF_SIZE - 1, 0)) > 0)
					{
						msg_len = msg_len + iReceived_size;
						tmp_buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, msg_len + BUF_SIZE);
						if (tmp_buffer == nullptr) ShowMemErrorExit();
						memcpy(tmp_buffer, buffer, msg_len);
						HeapFree(hHeap, 0, buffer);
						buffer = tmp_buffer;
						buffer[msg_len + 1] = 0;
						Log("realloc: %llu", msg_len);
					}

					if (iReceived_size < 0)
					{
						bm_wattron(12);
						bm_wprintw("WINNER: Get info failed: %ld\n", WSAGetLastError(), 0);
						bm_wattroff(12);
						Log("Winner: Error response: %i", WSAGetLastError());
					}
					else
					{
						find = strstr(buffer, "\r\n\r\n");
						if (find != nullptr)
						{
							json = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, msg_len);
							if (json == nullptr) ShowMemErrorExit();
							sprintf_s(json, HeapSize(hHeap, 0, json), "%s", find + 4 * sizeof(char));
						}
					} // recv() != SOCKET_ERROR
				} //send() != SOCKET_ERROR
			} // Connect() != SOCKET_ERROR
		} // socket() != INVALID_SOCKET
		iResult = closesocket(WalletSocket);
	} // getaddrinfo() == 0
	HeapFree(hHeap, 0, buffer);
	freeaddrinfo(result);
	return json;
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
		Log(" getaddrinfo failed with error: %llu", dwRetval);
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
			Log("Address: %s defied as %s", in_addr, out_addr);
		}
	}
	freeaddrinfo(result);
}


void proxy_i(std::shared_ptr<t_coin_info> coinInfo)
{
	const char* proxyName = coinNames[coinInfo->coin];
	int iResult;
	size_t const buffer_size = 1000;
	char* buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, buffer_size);
	if (buffer == nullptr) ShowMemErrorExit();
	char* tmp_buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, buffer_size);
	if (tmp_buffer == nullptr) ShowMemErrorExit();
	char tbuffer[9];
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
		bm_wattron(12);
		bm_wprintw("PROXY %s: getaddrinfo failed with error: %d\n", proxyName, iResult, 0);
		bm_wattroff(12);
	}

	ServerSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ServerSocket == INVALID_SOCKET) {
		bm_wattron(12);
		bm_wprintw("PROXY %s: socket failed with error: %ld\n", proxyName, WSAGetLastError(), 0);
		bm_wattroff(12);
		freeaddrinfo(result);
	}

	iResult = bind(ServerSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		bm_wattron(12);
		bm_wprintw("PROXY %s: bind failed with error: %d\n", proxyName, WSAGetLastError(), 0);
		bm_wattroff(12);
		freeaddrinfo(result);
		closesocket(ServerSocket);
	}
	freeaddrinfo(result);
	BOOL l = TRUE;
	iResult = ioctlsocket(ServerSocket, FIONBIO, (unsigned long*)&l);
	if (iResult == SOCKET_ERROR)
	{
		Log("Proxy %s: ! Error ioctlsocket's: %i", proxyName, WSAGetLastError());
		bm_wattron(12);
		bm_wprintw("PROXY %s: ioctlsocket failed: %ld\n", proxyName, WSAGetLastError(), 0);
		bm_wattroff(12);
	}

	iResult = listen(ServerSocket, 8);
	if (iResult == SOCKET_ERROR) {
		bm_wattron(12);
		bm_wprintw("PROXY %s: listen failed with error: %d\n", proxyName, WSAGetLastError(), 0);
		bm_wattroff(12);
		closesocket(ServerSocket);
	}
	Log("Proxy %s thread started", proxyName);

	for (; !exit_flag;)
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
				Log("Proxy %s:! Error Proxy's accept: %i", proxyName, WSAGetLastError());
				bm_wattron(12);
				bm_wprintw("PROXY %s: can't accept. Error: %ld\n", proxyName, WSAGetLastError(), 0);
				bm_wattroff(12);
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
							coinInfo->mining->shares.push_back({ client_address_str, get_accountId, get_deadline, get_nonce });
							LeaveCriticalSection(&coinInfo->locks->sharesLock);

							_strtime_s(tbuffer);
							bm_wattron(2);
							bm_wprintw("%s [%20llu|%-10s|Proxy ] DL found     : %s {%s}\n", tbuffer, get_accountId, proxyName,
								toStr(get_deadline / coinInfo->mining->currentBaseTarget, 11).c_str(), client_address_str, 0);
							bm_wattroff(2);
							Log("Proxy %s: received DL %llu from %s", proxyName, get_deadline, client_address_str);
							
							//Подтверждаем
							RtlSecureZeroMemory(buffer, buffer_size);
							size_t acc = Get_index_acc(get_accountId, coinInfo, targetDeadlineInfo);
							int bytes = sprintf_s(buffer, buffer_size, "HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n{\"result\": \"proxy\",\"accountId\": %llu,\"deadline\": %llu,\"targetDeadline\": %llu}", get_accountId, get_deadline / coinInfo->mining->currentBaseTarget, coinInfo->mining->bests[acc].targetDeadline);
							iResult = send(ClientSocket, buffer, bytes, 0);
							if (iResult == SOCKET_ERROR)
							{
								Log("Proxy %s: ! Error sending to client: %i", proxyName, WSAGetLastError());
								bm_wattron(12);
								bm_wprintw("PROXY %s: failed sending to client: %ld\n", proxyName, WSAGetLastError(), 0);
								bm_wattroff(12);
							}
							else
							{
								if (use_debug)
								{
									_strtime_s(tbuffer);
									bm_wattron(9);
									bm_wprintw("%s [%20llu|%-10s|Proxy ] DL confirmed to            %s\n", tbuffer, get_accountId, proxyName, client_address_str, 0);
									bm_wattroff(9);
								}
								Log("Proxy %s: sent confirmation to %s", proxyName, client_address_str);
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
							Log("Proxy %s: ! Error sending to client: %i", proxyName, WSAGetLastError());
							bm_wattron(12);
							bm_wprintw("PROXY %s: failed sending to client: %ld\n", proxyName, WSAGetLastError(), 0);
							bm_wattroff(12);
						}
						else if (loggingConfig.logAllGetMiningInfos)
						{
							Log("Proxy %s: sent update to %s: %s", proxyName, client_address_str, buffer);
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
							bm_wattron(15);
							bm_wprintw("PROXY %s: %s\n", proxyName, buffer, 0);//You can crash the miner when the proxy is enabled and you open the address in a browser.  wprintw("PROXY: %s\n", "Error", 0);
							bm_wattroff(15);
						}
					}
				}
			}
			iResult = closesocket(ClientSocket);
		}
		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
	HeapFree(hHeap, 0, buffer);
	HeapFree(hHeap, 0, tmp_buffer);
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

void send_i(std::shared_ptr<t_coin_info> coinInfo)
{
	const char* senderName = coinNames[coinInfo->coin];
	Log("Sender %s: started thread", senderName);
	SOCKET ConnectSocket;

	int iResult = 0;
	size_t const buffer_size = 1000;
	char* buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, buffer_size);
	if (buffer == nullptr) ShowMemErrorExit();

	char tbuffer[9];

	struct addrinfo *result = nullptr;
	struct addrinfo hints;

	for (; !exit_flag;)
	{
		if (coinInfo->network->stopSender == 1)
		{
			HeapFree(hHeap, 0, buffer);
			return;
		}

		const unsigned long long targetDeadlineInfo = getTargetDeadlineInfo(coinInfo);
		for (auto iter = coinInfo->mining->shares.begin(); iter != coinInfo->mining->shares.end()
			&& !exit_flag && !coinInfo->network->stopSender;)
		{
			//Гасим шару если она больше текущего targetDeadline, актуально для режима Proxy
			if ((iter->best / coinInfo->mining->currentBaseTarget) > coinInfo->mining->bests[Get_index_acc(iter->account_id, coinInfo, targetDeadlineInfo)].targetDeadline)
			{
				if (use_debug)
				{
					_strtime_s(tbuffer);
					bm_wattron(2);
					bm_wprintw("%s [%20llu|%-10s|Sender] DL discarded : %s > %s\n", tbuffer, iter->account_id, senderName,
						toStr(iter->best / coinInfo->mining->currentBaseTarget, 11).c_str(), toStr(coinInfo->mining->bests[Get_index_acc(iter->account_id, coinInfo, targetDeadlineInfo)].targetDeadline, 11).c_str(), 0);
					bm_wattroff(2);
				}
				EnterCriticalSection(&coinInfo->locks->sharesLock);
				iter = coinInfo->mining->shares.erase(iter);
				LeaveCriticalSection(&coinInfo->locks->sharesLock);
				continue;
			}

			RtlSecureZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;

			iResult = getaddrinfo(coinInfo->network->nodeaddr.c_str(), coinInfo->network->nodeport.c_str(), &hints, &result);
			if (iResult != 0) {
				decreaseNetworkQuality(coinInfo);
				_strtime_s(tbuffer);
				bm_wattron(12);
				bm_wprintw("%s [%20llu|%-10s|Sender] getaddrinfo failed with error: %d\n", tbuffer, iter->account_id, senderName, iResult, 0);
				bm_wattroff(12);
				continue;
			}
			ConnectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
			if (ConnectSocket == INVALID_SOCKET) {
				decreaseNetworkQuality(coinInfo);
				bm_wattron(12);
				bm_wprintw("SENDER %s: socket failed with error: %ld\n", senderName, WSAGetLastError(), 0);
				bm_wattroff(12);
				freeaddrinfo(result);
				continue;
			}
			const unsigned t = 1000;
			setsockopt(ConnectSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&t, sizeof(unsigned));
			iResult = connect(ConnectSocket, result->ai_addr, (int)result->ai_addrlen);
			if (iResult == SOCKET_ERROR)
			{
				decreaseNetworkQuality(coinInfo);
				Log("\nSender %s:! Error Sender's connect: %i", senderName, WSAGetLastError());
				bm_wattron(12);
				_strtime_s(tbuffer);
				bm_wprintw("%s SENDER %s: can't connect. Error: %ld\n", senderName, tbuffer, WSAGetLastError(), 0);
				bm_wattroff(12);
				freeaddrinfo(result);
				continue;
			}
			else
			{
				freeaddrinfo(result);

				int bytes = 0;
				RtlSecureZeroMemory(buffer, buffer_size);
				if (coinInfo->mining->miner_mode == 0)
				{
					bytes = sprintf_s(buffer, buffer_size, "POST /burst?requestType=submitNonce&secretPhrase=%s&nonce=%llu HTTP/1.0\r\nConnection: close\r\n\r\n", pass, iter->nonce);
				}
				if (coinInfo->mining->miner_mode == 1)
				{
					unsigned long long total = total_size / 1024 / 1024 / 1024;
					for (auto It = satellite_size.begin(); It != satellite_size.end(); ++It) total = total + It->second;
					bytes = sprintf_s(buffer, buffer_size, "POST /burst?requestType=submitNonce&accountId=%llu&nonce=%llu&deadline=%llu HTTP/1.0\r\nHost: %s:%s\r\nX-Miner: Blago %s\r\nX-Capacity: %llu\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", iter->account_id, iter->nonce, iter->best, coinInfo->network->nodeaddr.c_str(), coinInfo->network->nodeport.c_str(), version.c_str(), total);
				}

				// Sending to server
				iResult = send(ConnectSocket, buffer, bytes, 0);
				if (iResult == SOCKET_ERROR)
				{
					decreaseNetworkQuality(coinInfo);
					Log("Sender %s: ! Error deadline's sending: %i", senderName, WSAGetLastError());
					bm_wattron(12);
					bm_wprintw("SENDER %s: send failed: %ld\n", senderName, WSAGetLastError(), 0);
					bm_wattroff(12);
					continue;
				}
				else
				{
					unsigned long long dl = iter->best / coinInfo->mining->currentBaseTarget;
					_strtime_s(tbuffer);
					increaseNetworkQuality(coinInfo);
					Log("[%20llu] %s sent DL: %15llu %5llud %02llu:%02llu:%02llu", iter->account_id, senderName, dl, (dl) / (24 * 60 * 60), (dl % (24 * 60 * 60)) / (60 * 60), (dl % (60 * 60)) / 60, dl % 60, 0);
					bm_wattron(9);
					bm_wprintw("%s [%20llu|%-10s|Sender] DL sent      : %s %sd %02llu:%02llu:%02llu\n", tbuffer, iter->account_id, senderName,
						toStr(dl, 11).c_str(), toStr((dl) / (24 * 60 * 60), 7).c_str(), (dl % (24 * 60 * 60)) / (60 * 60), (dl % (60 * 60)) / 60, dl % 60, 0);
					bm_wattroff(9);

					EnterCriticalSection(&coinInfo->locks->sessionsLock);
					//sessions.push_back({ ConnectSocket, iter->account_id, dl, iter->best, iter->nonce });
					coinInfo->network->sessions.push_back({ ConnectSocket, dl, *iter });
					LeaveCriticalSection(&coinInfo->locks->sessionsLock);

					Log("[%20llu] Sender %s: Setting bests targetDL: %10llu", iter->account_id, senderName, dl);
					coinInfo->mining->bests[Get_index_acc(iter->account_id, coinInfo, targetDeadlineInfo)].targetDeadline = dl;
					EnterCriticalSection(&coinInfo->locks->sharesLock);
					iter = coinInfo->mining->shares.erase(iter);
					LeaveCriticalSection(&coinInfo->locks->sharesLock);
				}
			}
		}

		if (!coinInfo->network->sessions.empty())
		{
			EnterCriticalSection(&coinInfo->locks->sessionsLock);
			for (auto iter = coinInfo->network->sessions.begin(); iter != coinInfo->network->sessions.end() &&
				!exit_flag && !coinInfo->network->stopSender;)
			{
				ConnectSocket = iter->Socket;

				BOOL l = TRUE;
				iResult = ioctlsocket(ConnectSocket, FIONBIO, (unsigned long*)&l);
				if (iResult == SOCKET_ERROR)
				{
					decreaseNetworkQuality(coinInfo);
					Log("\nSender %s: ! Error ioctlsocket's: %i", senderName, WSAGetLastError());
					bm_wattron(12);
					bm_wprintw("SENDER %s: ioctlsocket failed: %ld\n", senderName, WSAGetLastError(), 0);
					bm_wattroff(12);
					continue;
				}
				RtlSecureZeroMemory(buffer, buffer_size);
				size_t  pos = 0;
				iResult = 0;
				do {
					iResult = recv(ConnectSocket, &buffer[pos], (int)(buffer_size - pos - 1), 0);
					if (iResult > 0) pos += (size_t)iResult;
				} while (iResult > 0);

				if (iResult == SOCKET_ERROR)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK) //разрыв соединения, молча переотправляем дедлайн
					{
						decreaseNetworkQuality(coinInfo);
						//wattron(6);
						//wprintw("%s [%20llu] not confirmed DL %10llu\n", tbuffer, iter->body.account_id, iter->deadline, 0);
						//wattroff(6);
						Log("Sender %s: ! Error getting confirmation for DL: %llu  code: %i", senderName, iter->deadline, WSAGetLastError());
						iter = coinInfo->network->sessions.erase(iter);
						coinInfo->mining->shares.push_back({ iter->body.file_name, iter->body.account_id, iter->body.best, iter->body.nonce });
					}
				}
				else //что-то получили от сервера
				{
					increaseNetworkQuality(coinInfo);

					//получили пустую строку, переотправляем дедлайн
					if (buffer[0] == '\0')
					{
						Log("Sender %s: zero-length message for DL: %llu", senderName, iter->deadline);
						coinInfo->mining->shares.push_back({ iter->body.file_name, iter->body.account_id, iter->body.best, iter->body.nonce });
					}
					else //получили ответ пула
					{
						char *find = strstr(buffer, "{");
						if (find == nullptr)
						{
							find = strstr(buffer, "\r\n\r\n");
							if (find != nullptr) find = find + 4;
							else find = buffer;
						}

						unsigned long long ndeadline;
						unsigned long long naccountId = 0;
						unsigned long long ntargetDeadline = 0;
						rapidjson::Document answ;
						// burst.ninja        {"requestProcessingTime":0,"result":"success","block":216280,"deadline":304917,"deadlineString":"3 days, 12 hours, 41 mins, 57 secs","targetDeadline":304917}
						// pool.burst-team.us {"requestProcessingTime":0,"result":"success","block":227289,"deadline":867302,"deadlineString":"10 days, 55 mins, 2 secs","targetDeadline":867302}
						// proxy              {"result": "proxy","accountId": 17930413153828766298,"deadline": 1192922,"targetDeadline": 197503}
						if (!answ.Parse<0>(find).HasParseError())
						{
							if (answ.IsObject())
							{
								if (answ.HasMember("deadline")) {
									if (answ["deadline"].IsString())	ndeadline = _strtoui64(answ["deadline"].GetString(), 0, 10);
									else
										if (answ["deadline"].IsInt64()) ndeadline = answ["deadline"].GetInt64();
									Log("Sender %s: confirmed deadline: %llu", senderName, ndeadline);

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
									_strtime_s(tbuffer);
									bm_wattron(10);
									if ((naccountId != 0) && (ntargetDeadline != 0))
									{
										EnterCriticalSection(&coinInfo->locks->bestsLock);
										coinInfo->mining->bests[Get_index_acc(naccountId, coinInfo, targetDeadlineInfo)].targetDeadline = ntargetDeadline;
										LeaveCriticalSection(&coinInfo->locks->bestsLock);
										
										
										bm_wprintw("%s [%20llu|%-10s|Sender] DL confirmed : %s %sd %02llu:%02llu:%02llu\n", tbuffer, naccountId, senderName,
											toStr(ndeadline, 11).c_str(), toStr(days, 7).c_str(), hours, min, sec, 0);
										Log("[%20llu] %s confirmed DL: %10llu %5llud %02u:%02u:%02u", naccountId, senderName, ndeadline, days, hours, min, sec, 0);
										Log("[%20llu] %s set targetDL: %10llu", naccountId, senderName, ntargetDeadline);
										if (use_debug) {
											bm_wprintw("%s [%20llu|%-10s|Sender] Set target DL: %s\n", tbuffer, naccountId, toStr(ntargetDeadline, 11).c_str(), 0);
										}
									}
									else bm_wprintw("%s [%20llu|%-10s|Sender] DL confirmed : %s %sd %02llu:%02llu:%02llu\n", tbuffer, iter->body.account_id, senderName,
										toStr(ndeadline, 11).c_str(), toStr(days, 7).c_str(), hours, min, sec, 0);
									bm_wattroff(10);
									if (ndeadline < coinInfo->mining->deadline || coinInfo->mining->deadline == 0)  coinInfo->mining->deadline = ndeadline;

									if (ndeadline != iter->deadline)
									{
										const char* answString = docToString(answ).c_str();
										Log("Sender %s: Calculated and confirmed deadlines don't match. Fast block or corrupted file? Response: %s", senderName, answString);
										std::thread{ Csv_Fail, coinInfo->coin, coinInfo->mining->currentHeight, iter->body.file_name, coinInfo->mining->currentBaseTarget, 4398046511104 / 240 / coinInfo->mining->currentBaseTarget, iter->body.nonce, iter->deadline,
											ndeadline, answString }.detach();
										std::thread{ increaseConflictingDeadline, iter->body.file_name }.detach();
										bm_wattron(6);
										bm_wprintw("----Fast block or corrupted file?----\n%s sent deadline:\t%llu\nServer's deadline:\t%llu \n----\n", senderName, iter->deadline, ndeadline, 0); //shares[i].file_name.c_str());
										bm_wattroff(6);
									}
								}
								else {
									if (answ.HasMember("errorDescription")) {
										const char* answString = docToString(answ).c_str();
										Log("Sender %s: Deadline %llu sent with error: %s", senderName, iter->deadline, answString);
										if (iter->body.retryCount < 1) {
											std::thread{ Csv_Fail, coinInfo->coin, coinInfo->mining->currentHeight, iter->body.file_name, coinInfo->mining->currentBaseTarget, 4398046511104 / 240 / coinInfo->mining->currentBaseTarget, iter->body.nonce, iter->deadline,
												0, answString }.detach();
											std::thread{ increaseConflictingDeadline, iter->body.file_name }.detach();
										}
										if (iter->deadline <= targetDeadlineInfo && iter->body.retryCount < maxSubmissionRetries) {
											Log("Sender %s: Deadline should have been accepted (%llu <= %llu). Retry #%i.", senderName, iter->deadline, targetDeadlineInfo, iter->body.retryCount + 1);
											coinInfo->mining->shares.push_back({ iter->body.file_name, iter->body.account_id, iter->body.best, iter->body.nonce, iter->body.retryCount + 1 });
										}
										bm_wattron(15);
										bm_wprintw("[ERROR %i] %s: %s\n", answ["errorCode"].GetInt(), senderName, answ["errorDescription"].GetString(), 0);
										bm_wattroff(15);
										bm_wattron(12);
										if (answ["errorCode"].GetInt() == 1004) bm_wprintw("%s: You need change reward assignment and wait 4 blocks (~16 minutes)\n", senderName); //error 1004
										bm_wattroff(12);
									}
									else {
										bm_wattron(15);
										bm_wprintw("%s: %s\n", senderName, find);
										bm_wattroff(15);
									}
								}
							}
						}
						else
						{
							if (strstr(find, "Received share") != nullptr)
							{
								_strtime_s(tbuffer);
								coinInfo->mining->deadline = coinInfo->mining->bests[Get_index_acc(iter->body.account_id, coinInfo, targetDeadlineInfo)].DL; //может лучше iter->deadline ?
																						   // if(deadline > iter->deadline) deadline = iter->deadline;
								std::thread{ increaseMatchingDeadline, iter->body.file_name }.detach();
								bm_wattron(10);
								bm_wprintw("%s [%20llu|%-10s|Sender] DL confirmed : %s\n", tbuffer, iter->body.account_id, senderName, toStr(iter->deadline, 11).c_str(), 0);
								bm_wattroff(10);
							}
							else //получили нераспознанный ответ
							{
								int minor_version;
								int status = 0;
								const char *msg;
								size_t msg_len;
								struct phr_header headers[12];
								size_t num_headers = sizeof(headers) / sizeof(headers[0]);
								phr_parse_response(buffer, strlen(buffer), &minor_version, &status, &msg, &msg_len, headers, &num_headers, 0);

								if (status != 0)
								{
									bm_wattron(6);
									//wprintw("%s [%20llu] NOT confirmed DL %10llu\n", tbuffer, iter->body.account_id, iter->deadline, 0);
									std::string error_str(msg, msg_len);
									bm_wprintw("%s: Server error: %d %s\n", senderName, status, error_str.c_str());
									bm_wattroff(6);
									Log("Sender %s: server error for DL: %llu", senderName, iter->deadline);
									coinInfo->mining->shares.push_back({ iter->body.file_name, iter->body.account_id, iter->body.best, iter->body.nonce });
								}
								else //получили непонятно что
								{
									bm_wattron(7);
									bm_wprintw("%s: %s\n", senderName, buffer);
									bm_wattroff(7);
								}
							}
						}
					}
					iResult = closesocket(ConnectSocket);
					Log("Sender %s: Close socket. Code = %i", senderName, WSAGetLastError());
					iter = coinInfo->network->sessions.erase(iter);
				}
				if (iter != coinInfo->network->sessions.end()) ++iter;
			}
			LeaveCriticalSection(&coinInfo->locks->sessionsLock);
		}
		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(coinInfo->network->send_interval));
	}
	HeapFree(hHeap, 0, buffer);
	return;
}

void updater_i(std::shared_ptr<t_coin_info> coinInfo)
{
	const char* updaterName = coinNames[coinInfo->coin];
	if (coinInfo->network->updateraddr.length() <= 3) {
		Log("Updater %s: GMI: ERROR in UpdaterAddr", updaterName);
		exit(2);
	}
	for (; !exit_flag;) {
		if (pollLocal(coinInfo) && coinInfo->mining->enable && !coinInfo->mining->newMiningInfoReceived) {
			setnewMiningInfoReceived(coinInfo, true);
		}

		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(coinInfo->network->update_interval));
	}
}


bool pollLocal(std::shared_ptr<t_coin_info> coinInfo) {
	const char* updaterName = coinNames[coinInfo->coin];
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
		Log("*! GMI %s: getaddrinfo failed with error: %i", updaterName, WSAGetLastError());
	}
	else {
		UpdaterSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (UpdaterSocket == INVALID_SOCKET)
		{
			decreaseNetworkQuality(coinInfo);
			Log("*! GMI %s: socket function failed with error: %i", updaterName, WSAGetLastError());
		}
		else {
			const unsigned t = 1000;
			setsockopt(UpdaterSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&t, sizeof(unsigned));
			//Log("*Connecting to server: %s:%s", updateraddr.c_str(), updaterport.c_str());
			iResult = connect(UpdaterSocket, result->ai_addr, (int)result->ai_addrlen);
			if (iResult == SOCKET_ERROR) {
				decreaseNetworkQuality(coinInfo);
				Log("*! GMI %s: connect function failed with error: %i", updaterName, WSAGetLastError());
			}
			else {
				int bytes = sprintf_s(buffer, buffer_size, "POST /burst?requestType=getMiningInfo HTTP/1.0\r\nHost: %s:%s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", coinInfo->network->nodeaddr.c_str(), coinInfo->network->nodeport.c_str());
				iResult = send(UpdaterSocket, buffer, bytes, 0);
				if (iResult == SOCKET_ERROR)
				{
					decreaseNetworkQuality(coinInfo);
					Log("*! GMI %s: send request failed: %i", updaterName, WSAGetLastError());
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
						Log("*! GMI %s: get mining info failed:: %i", updaterName, WSAGetLastError());
					}
					else {
						increaseNetworkQuality(coinInfo);
						if (loggingConfig.logAllGetMiningInfos) {
							Log("* GMI %s: Received: %s", updaterName, Log_server(buffer));
						}

						// locate HTTP header
						char *find = strstr(buffer, "\r\n\r\n");
						if (find == nullptr)	Log("*! GMI %s: error message from pool", updaterName);
						else {
							rapidjson::Document gmi;
							if (loggingConfig.logAllGetMiningInfos && gmi.Parse<0>(find).HasParseError())
								Log("*! GMI %s: error parsing JSON message from pool", updaterName);
							else if (!loggingConfig.logAllGetMiningInfos && gmi.Parse<0>(find).HasParseError())
								Log("*! GMI %s: error parsing JSON message from pool: %s", updaterName, Log_server(buffer));
							else {
								if (gmi.IsObject())
								{
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
											Log("*! GMI %s: Node response: Error decoding generationsignature: %s", updaterName, Log_server(buffer));
										}
										else if (sigDiffer) {
											newBlock = true;
											setSignature(coinInfo, sig);
											if (!loggingConfig.logAllGetMiningInfos) {
												Log("* GMI %s: Received new mining info: %s", updaterName, Log_server(buffer));
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
										Log("%s: newTargetDeadlineInfo: %llu", updaterName, newTargetDeadlineInfo);
										Log("%s: my_target_deadline: %llu", updaterName, coinInfo->mining->my_target_deadline);
										if ((newTargetDeadlineInfo > 0) && (newTargetDeadlineInfo < coinInfo->mining->my_target_deadline)) {
											setTargetDeadlineInfo(coinInfo, newTargetDeadlineInfo);
											if (loggingConfig.logAllGetMiningInfos || newBlock) {
												Log("%s: Target deadline from pool is lower than deadline set in the configuration. Updating targetDeadline: %llu", updaterName, newTargetDeadlineInfo);
											}
										}
										else {
											setTargetDeadlineInfo(coinInfo, coinInfo->mining->my_target_deadline);
											if (loggingConfig.logAllGetMiningInfos || newBlock) {
												Log("%s: Using target deadline from configuration: %llu", updaterName, coinInfo->mining->my_target_deadline);
											}
										}
									}
								}
							}
						}
					}
				}
			}
			iResult = closesocket(UpdaterSocket);
		}
		freeaddrinfo(result);
	}
	HeapFree(hHeap, 0, buffer);
	return newBlock;
}

void ShowWinner(std::shared_ptr<t_coin_info> coinInfo, unsigned long long const num_block)
{
	char* generator = nullptr;
	char* generatorRS = nullptr;
	unsigned long long last_block_height = 0;
	char* name = nullptr;
	char* rewardRecipient = nullptr;
	char* pool_accountRS = nullptr;
	char* pool_name = nullptr;
	unsigned long long timestamp0 = 0;
	unsigned long long timestamp1 = 0;
	char tbuffer[9];
	char* json;
	char* str_req;



	//get second to last block
	str_req = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, MAX_PATH);
	if (str_req == nullptr) ShowMemErrorExit();

	sprintf_s(str_req, HeapSize(hHeap, 0, str_req), "POST /burst?requestType=getBlock&height=%llu HTTP/1.0\r\nConnection: close\r\n\r\n", num_block - 1);
	json = GetJSON(coinInfo, str_req);
	//Log(" getBlocks: ");

	if (json == nullptr)	Log("-! error in message from pool (getBlocks)");
	else
	{
		rapidjson::Document doc_block;
		if (doc_block.Parse<0>(json).HasParseError() == false)
		{
			timestamp1 = doc_block["timestamp"].GetUint64();
		}
		else {
			Log("- error parsing JSON getBlocks.\nRequest: %s\nResponse: %s", str_req, json);
		}
	}
	HeapFree(hHeap, 0, str_req);
	if (json != nullptr) HeapFree(hHeap, 0, json);

	str_req = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, MAX_PATH);
	if (str_req == nullptr) ShowMemErrorExit();

	//get last block
	sprintf_s(str_req, HeapSize(hHeap, 0, str_req), "POST /burst?requestType=getBlock&height=%llu HTTP/1.0\r\nConnection: close\r\n\r\n", num_block);

	//try several times
	for (int attempt = 0;attempt < 10;attempt++) {

		//delay
		Sleep(1000);
		json = GetJSON(coinInfo, str_req);
		//Log(" getBlocks: ");

		if (json == nullptr)	Log("-! error in message from pool (getBlocks)");
		else
		{
			rapidjson::Document doc_block;
			if (doc_block.Parse<0>(json).HasParseError() == false)
			{
				//if has errorcode = wallet too slow
				if (!doc_block.HasMember("errorCode"))
				{

					generatorRS = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, strlen(doc_block["generatorRS"].GetString()) + 1);
					if (generatorRS == nullptr) ShowMemErrorExit();
					strcpy_s(generatorRS, HeapSize(hHeap, 0, generatorRS), doc_block["generatorRS"].GetString());
					generator = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, strlen(doc_block["generator"].GetString()) + 1);
					if (generator == nullptr) ShowMemErrorExit();
					strcpy_s(generator, HeapSize(hHeap, 0, generator), doc_block["generator"].GetString());
					last_block_height = doc_block["height"].GetUint();
					timestamp0 = doc_block["timestamp"].GetUint64();
					break;
				}
				else
				{
					_strtime_s(tbuffer);
					bm_wattron(11);
					bm_wprintw("%s Winner: wallet has no winner info yet...\n", tbuffer, 0);
					bm_wattroff(11);
				}
			}
			else {
				Log("- error parsing JSON getBlocks: %s", json);
				Log("- error parsing JSON getBlocks.\nRequest: %s\nResponse: %s", str_req, json);
			}
		}
	}
	HeapFree(hHeap, 0, str_req);
	if (json != nullptr) HeapFree(hHeap, 0, json);



	if ((generator != nullptr) && (generatorRS != nullptr) && (timestamp0 != 0) && (timestamp1 != 0))
		if (last_block_height == coinInfo->mining->height - 1)
		{
			// Запрос данных аккаунта
			str_req = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, MAX_PATH);
			if (str_req == nullptr) ShowMemErrorExit();
			sprintf_s(str_req, HeapSize(hHeap, 0, str_req), "POST /burst?requestType=getAccount&account=%s HTTP/1.0\r\nConnection: close\r\n\r\n", generator);
			json = GetJSON(coinInfo, str_req);
			//Log(" getAccount: ");

			if (json == nullptr)	Log("- error in message from pool (getAccount)");
			else
			{
				rapidjson::Document doc_acc;
				if (doc_acc.Parse<0>(json).HasParseError() == false)
				{
					if (doc_acc.HasMember("name"))
					{
						name = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, strlen(doc_acc["name"].GetString()) + 1);
						if (name == nullptr) ShowMemErrorExit();
						strcpy_s(name, HeapSize(hHeap, 0, name), doc_acc["name"].GetString());
					}
				}
				else Log("- error parsing JSON getAccount");
			}
			HeapFree(hHeap, 0, str_req);
			if (json != nullptr) HeapFree(hHeap, 0, json);

			// Запрос RewardAssighnment по данному аккаунту
			str_req = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, MAX_PATH);
			if (str_req == nullptr) ShowMemErrorExit();
			sprintf_s(str_req, HeapSize(hHeap, 0, str_req), "POST /burst?requestType=getRewardRecipient&account=%s HTTP/1.0\r\nConnection: close\r\n\r\n", generator);
			json = GetJSON(coinInfo, str_req);
			HeapFree(hHeap, 0, str_req);
			//Log(" getRewardRecipient: ");

			if (json == nullptr)	Log("- error in message from pool (getRewardRecipient)");
			else
			{
				rapidjson::Document doc_reward;
				if (doc_reward.Parse<0>(json).HasParseError() == false)
				{
					if (doc_reward.HasMember("rewardRecipient"))
					{
						rewardRecipient = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, strlen(doc_reward["rewardRecipient"].GetString()) + 1);
						if (rewardRecipient == nullptr) ShowMemErrorExit();
						strcpy_s(rewardRecipient, HeapSize(hHeap, 0, rewardRecipient), doc_reward["rewardRecipient"].GetString());
					}
				}
				else Log("-! error parsing JSON getRewardRecipient");
			}

			if (json != nullptr) HeapFree(hHeap, 0, json);

			if (rewardRecipient != nullptr)
			{
				// Если rewardRecipient != generator, то майнит на пул, узнаём имя пула...
				if (strcmp(generator, rewardRecipient) != 0)
				{
					// Запрос данных аккаунта пула
					str_req = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, MAX_PATH);
					if (str_req == nullptr) ShowMemErrorExit();
					sprintf_s(str_req, HeapSize(hHeap, 0, str_req), "POST /burst?requestType=getAccount&account=%s HTTP/1.0\r\nConnection: close\r\n\r\n", rewardRecipient);
					json = GetJSON(coinInfo, str_req);
					//Log(" getAccount: ");

					if (json == nullptr)
					{
						Log("- error in message from pool (pool getAccount)");
					}
					else
					{
						rapidjson::Document doc_pool;
						if (doc_pool.Parse<0>(json).HasParseError() == false)
						{
							pool_accountRS = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, strlen(doc_pool["accountRS"].GetString()) + 1);
							if (pool_accountRS == nullptr) ShowMemErrorExit();
							strcpy_s(pool_accountRS, HeapSize(hHeap, 0, pool_accountRS), doc_pool["accountRS"].GetString());
							if (doc_pool.HasMember("name"))
							{
								pool_name = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, strlen(doc_pool["name"].GetString()) + 1);
								if (pool_name == nullptr) ShowMemErrorExit();
								strcpy_s(pool_name, HeapSize(hHeap, 0, pool_name), doc_pool["name"].GetString());
							}
						}
						else Log("- error parsing JSON pool getAccount");
					}
					HeapFree(hHeap, 0, str_req);
					HeapFree(hHeap, 0, json);
				}
			}

			bm_wattron(11);
			_strtime_s(tbuffer);
			if (name != nullptr) bm_wprintw("%s Winner: %llus by %s (%s)\n", tbuffer, timestamp0 - timestamp1, generatorRS + 6, name, 0);
			else bm_wprintw("%s Winner: %llus by %s\n", tbuffer, timestamp0 - timestamp1, generatorRS + 6, 0);
			if (pool_accountRS != nullptr)
			{
				if (pool_name != nullptr) bm_wprintw("%s Winner's pool: %s (%s)\n", tbuffer, pool_accountRS + 6, pool_name, 0);
				else bm_wprintw("%s Winner's pool: %s\n", tbuffer, pool_accountRS + 6, 0);
			}
			bm_wattroff(11);
		}

	HeapFree(hHeap, 0, generatorRS);
	HeapFree(hHeap, 0, generator);
	HeapFree(hHeap, 0, name);
	HeapFree(hHeap, 0, rewardRecipient);
	HeapFree(hHeap, 0, pool_name);
	HeapFree(hHeap, 0, pool_accountRS);
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