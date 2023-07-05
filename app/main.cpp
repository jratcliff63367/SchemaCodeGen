// Implements of the console CreateDOM application
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <string.h>
#include <stdint.h>

#include "CreateDOM.h"

enum class ExportType : uint64_t
{
    cpp = (1<<0),
    python = (1<<1),
    typescript = (1<<2),
    json = (1<<3),
    protobuf = (1<<4)
};

int main(int argc, const char **argv)
{
	if (argc < 3)
	{
		printf("Usage: CreateDOM <fname.csv> <destDirectory> (options)\n");
        printf("Options:\n");
        printf(" -cpp : Export as C++ code\n");
        printf(" -python : Export as Python script.\n");
        printf(" -typescript : Export as Typescript\n");
        printf(" -json : Export as JSON spec.\n");
        printf(" -protobuf : Export as protobuf\n");
        printf("\n");
        printf("Only -cpp and -typescript are known to work at this time.\n");
        printf("Other formats are legacy and haven't been actively maintained.\n");
		printf("\n");
	}
	else
	{
		const char *csv = argv[1];
        const char *destDir = argv[2];
        uint64_t exportFlags = uint64_t(ExportType::cpp); // default is C++
        if ( argc > 3 )
        {
            exportFlags = 0;
            for (uint64_t i=3; i<argc; i++)
            {
                const char *option = argv[i];
                if ( strcmp(option,"-cpp") == 0 )
                {
                    exportFlags|=uint64_t(ExportType::cpp);
                }
                else if (strcmp(option, "-python") == 0)
                {
                    exportFlags |= uint64_t(ExportType::python);
                }
                else if (strcmp(option, "-typescript") == 0)
                {
                    exportFlags |= uint64_t(ExportType::typescript);
                }
                else if (strcmp(option, "-json") == 0)
                {
                    exportFlags |= uint64_t(ExportType::json);
                }
                else if (strcmp(option, "-protobuf") == 0)
                {
                    exportFlags |= uint64_t(ExportType::protobuf);
                }
            }
        }
		CREATE_DOM::CreateDOM *cdom = CREATE_DOM::CreateDOM::create(destDir);
		cdom->parseCSV(csv);
        if ( (exportFlags & uint64_t(ExportType::cpp)) || (exportFlags & uint64_t(ExportType::typescript)))
        {
            bool exportCPP = (exportFlags & uint64_t(ExportType::cpp)) ? true : false;
            bool exportTypeScript = ( exportFlags & uint64_t(ExportType::typescript)) ? true : false;
		    cdom->saveCPP(exportCPP, exportTypeScript);
        }
        if (exportFlags & uint64_t(ExportType::python))
        {
		    cdom->savePython();
        }
        if (exportFlags & uint64_t(ExportType::json))
        {
    		cdom->saveJSON();
        }
        if (exportFlags & uint64_t(ExportType::protobuf))
        {
    		cdom->savePROTOBUF();
        }
		cdom->release();
	}
}

