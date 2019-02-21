#pragma once
#include "stdafx.h"
#include "logger.h"
#include "loggerCsv.h"
#include "error.h"
#include "accounts.h"
#include "common.h"
#include "filemonitor.h"


#pragma comment(lib,"Ws2_32.lib")

#include "picohttpparser.h"

extern std::map <u_long, unsigned long long> satellite_size; // Структура с объемами плотов сателлитов

void init_network_info();

bool pollLocal(std::shared_ptr<t_coin_info> coinInfo);

void hostname_to_ip(char const *const  in_addr, char* out_addr);
void updater_i(std::shared_ptr<t_coin_info> coinInfo);
void proxy_i(std::shared_ptr<t_coin_info> coinInfo);
void send_i(std::shared_ptr<t_coin_info> coinInfo);
void confirm_i(std::shared_ptr<t_coin_info> coinInfo);
size_t xstr2strr(char *buf, size_t const bufsize, const char *const in);
