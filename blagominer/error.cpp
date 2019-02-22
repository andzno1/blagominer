#include "stdafx.h"
#include "error.h"

void ShowMemErrorExit(void)
{
	Log("!!! Error allocating memory");
	printToConsole(12, false, true, true, false, "Error allocating memory");
	system("pause > nul");
	exit(-1);
}