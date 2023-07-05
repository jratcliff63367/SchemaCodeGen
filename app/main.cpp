// Implements of the console CreateDOM application
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <string.h>
#include <stdint.h>

#include "CreateDOM.h"

int main(int argc, const char **argv)
{
	if (argc < 3)
	{
		printf("Usage: SchemaCodeGen <fname.csv> <destDirectory>\n");
        printf("Options:\n");
		printf("\n");
	}
	else
	{
		const char *csv = argv[1];
        const char *destDir = argv[2];
		CREATE_DOM::CreateDOM *cdom = CREATE_DOM::CreateDOM::create(destDir);
		cdom->parseCSV(csv);
		cdom->saveCPP(true,false);
		cdom->release();
	}
}

