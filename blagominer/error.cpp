#include "stdafx.h"
#include "error.h"

void ShowMemErrorExit(void)
{
	Log(L"!!! Error allocating memory");
	printToConsole(12, false, true, true, false, L"Error allocating memory");
	system("pause > nul");
	exit(-1);
}