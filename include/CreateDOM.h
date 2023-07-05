#ifndef CREATE_DOM_H
#define CREATE_DOM_H

// Declares the public interface for the CreateDOM class.
// This tool will parse a CSV file representing a spreadsheet which defines a 
// DOM.  Once parsed, it will auto-generate C++ code, a protobufs schema, and
// python bindings.

namespace CREATE_DOM
{

class CreateDOM
{
public:
	static CreateDOM *create(const char *destDir);

	// Parses this XML and accumulates all of the unique element and attribute names
	virtual void parseCSV(const char *xmlName) = 0;

	// Save the DOM as C++ code
	virtual void saveCPP(bool saveCPP,bool saveTypeScript) = 0;

	// Save the DOM as Python code
	virtual void savePython(void) = 0;

	// Save the DOM as a protobuf file
	virtual void savePROTOBUF(void) = 0;

	// Save the DOM as a JSON schema
	virtual void saveJSON(void) = 0;

	virtual void release(void) = 0;
};

}

#endif
