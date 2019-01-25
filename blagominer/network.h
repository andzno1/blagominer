#pragma once
#include "stdafx.h"
#include "logger.h"
#include "error.h"
#include "accounts.h"
#include "common.h"
#include "filemonitor.h"


#pragma comment(lib,"Ws2_32.lib")

#include "picohttpparser.h"

extern std::map <u_long, unsigned long long> satellite_size; // Структура с объемами плотов сателлитов

extern char str_signature[65];

const int maxSubmissionRetries = 3;

extern std::thread showWinnerBurst;

void init_network_info();

char* GetJSON(std::shared_ptr<t_coin_info> coinInfo, char const *const req);

void ShowWinner(std::shared_ptr<t_coin_info> coinInfo, unsigned long long const num_block);
bool pollLocal(std::shared_ptr<t_coin_info> coinInfo);

void hostname_to_ip(char const *const  in_addr, char* out_addr);
void updater_i(std::shared_ptr<t_coin_info> coinInfo);
void proxy_i(std::shared_ptr<t_coin_info> coinInfo);
void send_i(std::shared_ptr<t_coin_info> coinInfo, const unsigned long long currentHeight,
	const unsigned long long currentBaseTarget);
size_t xstr2strr(char *buf, size_t const bufsize, const char *const in);
