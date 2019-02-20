#include "stdafx.h"
#include "error.h"

void ShowMemErrorExit(void)
{
	Log("!!! Error allocating memory");
	printToConsole(MAIN, 12, false, true, false, "Error allocating memory\n");
	system("pause > nul");
	exit(-1);
}