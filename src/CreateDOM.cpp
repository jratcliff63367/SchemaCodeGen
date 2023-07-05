// Implements the CreateDOM class.
// This tool will parse a CSV file representing a spreadsheet which defines a 
// DOM.  Once parsed, it will auto-generate C++ code, a protobufs schema, and
// python bindings.

#include "CreateDOM.h"
#include "StringHelper.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <stdarg.h>
#include <unordered_map>
#include <map>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <sstream>
#include <stack>
#include <cfloat>

#define EXPORT_COMMAND_CODE 0

#ifdef _MSC_VER
#include <direct.h>
#pragma warning(disable:4996 4100 4189)
#else
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#endif

#ifndef _MSC_VER
// cross-platform compatibility stuff
inline int _stricmp(const char *a, const char *b)
{
	return strcasecmp(a, b);
}
char *_strupr(char *s)
{
	while (*s)
	{
		*s = toupper(*s);
		++s;
	}
	return s;
}
#endif

#define MAX_DIR_PATH 512

bool createDir(const char* pathName)
{
    bool ret = false;

#ifdef _MSC_VER
    int err = _mkdir(pathName);
    ret = err ? false : true;
#else
    const int dir_err = mkdir(pathName, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (-1 == dir_err)
    {
        ret = false;
    }
    else
    {
        ret = true;
    }
#endif

    return ret;
}

static bool isSlash(char c)
{
    return c == '/' || c == '\\';
}

void recursiveCreatePath(const char* dirName)
{
    char mpath[MAX_DIR_PATH];
    char* eos = &mpath[MAX_DIR_PATH - 1];
    char* dest = mpath;
    while (*dirName && dest < eos)
    {
        if (isSlash(*dirName))
        {
            *dest = 0;
            createDir(mpath);
        }
        *dest++ = *dirName++;
    }
    *dest = 0;
    createDir(mpath);
}

std::string fpout(const char *fname,const char *nspace,const char *destDir)
{
    recursiveCreatePath(destDir);
    std::string fqn = std::string(destDir) + "/" + fname;
    return fqn;
}

#pragma warning(disable:4100)

namespace CREATE_DOM
{
	class DOM;
	class Object;
	class MemberVariable;

    enum class OptionalType
    {
        required,           // Required item
        optional,           // Optional and use the std::optional type
        optional_deserialize // Optional for deserialization, but don't use std::optional type
    };

    enum class StandardType
    {
        none,
        u64,
        u32,
        u16,
        u8,
        i64,
        i32,
        i16,
        i8,
        float_type,
        bool_type,
        string_type,
    };

    const char *deserializeTypeName(StandardType type,bool &isNumeric)
    {
        const char *ret = nullptr;

        isNumeric = true;
        switch (type)
        {
            case CREATE_DOM::StandardType::none:
                break;
            case CREATE_DOM::StandardType::u64:
                ret = "GetUint64";
                break;
            case CREATE_DOM::StandardType::u32:
                ret = "GetUint32";
                break;
            case CREATE_DOM::StandardType::u16:
                ret = "GetUint16";
                break;
            case CREATE_DOM::StandardType::u8:
                ret = "GetUint8";
                break;
            case CREATE_DOM::StandardType::i64:
                ret = "GetInt64";
                break;
            case CREATE_DOM::StandardType::i32:
                ret = "GetInt32";
                break;
            case CREATE_DOM::StandardType::i16:
                ret = "GetInt16";
                break;
            case CREATE_DOM::StandardType::i8:
                ret = "GetInt8";
                break;
            case CREATE_DOM::StandardType::float_type:
                ret = "GetFloat";
                isNumeric = false;
                break;
            case CREATE_DOM::StandardType::bool_type:
                ret = "GetBool";
                isNumeric = false;
                break;
            case CREATE_DOM::StandardType::string_type:
                ret = "GetString";
                isNumeric = false;
                break;
            default:
                break;
        }

        return ret;
    }

    StandardType getStandardType(const char *type)
    {
        StandardType ret = StandardType::none;

        if (strcmp(type, "u64") == 0 )
        {
            ret = StandardType::u64;
        }
        else if ( strcmp(type, "u32") == 0 )
        {
            ret = StandardType::u32;
        }
        else if ( strcmp(type, "u16") == 0 )
        {
            ret = StandardType::u16;
        }
        else if ( strcmp(type, "u8") == 0 )
        {
            ret = StandardType::u8;
        }
        else if ( strcmp(type, "i64") == 0 )
        {
            ret = StandardType::i64;
        }
        else if ( strcmp(type, "i32") == 0 )
        {
            ret = StandardType::i32;
        }
        else if ( strcmp(type, "i16") == 0 )
        {
            ret = StandardType::i16;
        }
        else if ( strcmp(type, "i8") == 0 )
        {
            ret = StandardType::i8;
        }
        else if ( strcmp(type, "float") == 0 )
        {
            ret = StandardType::float_type;
        }
        else if ( strcmp(type, "bool") == 0)
        {
            ret = StandardType::bool_type;
        }
        else if (strcmp(type, "string") == 0)
        {
            ret = StandardType::string_type;
        }


        return ret;
    }

    typedef std::unordered_map< std::string, bool > ClassEnumMap;

    struct OmniCommandInstance
    {
        std::string mCommand;
        std::string mCommandType;
    };

    bool getBool(const char *str)
    {
        bool ret = false;

        int v = atoi(str);
        if ( v )
        {
            ret = true;
        }
        else if ( _stricmp(str,"true") == 0 )
        {
            ret = true;
        }

        return ret;
    }

    typedef std::vector< OmniCommandInstance > OmniCommandInstanceVector;

	static std::string getPythonArgDef(const MemberVariable &var, const DOM &dom);
	static std::string getCppValueInitializer(const MemberVariable &var, const DOM &dom, bool isDef);
	static std::string getCppRValue(const MemberVariable &var, const DOM &dom, bool isDef);

	static Object* typeInfo(const DOM& dom, const std::string& typeName);
	static bool isEnumType(const DOM& dom, const std::string& typeName);
	static bool isClassType(const DOM& dom, const std::string& typeName);

	char lowerCase(char c)
	{
		if (c >= 'A' && c <= 'Z')
		{
			c += 32;
		}
		return c;
	}

	class CodePrinter
	{
	public:
		CodePrinter(void)
		{
		}
		CodePrinter(const std::string &destFileName)
		{
            if ( destFileName.size() )
            {
                mDestFileName = destFileName;
                printf("Generating output for file: (%s)\n", destFileName.c_str());
            }
		}

        ~CodePrinter(void)
        {
            assert( mFinalized);
        }

        void linefeed(void)
        {
            printCode(0,"\n");
        }

		void printCode(uint32_t indent, const char *fmt, ...)
		{
			va_list         args;
			char            buffer[4096];
			va_start(args, fmt);
			STRING_HELPER::stringFormatV(buffer, sizeof(buffer), fmt, args);
			va_end(args);
			if (indent)
			{
				size_t currentPos = doTell() - mLastLineFeed;
				size_t indentLocation = indent * 4;	// This is the column we want to be on..
				if (currentPos < indentLocation)
				{
					// How many spaces do we need to get to the next tab location
					uint32_t nextTab = uint32_t(currentPos % 4);
					for (size_t i = 0; i < nextTab; i++)
					{
						doPrintf(" ");
					}
					currentPos += nextTab;	// Ok, advance the current positions
					if (currentPos < indentLocation)
					{
						uint32_t indentCount = uint32_t(indentLocation - currentPos);
						uint32_t tabCount = indentCount / 4;
						for (size_t i = 0; i < tabCount; i++)
						{
							doPrintf("    ");
						}
					}
				}
				else
				{
					doPrintf(" ");
				}
			}
			doPrintf("%s", buffer);
			const char *scan = buffer;
			while (*scan)
			{
				if (*scan == 10 || *scan == 10)
				{
					mLastLineFeed = doTell();
					break;
				}
				scan++;
			}

		}

        size_t doTell(void)
        {
            return mOutput.size();
        }

        void doPrintf(const char *fmt,...)
        {
            char scratch[8192];
            va_list arg;
            va_start(arg, fmt);
            int32_t r = ::vsnprintf(scratch, sizeof(scratch), fmt, arg);
            va_end(arg);
            mOutput+=std::string(scratch);
        }

        void finalize(void)
        {
            mFinalized = true;
            if ( !mDestFileName.size() )
            {
                return;
            }
            if ( mOutput.size() )
            {
                FILE *fph = fopen(mDestFileName.c_str(),"rb");
                bool needSave = true;
                if ( fph )
                {
                    printf("Reading old file '%s' to see if it has changed.\n", mDestFileName.c_str());
                    fseek(fph,0L,SEEK_END);
                    size_t flen = ftell(fph);
                    fseek(fph,0L,SEEK_SET);
                    if ( flen == mOutput.size() )
                    {
                        char *temp = (char *)malloc(mOutput.size());
                        size_t r = fread(temp,mOutput.size(),1,fph);
                        if ( r == 1 )
                        {
                            if ( memcmp(temp,mOutput.c_str(),mOutput.size()) == 0 )
                            {
                                needSave = false;
                                printf("File has not changed, so not saving a new copy.\n");
                            }
                            else
                            {
                                printf("File is the same size, but has different content, so saving.\n");
                            }
                        }
                        else
                        {
                        }
                        free(temp);
                    }
                    else
                    {
                        // File is a different size, so needs to be saved..
                        printf("File is a different size, so we are saving out the new version.\n");
                    }
                    fclose(fph);
                }

                if ( needSave )
                {
                    fph = fopen(mDestFileName.c_str(),"wb");
                    if ( fph )
                    {
                        printf("Saving output file (%s) which is %lld bytes long.\n", mDestFileName.c_str(), (long long)mOutput.size());
                        fwrite(mOutput.c_str(),mOutput.size(),1,fph);
                        fclose(fph);
                    }
                    else
                    {
                        printf("** ERROR ** Failed to open output file: (%s)\n", mDestFileName.c_str());
                    }
                }
            }
        }

		size_t	mLastLineFeed{ 0 };
        std::string mOutput;
        std::string mDestFileName;
        bool        mFinalized{false};
};

typedef std::vector< std::string > StringVector;


class MemberVariable
{
public:
    MemberVariable(void)
    {
    }
	void memberNeedsReflection(const StringVector &needsReflectionClasses)
	{
		mNeedsReflection = false;
		if (mIsPointer || mIsString)
		{
			mNeedsReflection = true;
		}
		else
		{
			for (auto &i : needsReflectionClasses)
			{
				if (i == mType)
				{
					mNeedsReflection = true;
					break;
				}
			}
		}
	}
	bool needsReflection(void) const
	{
		bool ret = false;

		if (mIsArray || mIsPointer || mIsString)
		{
			ret = true;
		}

		return ret;
	}

	void init(void)
	{
		if (!mDefaultValue.empty())
		{
			char scratch[512];
			STRING_HELPER::stringFormat(scratch, 512, "%s::%s", mType.c_str(), mDefaultValue.c_str());
			mQualifiedDefaultValue = std::string(scratch);
		}
	}

	bool			mIsArray{ false }; // true if this data item is an array
	bool			mIsPointer{ false };
	bool			mIsString{ false };
	bool			mNeedsReflection{ false }; // determines where or not this refers to a data type that needs reflection
    OptionalType    mIsOptional{OptionalType::required};
    bool            mIsMap{false};
    bool            mSerializeEnumAsInteger{false};
    std::string     mMapType;
	std::string		mMember;	// name of this data item
    std::string     mAlias;
	std::string		mType;	// Type of data item
	std::string		mInheritsFrom;
	std::string		mProtoType;	// Special case data type for protobufs specification only
	std::string		mEngineSpecific;
	std::string		mDefaultValue;
	std::string		mMinValue;
	std::string		mMaxValue;
	std::string		mShortDescription;
	std::string		mLongDescription;
	std::string		mQualifiedDefaultValue;
};

typedef std::vector< MemberVariable > MemberVariableVector;

const char *getCppTypeString(const char *type,bool isDef)
{
	const char *ret = type;

	if (strcmp(type, "string") == 0)
	{
		if (isDef)
		{
			ret = "std::string";
		}
		else
		{
			ret = "const char *";
		}
	}
	else if (strcmp(type, "u8") == 0)
	{
		ret = "uint8_t";
	}
	else if (strcmp(type, "u16") == 0)
	{
		ret = "uint16_t";
	}
	else if (strcmp(type, "u32") == 0)
	{
		ret = "uint32_t";
	}
	else if (strcmp(type, "u64") == 0)
	{
		ret = "uint64_t";
	}
	else if (strcmp(type, "i8") == 0)
	{
		ret = "int8_t";
	}
	else if (strcmp(type, "i16") == 0)
	{
		ret = "int16_t";
	}
	else if (strcmp(type, "i32") == 0)
	{
		ret = "int32_t";
	}
	else if (strcmp(type, "i64") == 0)
	{
		ret = "int64_t";
	}

	return ret;
}

char upcase(char c)
{
	if (c >= 'a' && c <= 'z')
	{
		c -= 32;
	}
	return c;
}

class Object
{
public:
	void clear(void)
	{
		Object c;
		*this = c;
	}

	const char *getPROTOTypeString(const char *type)
	{
		const char *ret = type;

		if (strcmp(type, "string") == 0)
		{
			ret = "string";
		}
		else if (strcmp(type, "u8") == 0)
		{
			ret = "uint32";
		}
		else if (strcmp(type, "u16") == 0)
		{
			ret = "uint32";
		}
		else if (strcmp(type, "u32") == 0)
		{
			ret = "uint32";
		}
		else if (strcmp(type, "u64") == 0)
		{
			ret = "uint64";
		}
		else if (strcmp(type, "i8") == 0)
		{
			ret = "int32";
		}
		else if (strcmp(type, "i16") == 0)
		{
			ret = "int32";
		}
		else if (strcmp(type, "i32") == 0)
		{
			ret = "int32";
		}
		else if (strcmp(type, "i64") == 0)
		{
			ret = "int64";
		}

		return ret;
	}

	const char *getClassNameString(const std::string &name,bool isDef)
	{
		const char *ret = name.c_str();
		static char scratch[512];
		if (isDef)
		{
			STRING_HELPER::stringFormat(scratch, 512, "%sDef", name.c_str());
			ret = scratch;
		}
		return ret;
	}

	const char *getMemberName(const std::string &name, bool isDef,bool isMap)
	{
		const char *ret = name.c_str();
		static char scratch[512];
        static char mapName[512];
		if (isDef)
		{
			char temp[512];
			strncpy(temp, name.c_str(), 512);
			temp[0] = upcase(temp[0]); // upper case the first character of the member variable name
			STRING_HELPER::stringFormat(scratch, 512, "m%s", temp);
			ret = scratch;
		}
        else if ( isMap )
        {
            snprintf(mapName,sizeof(mapName), "_%s", ret );
            ret = mapName;
        }
		return ret;
	}

	bool classNeedsReflection(const std::string &className, const StringVector &needsReflection)
	{
		bool ret = false;

		for (auto &i : needsReflection)
		{
			if (i == className)
			{
				ret = true;
				break;
			}
		}
		return ret;
	}

    void saveOmniCommand(const char *baseName,const char *commandType,OmniCommandInstanceVector &instances,const char *nspace,const char *destDir)
    {

        OmniCommandInstance oci;
        oci.mCommand = std::string(baseName);
        oci.mCommandType = std::string(commandType);
        instances.push_back(oci);

        std::string commandName;
        std::string responseName;
        const char *scan = baseName;
        while ( *scan )
        {
            if ( strncmp(scan,"Command",7) == 0 )
            {
                scan+=7;
                commandName+="Command";
                responseName+="Response";
            }
            else
            {
                commandName+=*scan;
                responseName+=*scan;
                scan++;
            }
        }
        std::string cppName = "OmniApi" + commandName + ".cpp";
        std::string headerName = "OmniApi" + commandName+".h";

        std::string fphCPP = fpout(cppName.c_str(),nspace,destDir);
        CodePrinter cpp(fphCPP);
        {
            cpp.printCode(0, "#include \"OmniApi.h\"\n");
            cpp.printCode(0, "#include \"omniverse_api.h\"\n");
            cpp.printCode(0, "#include \"ApiConnection.h\"\n");
            cpp.printCode(0, "#include \"TimeStamp.h\"\n");
            cpp.printCode(0, "#include \"UserAllocated.h\"\n");
            cpp.printCode(0, "#include \"RapidJSONDocument.h\"\n");
            cpp.printCode(0, "#include \"useraccounts/UserAccounts.h\"\n");
            cpp.printCode(0, "\n");
            cpp.printCode(0, "namespace omniapi\n");
            cpp.printCode(0, "{\n");
            cpp.printCode(0, "\n");
            cpp.printCode(0, "\n");
            cpp.printCode(0, "\n");
            cpp.printCode(0, "class OmniApiCommand%s : public OmniApiCommand, public %s, public userallocated::UserAllocated\n", commandName.c_str(), responseName.c_str());
            cpp.printCode(0, "{\n");
            cpp.printCode(0, "public:\n");
            cpp.printCode(0, "    OmniApiCommand%s(const rapidjson::RapidJSONDocument &d,OmniConnection *oc)\n", commandName.c_str());
            cpp.printCode(0, "    {\n");
            cpp.printCode(0, "        %s p;\n", commandName.c_str());
            cpp.printCode(0, "        deserializeFrom<rapidjson::Document>(d,p);\n");
            cpp.printCode(0, "        %s::id = p.id;\n", responseName.c_str());
            cpp.printCode(0, "    }\n");
            cpp.printCode(0, "\n");
            cpp.printCode(0, "    virtual uint64_t getId(void) const final\n");
            cpp.printCode(0, "    {\n");
            cpp.printCode(0, "        return %s::id;\n", responseName.c_str());
            cpp.printCode(0, "    }\n");
            cpp.printCode(0, "\n");
            cpp.printCode(0, "    virtual CommandType getCommandType(void) const final\n");
            cpp.printCode(0, "    {\n");
            cpp.printCode(0, "        return CommandType::%s;\n", commandType);
            cpp.printCode(0, "    }\n");
            cpp.printCode(0, "\n");
            cpp.printCode(0, "    virtual void process(Callback *c) final\n");
            cpp.printCode(0, "    {\n");
            cpp.printCode(0, "        // Get the time stamp on the server at the time this response went out\n");
            cpp.printCode(0, "        ts.omni_server_out_ts = timestamp::getTimeStamp();\n");
            cpp.printCode(0, "        // Set the status response string to 'OK'\n");
            cpp.printCode(0, "        status = std::string(omniapi::stringifyEnum(omniapi::StatusType::OK));\n");
            cpp.printCode(0, "        // Serialize the JSON response\n");
            cpp.printCode(0, "        std::string response = serialize(*this);\n");
            cpp.printCode(0, "        // Send the response back to the caller\n");
            cpp.printCode(0, "        c->sendResponse(this,response.c_str(),response.size(), nullptr,0,true);\n");
            cpp.printCode(0, "    }\n");
            cpp.printCode(0, "\n");
            cpp.printCode(0, "    virtual void release(void)\n");
            cpp.printCode(0, "    {\n");
            cpp.printCode(0, "        delete this;\n");
            cpp.printCode(0, "    }\n");
            cpp.printCode(0, "\n");
            cpp.printCode(0, "};\n");
            cpp.printCode(0, "\n");
            cpp.printCode(0, "OmniApiCommand *create%sInstance(const rapidjson::RapidJSONDocument &d,OmniConnection *oc)\n", commandName.c_str());
            cpp.printCode(0, "{\n");
            cpp.printCode(0, "    OmniApiCommand%s *ret = UA_NEW(OmniApiCommand%s)(d,oc);\n", commandName.c_str(), commandName.c_str());
            cpp.printCode(0, "    if ( !ret->getId() )\n");
            cpp.printCode(0, "    {\n");
            cpp.printCode(0, "        ret->release();\n");
            cpp.printCode(0, "        ret = nullptr;\n");
            cpp.printCode(0, "    }\n");
            cpp.printCode(0, "    return static_cast< OmniApiCommand *>(ret);\n");
            cpp.printCode(0, "}\n");
            cpp.printCode(0, "\n");
            cpp.printCode(0, "}\n");
        }
        cpp.finalize();
    }

    bool isStandardType(const char *type)
    {
        bool ret = false;

        if ( strcmp(type,"u64") == 0 ||
             strcmp(type,"u32") == 0 ||
             strcmp(type,"u16") == 0 ||
             strcmp(type,"u8") == 0 ||
             strcmp(type, "i64") == 0 ||
             strcmp(type, "i32") == 0 ||
             strcmp(type, "i16") == 0 ||
             strcmp(type, "i8") == 0 ||
            strcmp(type, "float") == 0 ||
            strcmp(type, "bool") == 0 )
        {
            ret = true;
        }

        return ret;
    }

    void saveDeserialize(CodePrinter &cpheader, CodePrinter &cpimpl, const ClassEnumMap &classEnum)
    {
        if (mIsEnum)
        {
            return; // we don't serialize enums..
        }
        cpheader.linefeed();

#if 1
        cpimpl.linefeed();
        cpimpl.printCode(0,"// Deserialize object %s\n", mName.c_str());
        cpimpl.printCode(0,"template<typename DocumentOrObject>\n");
        cpimpl.printCode(0,"bool deserializeFrom(const DocumentOrObject& d, %s& r)\n", mName.c_str());
        cpimpl.printCode(0,"{\n");
        if (!mInheritsFrom.empty())
        {
            cpimpl.printCode(1,"//Deserialize the base class (%s) first.\n", mInheritsFrom.c_str());
            cpimpl.printCode(1, "if ( !deserializeFrom(d,static_cast<%s&>(r)))\n",mInheritsFrom.c_str());
            cpimpl.printCode(1, "{\n");
            cpimpl.printCode(2, "return false;\n");
            cpimpl.printCode(1, "}\n");

        }
        // Get a list of non map items.
        std::vector< std::string > nonMapItems;
        for (auto &i:mItems)
        {
            if (!i.mInheritsFrom.empty())
            {
                continue; // already serialized by the base class
            }
            if ( !i.mIsMap )
            {
                nonMapItems.push_back(i.mMember);
            }
        }
        for (auto &i : mItems)
        {
            if ( !i.mInheritsFrom.empty() )
            {
                continue; // already serialized by the base class
            }
            cpimpl.printCode(1,"// Deserialize member: '%s' of type '%s'\n", i.mMember.c_str(), i.mType.c_str() );
            cpimpl.printCode(1,"{\n");
            StandardType type = getStandardType(i.mType.c_str());
            if ( type != StandardType::none )
            {
                bool isNumeric;
                const char *getType = deserializeTypeName(type,isNumeric);
                if ( i.mIsMap )
                {
                    cpimpl.printCode(2,"// Deserialize map: '_%s' of type 'string'\n", i.mMember.c_str());
                    cpimpl.printCode(2,"{\n");
                    cpimpl.printCode(3,"for (rapidjson::Value::ConstMemberIterator iter = d.MemberBegin(); iter != d.MemberEnd(); ++iter)\n");
                    cpimpl.printCode(3,"{\n");
                    cpimpl.printCode(4,"const char* key = iter->name.GetString();\n");
                    cpimpl.printCode(4,"const rapidjson::Value &item = iter->value;\n");
                    if ( isNumeric )
                    {
                        assert(nonMapItems.empty()); // need to handle this edge case
                        cpimpl.printCode(4,"{\n");
                        cpimpl.printCode(4,"    if (item.IsString())\n");
                        cpimpl.printCode(4,"    {\n");
                        cpimpl.printCode(4,"        uint64_t ivalue = 0;\n");
                        cpimpl.printCode(4,"        stringToInt(item.GetString(), ivalue);\n");
                        cpimpl.printCode(4,"        r._%s[std::string(key)] = ivalue;\n", i.mMember.c_str());
                        cpimpl.printCode(4,"    }\n");
                        cpimpl.printCode(4,"    else if (item.IsNumber())\n");
                        cpimpl.printCode(4,"    {\n");
                        cpimpl.printCode(4,"        r._%s[std::string(key)] = item.GetUint64();\n", i.mMember.c_str());
                        cpimpl.printCode(4,"    }\n");
                        cpimpl.printCode(4,"}\n");
                    }
                    else
                    {
                        cpimpl.printCode(4,"if (item.IsString())\n");
                        cpimpl.printCode(4,"{\n");
                        cpimpl.printCode(5,"const char *value = item.GetString();\n");
                        if ( !nonMapItems.empty() )
                        {
                            cpimpl.printCode(5, "// Skip keys which we already deserialize explicitly by name.\n");
                            cpimpl.printCode(5,"if ( r.isMember(key) )\n");
                            cpimpl.printCode(5, "{\n");
                            cpimpl.printCode(5, "}\n");
                            cpimpl.printCode(5, "else\n");
                        }
                        cpimpl.printCode(5,"{\n");
                        cpimpl.printCode(6,"r._%s[std::string(key)] = std::string(value);\n", i.mMember.c_str());
                        cpimpl.printCode(5,"}\n");
                        cpimpl.printCode(4,"}\n");
                    }
                    cpimpl.printCode(3, "}\n");
                    cpimpl.printCode(2, "}\n");
                }
                else if ( getType )
                {
                    cpimpl.printCode(2,"auto found = d.FindMember(\"%s\");\n", i.mMember.c_str());
                    cpimpl.printCode(2,"if ( found != d.MemberEnd() )\n");
                    cpimpl.printCode(2,"{\n");
                    cpimpl.printCode(3,"const rapidjson::Value &v = found->value;\n");

                    if ( isNumeric )
                    {
                        if ( i.mIsArray )
                        {
                            const char *cppType = getCppTypeString(i.mType.c_str(),false);

                            cpimpl.printCode(3,"if (v.IsArray())\n");
                            cpimpl.printCode(3,"{\n");
                            cpimpl.printCode(3,"    std::vector< %s > %s;\n", cppType, i.mMember.c_str());
                            cpimpl.printCode(3,"    for (rapidjson::SizeType i = 0; i < v.Size(); i++)\n");
                            cpimpl.printCode(3,"    {\n");
                            cpimpl.printCode(3,"        const rapidjson::Value& entry = v[i];\n");
                            cpimpl.printCode(3,"        %s ivalue;\n", cppType);
                            cpimpl.printCode(3,"        if (entry.IsString())\n");
                            cpimpl.printCode(3,"        {\n");
                            cpimpl.printCode(3,"            stringToInt(entry.GetString(), ivalue);\n");
                            cpimpl.printCode(3,"        }\n");
                            cpimpl.printCode(3,"        else if (entry.IsNumber())\n");
                            cpimpl.printCode(3,"        {\n");
                            cpimpl.printCode(3,"            ivalue = entry.%s();\n", getType);
                            cpimpl.printCode(3,"        }\n");
                            cpimpl.printCode(3,"        else\n");
                            cpimpl.printCode(3,"        {\n");
                            cpimpl.printCode(3,"            return false;\n");
                            cpimpl.printCode(3,"        }\n");
                            cpimpl.printCode(3,"        %s.push_back(ivalue);\n", i.mMember.c_str());
                            cpimpl.printCode(3,"    }\n");
                            cpimpl.printCode(3,"    r.%s = %s;\n", i.mMember.c_str(), i.mMember.c_str());
                            cpimpl.printCode(3,"}\n");
                            cpimpl.printCode(3,"else\n");
                            cpimpl.printCode(3,"{\n");
                            cpimpl.printCode(3,"    return false;\n");
                            cpimpl.printCode(3,"}\n");
                        }
                        else
                        {
                            cpimpl.printCode(3,"if ( v.IsString() )\n");
                            cpimpl.printCode(3,"{\n");
                            cpimpl.printCode(4,"%s ivalue;\n", getCppTypeString(i.mType.c_str(),false));
                            cpimpl.printCode(4,"stringToInt(v.GetString(),ivalue);\n");
                            cpimpl.printCode(4,"r.%s = ivalue;\n",i.mMember.c_str());
                            cpimpl.printCode(3,"}\n");
                            cpimpl.printCode(3,"else if ( v.IsNumber() )\n");
                            cpimpl.printCode(3,"{\n");
                            cpimpl.printCode(4,"r.%s = v.%s();\n",i.mMember.c_str(), getType);
                            cpimpl.printCode(3,"}\n");
                            cpimpl.printCode(3, "else\n");
                            cpimpl.printCode(3,"{\n");
                            cpimpl.printCode(4,"return false;\n");
                            cpimpl.printCode(3,"}\n");
                        }
                    }
                    else
                    {
                        const char *checkName = nullptr;
                        const char *getName = nullptr;
                        switch ( type )
                        {
                        case StandardType::bool_type:
                            checkName = "IsBool";
                            getName = "GetBool";
                            break;
                        case StandardType::float_type:
                            checkName = "IsFloat";
                            getName = "GetFloat";
                            break;
                        case StandardType::string_type:
                            checkName = "IsString";
                            getName = "GetString";
                            break;
                        default:
                            assert(0);
                            break;
                        }
                        cpimpl.printCode(3,"if ( v.%s() )\n",checkName);
                        cpimpl.printCode(3,"{\n");
                        if ( i.mIsArray )
                        {
                            cpimpl.printCode(4,"if (v.IsArray())\n");
                            cpimpl.printCode(4,"{\n");
                            cpimpl.printCode(4,"    for (rapidjson::SizeType i = 0; i < v.Size(); i++)\n");
                            cpimpl.printCode(4,"    {\n");
                            cpimpl.printCode(4,"        const rapidjson::Value& item = v[i];\n");
                            cpimpl.printCode(4,"        if (item.%s())\n", checkName);
                            cpimpl.printCode(4,"        {\n");
                            cpimpl.printCode(4,"            r.%s.push_back(std::string(item.%s()));\n",i.mMember.c_str(), getName);
                            cpimpl.printCode(4,"        }\n");
                            cpimpl.printCode(4,"        else\n");
                            cpimpl.printCode(4,"        {\n");
                            cpimpl.printCode(4,"            return false;\n");
                            cpimpl.printCode(4,"        }\n");
                            cpimpl.printCode(4,"    }\n");
                            cpimpl.printCode(4,"}\n");
                            cpimpl.printCode(4,"else\n");
                            cpimpl.printCode(4,"{\n");
                            cpimpl.printCode(4,"    return false;\n");
                            cpimpl.printCode(4,"}\n");
                        }
                        else
                        {
                            cpimpl.printCode(4,"r.%s = v.%s();\n", i.mMember.c_str(), getType);
                        }
                        cpimpl.printCode(3,"}\n");
                        if( type == StandardType::float_type )
                        {
                            cpimpl.printCode(3, "else if ( v.IsNumber() )\n");
                            cpimpl.printCode(3,"{\n");
                            cpimpl.printCode(4, "r.%s = float(v.GetUint64());\n", i.mMember.c_str(), getType);
                            cpimpl.printCode(3,"}\n");
                        }
                        cpimpl.printCode(3,"else\n");
                        cpimpl.printCode(3,"{\n");
                        cpimpl.printCode(4,"return false;\n");
                        cpimpl.printCode(3,"}\n");
                    }
                    cpimpl.printCode(2,"}\n");
                    if ( i.mIsOptional != OptionalType::required)
                    {
                    }
                    else
                    {
                        cpimpl.printCode(2,"else\n");
                        cpimpl.printCode(2,"{\n");
                        cpimpl.printCode(3,"return false;\n");
                        cpimpl.printCode(2,"}\n");
                    }
                }
            }
            else
            {
                ClassEnumMap::const_iterator found = classEnum.find(i.mType);
                if (found != classEnum.end())
                {
                    if (i.mIsArray)
                    {
                        if ( i.mIsMap )
                        {
                            cpimpl.printCode(2,"//Deserialize this array of enums as a map.\n");
                            cpimpl.printCode(2,"for (rapidjson::Value::ConstMemberIterator iter = d.MemberBegin(); iter != d.MemberEnd(); ++iter)\n");
                            cpimpl.printCode(2,"{\n");
                            cpimpl.printCode(2,"    const char* key = iter->name.GetString();\n");
                            cpimpl.printCode(2,"    const rapidjson::Value &item = iter->value;\n");
                            cpimpl.printCode(2,"    if (item.IsArray())\n");
                            cpimpl.printCode(2,"    {\n");
                            cpimpl.printCode(2,"        std::vector< %s > items;\n", i.mType.c_str());
                            cpimpl.printCode(2,"        for (rapidjson::SizeType i = 0; i < item.Size(); i++)\n");
                            cpimpl.printCode(2,"        {\n");
                            cpimpl.printCode(2,"            const rapidjson::Value& entry = item[i];\n");
                            cpimpl.printCode(2,"            if (entry.IsString())\n");
                            cpimpl.printCode(2,"            {\n");
                            cpimpl.printCode(2,"                bool isOk;\n");
                            cpimpl.printCode(2,"                %s p = unstringifyEnum<%s>(entry.GetString(), isOk);\n", i.mType.c_str(), i.mType.c_str());
                            cpimpl.printCode(2,"                if (isOk)\n");
                            cpimpl.printCode(2,"                {\n");
                            cpimpl.printCode(2,"                    items.push_back(p);\n");
                            cpimpl.printCode(2,"                }\n");
                            cpimpl.printCode(2,"                else\n");
                            cpimpl.printCode(2,"                {\n");
                            cpimpl.printCode(2,"                    return false;\n");
                            cpimpl.printCode(2,"                }\n");
                            cpimpl.printCode(2,"            }\n");
                            cpimpl.printCode(2,"            else\n");
                            cpimpl.printCode(2,"            {\n");
                            cpimpl.printCode(2,"                return false;\n");
                            cpimpl.printCode(2,"            }\n");
                            cpimpl.printCode(2,"        }\n");
                            cpimpl.printCode(2,"        r._%s[std::string(key)] = items;\n", i.mMember.c_str());
                            cpimpl.printCode(2,"    }\n");
                            cpimpl.printCode(2,"    else\n");
                            cpimpl.printCode(2,"    {\n");
                            cpimpl.printCode(2,"        return false;\n");
                            cpimpl.printCode(2,"    }\n");
                            cpimpl.printCode(2,"}\n");
                        }
                        else
                        {
                            cpimpl.printCode(2,"//Deserialize an array of objects of type '%s' to array '%s'.\n", i.mType.c_str(), i.mMember.c_str());
                            cpimpl.printCode(2,"auto found = d.FindMember(\"%s\");\n", i.mMember.c_str());
                            cpimpl.printCode(2,"if (found != d.MemberEnd())\n");
                            cpimpl.printCode(2,"{\n");
                            cpimpl.printCode(3,"const rapidjson::Value &v = found->value;\n");
                            cpimpl.printCode(3,"if (v.IsArray())\n");
                            cpimpl.printCode(3,"{\n");
                            cpimpl.printCode(4,"for (rapidjson::SizeType i = 0; i < v.Size(); i++)\n");
                            cpimpl.printCode(4,"{\n");
                            cpimpl.printCode(5,"const rapidjson::Value& item = v[i];\n");
                            if ( (*found).second )
                            {
                                cpimpl.printCode(5,"if (item.IsString())\n");
                                cpimpl.printCode(5,"{\n");
                                cpimpl.printCode(6,"bool isOk;\n");
                                cpimpl.printCode(6,"%s h = unstringifyEnum<%s>(item.GetString(), isOk);\n", i.mType.c_str(), i.mType.c_str());
                                cpimpl.printCode(6,"if (isOk)\n");
                                cpimpl.printCode(6,"{\n");
                                cpimpl.printCode(7,"r.acl.push_back(h);\n");
                                cpimpl.printCode(6,"}\n");
                                cpimpl.printCode(6,"else\n");
                                cpimpl.printCode(6,"{\n");
                                cpimpl.printCode(7,"return false;\n");
                                cpimpl.printCode(6,"}\n");
                                cpimpl.printCode(5,"}\n");
                                cpimpl.printCode(5,"else\n");
                                cpimpl.printCode(5,"{\n");
                                cpimpl.printCode(6,"return false;\n");
                                cpimpl.printCode(5,"}\n");
                            }
                            else
                            {
                                cpimpl.printCode(2, "            %s h;\n", i.mType.c_str());
                                cpimpl.printCode(2,"            if (deserializeFrom(item, h))\n");
                                cpimpl.printCode(2,"            {\n");
                                cpimpl.printCode(2,"                r.%s.push_back(h);\n", i.mMember.c_str());
                                cpimpl.printCode(2,"            }\n");
                                cpimpl.printCode(2,"            else\n");
                                cpimpl.printCode(2,"            {\n");
                                cpimpl.printCode(2,"                return false;\n");
                                cpimpl.printCode(2,"            }\n");
                            }
                            cpimpl.printCode(2,"        }\n");
                            cpimpl.printCode(2,"    }\n");
                            cpimpl.printCode(2,"    else\n");
                            cpimpl.printCode(2,"    {\n");
                            cpimpl.printCode(2,"        return false;\n");
                            cpimpl.printCode(2,"    }\n");
                            cpimpl.printCode(2,"}\n");
                            if (i.mIsOptional != OptionalType::required)
                            {
                            }
                            else
                            {
                                cpimpl.printCode(2, "else\n");
                                cpimpl.printCode(2, "{\n");
                                cpimpl.printCode(3, "return false;\n");
                                cpimpl.printCode(2, "}\n");
                            }
                        }
                    }
                    else
                    {
                        if ((*found).second) // if it is an enum..
                        {
                            cpimpl.printCode(2, "auto found = d.FindMember(\"%s\");\n", i.mMember.c_str());
                            cpimpl.printCode(2, "if ( found != d.MemberEnd() )\n");
                            cpimpl.printCode(2, "{\n");
                            cpimpl.printCode(3, "const rapidjson::Value &v = found->value;\n");
                            cpimpl.printCode(3,"if ( v.IsString() )\n");
                            cpimpl.printCode(3,"{\n");
                            cpimpl.printCode(4,"bool isOk;\n");
                            cpimpl.printCode(4,"r.%s = unstringifyEnum<%s>(v.GetString(),isOk);\n", i.mMember.c_str(),i.mType.c_str()); 
                            cpimpl.printCode(4,"if ( !isOk )\n");
                            cpimpl.printCode(4, "{\n");
                            cpimpl.printCode(5, "return false;\n");
                            cpimpl.printCode(4, "}\n");
                            cpimpl.printCode(3,"}\n");
                            cpimpl.printCode(3,"else\n");
                            cpimpl.printCode(3, "{\n");
                            if ( i.mSerializeEnumAsInteger )
                            {
                                cpimpl.printCode(4, "if ( v.IsNumber() )\n");
                                cpimpl.printCode(4, "{\n");
                                cpimpl.printCode(5, "uint64_t evalue = v.GetUint64();\n");
                                cpimpl.printCode(5, "r.%s = %s(evalue);\n", i.mMember.c_str(), i.mType.c_str());
                                cpimpl.printCode(4, "}\n");
                                cpimpl.printCode(4, "else\n");
                                cpimpl.printCode(4, "{\n");
                                cpimpl.printCode(5, "return false;\n");
                                cpimpl.printCode(4, "}\n");
                            }
                            else
                            {
                                cpimpl.printCode(4, "return false;\n");
                            }
                            cpimpl.printCode(3, "}\n");
                            cpimpl.printCode(2, "}\n");
                        }
                        else
                        {
                            cpimpl.printCode(2,"// Deserialize object type '%s' into member variable '%s'\n",i.mType.c_str(),i.mMember.c_str() );
                            cpimpl.printCode(2, "auto found = d.FindMember(\"%s\");\n",i.mMember.c_str());
                            cpimpl.printCode(2, "if (found != d.MemberEnd())\n");
                            cpimpl.printCode(2, "{\n");
                            cpimpl.printCode(3, "%s h;\n", i.mType.c_str());
                            cpimpl.printCode(3, "if ( deserializeFrom(found->value, h) )\n");
                            cpimpl.printCode(3,"{\n");
                            cpimpl.printCode(4, "r.%s = h;\n", i.mMember.c_str());
                            cpimpl.printCode(3, "}\n");
                            cpimpl.printCode(3,"else\n");
                            cpimpl.printCode(3, "{\n");
                            cpimpl.printCode(4, "return false;\n", i.mMember.c_str());
                            cpimpl.printCode(3, "}\n");
                            cpimpl.printCode(2, "}\n");
                            if (i.mIsOptional  != OptionalType::required)
                            {
                            }
                            else
                            {
                                cpimpl.printCode(2, "else\n");
                                cpimpl.printCode(2, "{\n");
                                cpimpl.printCode(3, "return false;\n");
                                cpimpl.printCode(2, "}\n");
                            }
                        }
                    }
                }
                else
                {
                    cpimpl.printCode(2, "// TODO: Need to deserialize this type.\n");
                }
            }
            cpimpl.printCode(1, "}\n");

        }
        cpimpl.printCode(0,"    return true;\n");
        cpimpl.printCode(0,"}\n");
        cpimpl.linefeed();

        cpimpl.linefeed();
        cpimpl.printCode(0,"template struct details::Deserialize<%s>;\n", mName.c_str());
        cpimpl.linefeed();
// Test method, probably no longer needed
#if 0
        cpimpl.linefeed();
        cpimpl.printCode(0,"void test%s(void)\n", mName.c_str());
        cpimpl.printCode(0,"{\n");
        cpimpl.printCode(1,"%s test;\n", mName.c_str());
        cpimpl.printCode(1,"std::string str = serialize(test);\n");
        cpimpl.printCode(1,"bool isOk;\n");
        cpimpl.printCode(1,"deserialize<%s>(str,isOk);\n", mName.c_str());
        cpimpl.printCode(0,"}\n");
        cpimpl.linefeed();
#endif

#endif

#if 0
        cpimpl.linefeed();
        cpimpl.printCode(0, "template<typename DocumentOrObject, typename Alloc>\n");
        cpimpl.printCode(0, "DocumentOrObject& serializeTo(const %s& type, DocumentOrObject& d, Alloc& alloc)\n", mName.c_str());
        cpimpl.printCode(0, "{\n");

        if (!mInheritsFrom.empty())
        {
            cpimpl.printCode(1, "serializeTo(static_cast<const %s&>(type), d, alloc);\n", mInheritsFrom.c_str());
        }

        for (auto &i : mItems)
        {
            const char *type = i.mType.c_str();
            if (isStandardType(type))
            {
                if (i.mIsOptional)
                {
                    cpimpl.printCode(1, "if ( type.%s.has_value() )\n", i.mMember.c_str());
                    cpimpl.printCode(1, "{\n");
                    cpimpl.printCode(2, "d.AddMember(\"%s\",type.%s.value(),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                    cpimpl.printCode(1, "}\n");
                }
                else
                {
                    cpimpl.printCode(1, "d.AddMember(\"%s\",type.%s,alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                }
            }
            else if (strcmp(type, "string") == 0)
            {
                if (i.mIsOptional)
                {
                    cpimpl.printCode(1, "if ( type.%s.has_value() )\n", i.mMember.c_str());
                    cpimpl.printCode(1, "{\n");
                    cpimpl.printCode(2, "d.AddMember(\"%s\",rapidjson::StringRef(type.%s.value().c_str()),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                    cpimpl.printCode(1, "}\n");
                }
                else
                {
                    cpimpl.printCode(1, "d.AddMember(\"%s\",rapidjson::StringRef(type.%s.c_str()),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                }
            }
            else
            {
                ClassEnumMap::const_iterator found = classEnum.find(i.mType);
                if (found != classEnum.end())
                {
                    if (i.mIsArray)
                    {
                        cpimpl.printCode(1, "{\n");

                        if (i.mIsMap)
                        {
                            cpimpl.printCode(2, "for (auto &i : type._name)\n");
                            cpimpl.printCode(2, "{\n");
                            cpimpl.printCode(3, "rapidjson::Value v(rapidjson::kArrayType);\n");
                            cpimpl.printCode(3, "for (auto &j : i.second)\n");
                            cpimpl.printCode(3, "{\n");
                            cpimpl.printCode(4, "const char *name = stringifyEnum(j);\n");
                            cpimpl.printCode(4, "rapidjson::Value item(rapidjson::StringRef(name));\n");
                            cpimpl.printCode(4, "v.PushBack(item, alloc);\n");
                            cpimpl.printCode(3, "}\n");
                            cpimpl.printCode(3, "d.AddMember(rapidjson::StringRef(i.first.c_str()), v, alloc);\n");
                            cpimpl.printCode(2, "}\n");
                        }
                        else
                        {
                            cpimpl.printCode(2, "rapidjson::Value array(rapidjson::kArrayType);\n");
                            cpimpl.printCode(2, "for (auto &i : type.%s)\n", i.mMember.c_str());
                            cpimpl.printCode(2, "{\n");
                            cpimpl.printCode(3, "rapidjson::Value v(rapidjson::kObjectType);\n");
                            cpimpl.printCode(3, "serializeTo(i, v, alloc);\n");
                            cpimpl.printCode(3, "array.PushBack(v, alloc);\n");
                            cpimpl.printCode(2, "}\n");
                            cpimpl.printCode(2, "d.AddMember(\"%s\", array, alloc);\n", i.mMember.c_str());
                        }

                        cpimpl.printCode(1, "}\n");
                    }
                    else
                    {
                        if ((*found).second) // if it is an enum..
                        {
                            if (i.mIsOptional)
                            {
                                cpimpl.printCode(1, "if ( type.%s.has_value() )\n", i.mMember.c_str());
                                cpimpl.printCode(1, "{\n");
                                cpimpl.printCode(2, "d.AddMember(\"%s\",rapidjson::StringRef(stringifyEnum(type.%s)),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                                cpimpl.printCode(1, "}\n");
                            }
                            else
                            {
                                cpimpl.printCode(1, "d.AddMember(\"%s\",rapidjson::StringRef(stringifyEnum(type.%s)),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                            }
                        }
                        else
                        {
                            if (i.mIsOptional)
                            {
                                cpimpl.printCode(1, "if ( type.%s.has_value() )\n", i.mMember.c_str());
                            }
                            cpimpl.printCode(1, "{\n");
                            cpimpl.printCode(2, "rapidjson::Value v(rapidjson::kObjectType);\n");
                            if (i.mIsOptional)
                            {
                                cpimpl.printCode(2, "serializeTo(type.%s.value(),v,alloc);\n", i.mMember.c_str());
                            }
                            else
                            {
                                cpimpl.printCode(2, "serializeTo(type.%s,v,alloc);\n", i.mMember.c_str());
                            }
                            cpimpl.printCode(2, "d.AddMember(\"%s\",v,alloc);\n", i.mMember.c_str());
                            cpimpl.printCode(1, "}\n");
                        }
                    }
                }
                else
                {
                    assert(0);
                }
            }

        }

        cpimpl.printCode(0, "    return d;\n");
        cpimpl.printCode(0, "}\n");
        cpimpl.linefeed();
        cpimpl.printCode(0, "std::string serialize(const %s& type)\n", mName.c_str());
        cpimpl.printCode(0, "{\n");
        cpimpl.printCode(0, "    rapidjson::Document d;\n");
        cpimpl.printCode(0, "    d.SetObject();\n");
        cpimpl.printCode(0, "    serializeTo(type, d, d.GetAllocator());\n");
        cpimpl.printCode(0, "    return serializeDocument(d);\n");
        cpimpl.printCode(0, "}\n");
        cpimpl.linefeed();
#endif
    }


    void saveSerialize(CodePrinter &cpheader,CodePrinter &cpimpl,const ClassEnumMap &classEnum)
    {
        if ( mIsEnum )
        {
            return; // we don't serialize enums..
        }
        cpheader.linefeed();
        cpheader.printCode(0,"std::string serialize(const %s& type);\n", mName.c_str());

        cpimpl.linefeed();
        cpimpl.printCode(0,"template<typename DocumentOrObject, typename Alloc>\n");
        cpimpl.printCode(0,"DocumentOrObject& serializeTo(const %s& type, DocumentOrObject& d, Alloc& alloc)\n", mName.c_str());
        cpimpl.printCode(0,"{\n");

        if ( !mInheritsFrom.empty())
        {
            cpimpl.printCode(1,"serializeTo(static_cast<const %s&>(type), d, alloc);\n", mInheritsFrom.c_str());
        }

        for (auto &i : mItems)
        {
            if ( !i.mInheritsFrom.empty() )
            {
                continue;
            }
            const char *type = i.mType.c_str();
            if ( isStandardType(type) )
            {
                if ( i.mIsArray )
                {
                    if ( i.mIsMap )
                    {
                        cpimpl.printCode(1,"// Serialize this map type: %s : %s\n", i.mMember.c_str(), i.mType.c_str());
                        cpimpl.printCode(1,"for (auto &i : type._%s)\n", i.mMember.c_str());
                        cpimpl.printCode(1,"{\n");
                        cpimpl.printCode(1,"    d.AddMember(rapidjson::StringRef(i.first.c_str()), i.second, alloc);\n");
                        cpimpl.printCode(1,"}\n");
                    }
                    else
                    {


                        cpimpl.printCode(1,"{\n");
                        cpimpl.printCode(1,"    rapidjson::Value varray(rapidjson::kArrayType);\n");
                        cpimpl.printCode(1,"    for (auto &i : type.%s)\n",i.mMember.c_str());
                        cpimpl.printCode(1,"    {\n");
                        cpimpl.printCode(1,"        rapidjson::Value v(i);\n");
                        cpimpl.printCode(1,"        varray.PushBack(v, alloc);\n");
                        cpimpl.printCode(1,"    }\n");
                        cpimpl.printCode(1,"    d.AddMember(\"%s\", varray, alloc);\n", i.mMember.c_str());
                        cpimpl.printCode(1,"}\n");
                    }
                }
                else if ( i.mIsOptional == OptionalType::optional)
                {
                    cpimpl.printCode(1,"if ( type.%s.has_value() )\n", i.mMember.c_str());
                    cpimpl.printCode(1,"{\n");
                    cpimpl.printCode(2,"d.AddMember(\"%s\",type.%s.value(),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                    cpimpl.printCode(1,"}\n");
                }
                else
                {
                    cpimpl.printCode(1,"d.AddMember(\"%s\",type.%s,alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                }
            }
            else if ( strcmp(type,"string") == 0 )
            {
                if ( i.mIsArray )
                {
                    if (i.mIsOptional == OptionalType::optional)
                    {
                        cpimpl.printCode(1, "if ( type.%s.has_value() )\n", i.mMember.c_str());
                    }
                    cpimpl.printCode(1, "{\n");
                    cpimpl.printCode(1, "    rapidjson::Value v(rapidjson::kArrayType);\n");
                    if ( i.mIsMap )
                    {
                        cpimpl.printCode(1, "    for (auto &i : type._%s)\n", i.mMember.c_str());
                    }
                    else
                    {
                        cpimpl.printCode(1, "    for (auto &i : type.%s)\n", i.mMember.c_str());
                    }
                    cpimpl.printCode(1, "    {\n");
                    if ( i.mIsMap )
                    {
                        cpimpl.printCode(1, "        d.AddMember(rapidjson::StringRef(i.first.c_str()), rapidjson::StringRef(i.second.c_str()), alloc);\n");
                    }
                    else
                    {
                        cpimpl.printCode(1, "        rapidjson::Value item(rapidjson::StringRef(i.c_str()));\n");
                        cpimpl.printCode(1, "        v.PushBack(item, alloc);\n");
                    }
                    cpimpl.printCode(1, "    }\n");
                    if ( !i.mIsMap )
                    {
                        cpimpl.printCode(1, "    d.AddMember(\"%s\", v, alloc);\n", i.mMember.c_str());
                    }
                    cpimpl.printCode(1, "}\n");
                }
                else
                {
                    if ( i.mIsOptional == OptionalType::optional)
                    {
                        cpimpl.printCode(1, "if ( type.%s.has_value() )\n", i.mMember.c_str());
                        cpimpl.printCode(1, "{\n");
                        cpimpl.printCode(2, "d.AddMember(\"%s\",rapidjson::StringRef(type.%s.value().c_str()),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                        cpimpl.printCode(1, "}\n");
                    }
                    else
                    {
                        cpimpl.printCode(1, "d.AddMember(\"%s\",rapidjson::StringRef(type.%s.c_str()),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                    }
                }
            }
            else
            {
                ClassEnumMap::const_iterator found = classEnum.find(i.mType);
                if ( found != classEnum.end() )
                {
                    if ( i.mIsArray )
                    {
                        cpimpl.printCode(1,"{\n");
                        if ( i.mIsMap )
                        {
                            cpimpl.printCode(2,"for (auto &i : type._%s)\n", i.mMember.c_str());
                            cpimpl.printCode(2,"{\n");
                            cpimpl.printCode(3,"rapidjson::Value v(rapidjson::kArrayType);\n");
                            cpimpl.printCode(3,"for (auto &j : i.second)\n");
                            cpimpl.printCode(3,"{\n");
                            cpimpl.printCode(4,"const char *name = stringifyEnum(j);\n");
                            cpimpl.printCode(4,"rapidjson::Value item(rapidjson::StringRef(name));\n");
                            cpimpl.printCode(4,"v.PushBack(item, alloc);\n");
                            cpimpl.printCode(3,"}\n");
                            cpimpl.printCode(3,"d.AddMember(rapidjson::StringRef(i.first.c_str()), v, alloc);\n");
                            cpimpl.printCode(2,"}\n");
                        }
                        else
                        {
                            cpimpl.printCode(2,"rapidjson::Value array(rapidjson::kArrayType);\n");
                            cpimpl.printCode(2,"for (auto &i : type.%s)\n",i.mMember.c_str());
                            cpimpl.printCode(2,"{\n");
                            if ( (*found).second )
                            {
                                cpimpl.printCode(3, "rapidjson::Value v(rapidjson::StringRef(stringifyEnum(i)));\n");
                            }
                            else
                            {
                                cpimpl.printCode(3,"rapidjson::Value v(rapidjson::kObjectType);\n");
                                cpimpl.printCode(3,"serializeTo(i, v, alloc);\n");
                            }
                            cpimpl.printCode(3,"array.PushBack(v, alloc);\n");
                            cpimpl.printCode(2,"}\n");
                            cpimpl.printCode(2,"d.AddMember(\"%s\", array, alloc);\n", i.mMember.c_str());
                        }

                        cpimpl.printCode(1,"}\n");
                    }
                    else
                    {
                        if ( (*found).second ) // if it is an enum..
                        {
                            if (i.mIsOptional == OptionalType::optional)
                            {
                                cpimpl.printCode(1, "if ( type.%s.has_value() )\n", i.mMember.c_str());
                                cpimpl.printCode(1, "{\n");
                                if ( i.mSerializeEnumAsInteger )
                                {
                                    cpimpl.printCode(2, "d.AddMember(\"%s\",uint64_t(type.%s.value()),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                                }
                                else
                                {
                                    cpimpl.printCode(2, "d.AddMember(\"%s\",rapidjson::StringRef(stringifyEnum(type.%s.value())),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                                }
                                cpimpl.printCode(1, "}\n");
                            }
                            else
                            {
                                if ( i.mSerializeEnumAsInteger )
                                {
                                    cpimpl.printCode(1, "d.AddMember(\"%s\",uint64_t(type.%s),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                                }
                                else
                                {
                                    cpimpl.printCode(1, "d.AddMember(\"%s\",rapidjson::StringRef(stringifyEnum(type.%s)),alloc);\n", i.mMember.c_str(), i.mMember.c_str());
                                }
                            }
                        }
                        else
                        {
                            if (i.mIsOptional  == OptionalType::optional)
                            {
                                cpimpl.printCode(1, "if ( type.%s.has_value() )\n", i.mMember.c_str());
                            }
                            cpimpl.printCode(1, "{\n");
                            cpimpl.printCode(2,"rapidjson::Value v(rapidjson::kObjectType);\n");
                            if ( i.mIsOptional  == OptionalType::optional)
                            {
                                cpimpl.printCode(2, "serializeTo(type.%s.value(),v,alloc);\n", i.mMember.c_str());
                            }
                            else
                            {
                                cpimpl.printCode(2, "serializeTo(type.%s,v,alloc);\n", i.mMember.c_str());
                            }
                            cpimpl.printCode(2,"d.AddMember(\"%s\",v,alloc);\n", i.mMember.c_str() );
                            cpimpl.printCode(1, "}\n");
                        }
                    }
                }
                else
                {
                    assert(0);
                }
            }

        }

        cpimpl.printCode(0,"    return d;\n");
        cpimpl.printCode(0,"}\n");
        cpimpl.linefeed();
        cpimpl.printCode(0,"std::string serialize(const %s& type)\n", mName.c_str());
        cpimpl.printCode(0,"{\n");
        cpimpl.printCode(0,"    rapidjson::Document d;\n");
        cpimpl.printCode(0,"    d.SetObject();\n");
        cpimpl.printCode(0,"    serializeTo(type, d, d.GetAllocator());\n");
        cpimpl.printCode(0,"    return serializeDocument(d);\n");
        cpimpl.printCode(0,"}\n");
        cpimpl.linefeed();

    }

    void saveTypeScript(CodePrinter &cpdom,CodePrinter &cpenum,CodePrinter &cpenumImpl,const DOM &dom,OmniCommandInstanceVector &instances,const char *nspace,const char *destDir)
    {
#if EXPORT_COMMAND_CODE
        if (_stricmp(mType.c_str(), "Class") == 0 &&
            _stricmp(mInheritsFrom.c_str(),"Command") == 0 )
        {
            const char *commandName = mName.c_str();
            const char *commandType = nullptr;
            for (auto &i : mItems)
            {
                if ( strcmp(i.mMember.c_str(),"command") == 0 &&
                     strcmp(i.mType.c_str(),"CommandType") == 0 &&
                     strcmp(i.mInheritsFrom.c_str(),"Command") == 0 )
                {
                    commandType = i.mDefaultValue.c_str();
                    break;
                }
            }
            if ( commandType )
            {
                saveOmniCommand(commandName,commandType,instances,nspace,destDir);
            }
        }
#endif
        if (_stricmp(mType.c_str(), "Enum") == 0)
        {

            {
                cpenum.linefeed();
                cpenum.printCode(0,"const char *stringifyEnum(%s x);\n", mName.c_str());
                cpenum.printCode(0,"std::string stringifyEnumStdString(%s x);\n", mName.c_str());
                cpenum.linefeed();

                cpenumImpl.printCode(0,"struct %sKey\n", mName.c_str());
                cpenumImpl.printCode(0,"{\n");
                cpenumImpl.printCode(1,"%s key;\n", mName.c_str());
                cpenumImpl.printCode(1,"const char *value;\n");
                cpenumImpl.printCode(0,"};\n");

                cpenumImpl.printCode(0, "static %sKey %sList[]\n", mName.c_str(), mName.c_str());
                cpenumImpl.printCode(0, "{\n");

                for (auto &i : mItems)
                {
                    const char *stringName = i.mMember.c_str();
                    if ( i.mAlias.size() )
                    {
                        stringName = i.mAlias.c_str();
                    }
                    cpenumImpl.printCode(1,"{ %s::%s, \"%s\" },\n", mName.c_str(), i.mMember.c_str(), stringName);
                }

                cpenumImpl.printCode(0, "};\n");

                cpenumImpl.printCode(0,"const char* stringifyEnum(%s in)\n",mName.c_str());
                cpenumImpl.printCode(0,"{\n");
                cpenumImpl.printCode(1, "const char *ret = nullptr;\n");
                cpenumImpl.printCode(1, "static std::unordered_map<%s, const char *> enumToStringMap;\n", mName.c_str());
                cpenumImpl.printCode(1, "static std::once_flag first;\n");
                cpenumImpl.printCode(1, "std::call_once(first, []()\n");
                cpenumImpl.printCode(1, "{\n");
                cpenumImpl.printCode(2, "for (auto e : %sList)\n", mName.c_str());
                cpenumImpl.printCode(2, "{\n");
                cpenumImpl.printCode(3,"enumToStringMap[e.key] = e.value;\n");
                cpenumImpl.printCode(2, "}\n");
                cpenumImpl.printCode(1, "});\n");
                cpenumImpl.printCode(1, "{\n");
                cpenumImpl.printCode(2, "const auto &found = enumToStringMap.find(in);\n");
                cpenumImpl.printCode(2, "if (found != enumToStringMap.end())\n");
                cpenumImpl.printCode(2, "{\n");
                cpenumImpl.printCode(3, "ret = (*found).second;\n");
                cpenumImpl.printCode(2, "}\n");
                cpenumImpl.printCode(2, "else\n");
                cpenumImpl.printCode(2, "{\n");
                cpenumImpl.printCode(3, "assert(0); // This should never happen unless the enum passed was corrupted\n");
                cpenumImpl.printCode(2, "}\n");
                cpenumImpl.printCode(1, "}\n");
                cpenumImpl.printCode(1, "return ret;\n");
                cpenumImpl.printCode(0,"}\n");
                cpenumImpl.printCode(0,"\n");
                cpenumImpl.linefeed();

                cpenumImpl.printCode(0, "std::string stringifyEnumStdString(%s in)\n", mName.c_str());
                cpenumImpl.printCode(0, "{\n");
                cpenumImpl.printCode(1, "return std::string(stringifyEnum(in));\n");
                cpenumImpl.printCode(0, "}\n");
                cpenumImpl.linefeed();



                cpenumImpl.printCode(0,"template<> %s unstringifyEnum(const std::string &in, bool& isValid)\n", mName.c_str());
                cpenumImpl.printCode(0,"{\n");
                const char *defaultValue =nullptr;
                if ( !mItems.empty() )
                {
                    defaultValue = mItems[0].mMember.c_str();
                }
                if ( defaultValue )
                {
                    cpenumImpl.printCode(0,"    %s ret = %s::%s;\n", mName.c_str(), mName.c_str(), defaultValue);
                }
                else
                {
                    cpenumImpl.printCode(0,"    %s ret;\n", mName.c_str());
                }
                cpenumImpl.printCode(0,"    isValid = false;\n");
                cpenumImpl.printCode(0,"    static std::unordered_map<std::string, %s> stringToEnumMap;\n", mName.c_str());
                cpenumImpl.printCode(0,"    static std::once_flag first;\n");
                cpenumImpl.printCode(0,"    std::call_once(first, []()\n");
                cpenumImpl.printCode(0,"    {\n");
                cpenumImpl.printCode(0,"        for (auto e : %sList)\n",mName.c_str());
                cpenumImpl.printCode(0,"        {\n");
                cpenumImpl.printCode(0,"            stringToEnumMap[std::string(e.value)] = e.key;\n");
                cpenumImpl.printCode(0,"        }\n");
                cpenumImpl.printCode(0,"    });\n");
                cpenumImpl.printCode(0,"    const auto &found = stringToEnumMap.find(in);\n");
                cpenumImpl.printCode(0,"    if (found != stringToEnumMap.end())\n");
                cpenumImpl.printCode(0,"    {\n");
                cpenumImpl.printCode(0,"        ret = (*found).second;\n");
                cpenumImpl.printCode(0,"        isValid = true;\n");
                cpenumImpl.printCode(0,"    }\n");
                cpenumImpl.printCode(0,"    return ret;\n");
                cpenumImpl.printCode(0,"}\n");
                cpenumImpl.linefeed();
            }

            // it's an enum...
            cpdom.printCode(0, "\n");
            if (!mShortDescription.empty())
            {
                cpdom.printCode(0, "// %s\n", mShortDescription.c_str());
            }
            if (!mLongDescription.empty())
            {
                cpdom.printCode(0, "// %s\n", mLongDescription.c_str());
            }
            cpdom.printCode(0, "enum %s\n", mName.c_str());
            cpdom.printCode(0, "{\n");
            for (auto &i : mItems)
            {
                cpdom.printCode(1, "%s,", i.mMember.c_str());
                cpdom.printCode(10, "// %s\n",
                    i.mShortDescription.c_str());
            }
            cpdom.printCode(0, "}\n");
            cpdom.printCode(0, "\n");
            return;
        }


        cpdom.printCode(0, "\n");
        if (!mShortDescription.empty())
        {
            cpdom.printCode(0, "// %s\n", mShortDescription.c_str());
        }
        if (!mLongDescription.empty())
        {
            cpdom.printCode(0, "// %s\n", mLongDescription.c_str());
        }
        cpdom.printCode(0, "type %s ", mName.c_str() );
        if (!mInheritsFrom.empty())
        {
            cpdom.printCode(0, " = %s & {\n", mInheritsFrom.c_str());
        }
        else
        {
            cpdom.printCode(0, " = {\n");
        }

        for (auto &i : mItems)
        {
            if ( i.mMember == "_skip_" )
            {
                continue;
            }
            if ( i.mInheritsFrom.empty() ) // we don't print out inherited items...
            {
                const char *type = i.mType.c_str();
                if ( strcmp(type,"u64") == 0 )
                {
                    type = "uint64";
                }
                else if (strcmp(type, "u32") == 0)
                {
                    type = "uint32";
                }
                else if (strcmp(type, "u16") == 0)
                {
                    type = "uint16";
                }
                else if (strcmp(type, "u8") == 0)
                {
                    type = "uint8";
                }
                else if (strcmp(type, "i64") == 0)
                {
                    type = "int64";
                }
                else if (strcmp(type, "i32") == 0)
                {
                    type = "int32";
                }
                else if (strcmp(type, "i16") == 0)
                {
                    type = "int16";
                }
                else if (strcmp(type, "i8") == 0)
                {
                    type = "int8";
                }
                else if ( strcmp(type,"bool") == 0 )
                {
                    type = "boolean";
                }
                if ( i.mIsMap )
                {
                    cpdom.printCode(1, "[%s%s]%s: %s%s // %s\n",
                        i.mMember.c_str(),
                        i.mMapType.c_str(),
                        i.mIsOptional != OptionalType::required ? "?" : "",
                        type,
                        i.mIsArray ? "[]" : "",
                        i.mShortDescription.c_str());
                }
                else
                {
                    cpdom.printCode(1,"%s%s: %s%s // %s\n", 
                        i.mMember.c_str(),
                        i.mIsOptional != OptionalType::required ? "?" : "", 
                        type, 
                        i.mIsArray ? "[]" : "",
                        i.mShortDescription.c_str());
                }
            }
        }


        cpdom.printCode(0,"}\n\n");

    }

    void saveCPP(CodePrinter &cpdom,
        StringVector &arrays,
        const StringVector &_needsReflection,
        StringVector &cloneObjects,
        const DOM &dom)
    {
        // check if an alias to an existing type was provided
        if (!mAlias.empty())
        {
            if (mAlias != mName)
            {
                // emit a typedef
                cpdom.printCode(0, "using %s = %s;\n", getClassNameString(mName, false), mAlias.c_str());
            }
            return;
        }

        if (_stricmp(mType.c_str(), "Enum") == 0)
        {
            // it's an enum...
            cpdom.printCode(0, "\n");
            if (!mShortDescription.empty())
            {
                cpdom.printCode(0, "// %s\n", mShortDescription.c_str());
            }
            if (!mLongDescription.empty())
            {
                cpdom.printCode(0, "// %s\n", mLongDescription.c_str());
            }
            cpdom.printCode(0, "enum class %s : uint64_t\n", mName.c_str());
            cpdom.printCode(0, "{\n");
            for (auto &i : mItems)
            {
                if ( i.mDefaultValue.empty() )
                {
                    cpdom.printCode(1, "%s,", i.mMember.c_str());
                }
                else
                {
                    cpdom.printCode(1, "%s=%s,", i.mMember.c_str(), i.mDefaultValue.c_str());
                }
                cpdom.printCode(10, "// %s\n",
                    i.mShortDescription.c_str());
            }
            cpdom.printCode(0, "};\n");
            cpdom.printCode(0, "\n");
            return;
        }

        bool hasInheritance = !mInheritsFrom.empty();
        bool hasArrays = false;
        bool hasPointer = false;
        bool hasStrings = false;


        // Before we can save out the class definition; we need to first see
        // if this class definition contains any arrays..
        for (auto &i : mItems)
        {
            if (strcmp(i.mType.c_str(), "string") == 0)
            {
                hasStrings = true;
            }
            if (i.mIsPointer)
            {
                hasPointer = true;
            }
            if (i.mIsArray) // ok.. this is an array of object!
            {
                hasArrays = true;
                // ok, now see if this array is already represented.
                bool found = false;
                for (auto &j : arrays)
                {
                    if (j == i.mType)
                    {
                        found = true;
                        break;
                    }
                }
                // If this array type not already represented; we need to define it
                if (!found)
                {
                    char temp[512];
                    strncpy(temp, i.mType.c_str(), 512);
                    temp[0] = upcase(temp[0]);
                    if (i.mIsPointer)
                    {
                    }
                    else if (!i.mIsString)
                    {
                        if (i.mNeedsReflection)
                        {
                        }
                        else
                        {
                            if (typeInfo(dom, i.mType.c_str()) != nullptr)
                            {
                            }
                            else
                            {
                            }
                        }
                    }
                    arrays.push_back(i.mType);
                }
            }
        }
        bool needsDef = false;
        if (hasArrays || hasInheritance || hasPointer || hasStrings || mClone || mNeedsReflection)
        {
            needsDef = true;
        }
        auto classDefinition = [this](CodePrinter &cp, bool isDef, bool needsDeepCopy, const StringVector &_needsReflection, StringVector &cloneObjects)
        {
            cp.printCode(0, "\n");
            if (!mShortDescription.empty())
            {
                cp.printCode(0, "// %s\n", mShortDescription.c_str());
            }
            if (!mLongDescription.empty())
            {
                cp.printCode(0, "// %s\n", mLongDescription.c_str());
            }
            cp.printCode(0, "class %s", getClassNameString(mName, isDef));
            bool firstInherit = true;
            if (!mInheritsFrom.empty())
            {
                firstInherit = false;
                cp.printCode(0, " : public %s", getClassNameString(mInheritsFrom.c_str(), isDef));
            }

            if ((mClone || mNeedsReflection) && isDef)
            {
                bool doCloneObject = true;
                // Don't add this if we inherit from an object which already has clone object!
                if (!mInheritsFrom.empty())
                {
                    for (auto &i : cloneObjects)
                    {
                        if (i == mInheritsFrom)
                        {
                            doCloneObject = false;
                            break;
                        }
                    }
                }
                if (doCloneObject)
                {
                    cp.printCode(0, "%s public CloneObject", firstInherit ? ":" : ",");
                    cloneObjects.push_back(mName);
                }
            }

            cp.printCode(0, "\n");
            cp.printCode(0, "{\n");

            cp.printCode(0, "public:\n");
        };

        bool needsDeepCopy = mInheritsFrom.empty() ? false : true;
        if (!needsDeepCopy)
        {
            needsDeepCopy = mClone | mNeedsReflection;
            if (!needsDeepCopy)
            {
                for (auto &i : mItems)
                {
                    if (i.mIsArray && i.mIsPointer)
                    {
                        needsDeepCopy = true;
                        break;
                    }
                    else if (i.mIsPointer)
                    {
                        needsDeepCopy = true;
                        break;
                    }
                }
            }
        }

        classDefinition(cpdom, false, needsDeepCopy, _needsReflection, cloneObjects);

        // Implementation of the class body..
        auto classBody = [this](CodePrinter &cp, bool isDef, bool needsDeepCopy, const StringVector &_needsReflection, const DOM &dom)
        {
            bool hasInheritedItemsWithDefaultValues = false;
            for (auto &i : mItems)
            {
                // If this is an 'inherited' data item. Don't clear it here
                // Because it was already handled in the initializer
                if (!i.mInheritsFrom.empty() && !i.mDefaultValue.empty())
                {
                    hasInheritedItemsWithDefaultValues = true;
                    break;
                }
            }
            {
                std::vector< std::string > nonMapItems;
                for (auto &k:mItems)
                {
                    if ( !k.mIsMap )
                    {
                        nonMapItems.push_back(k.mMember);
                    }
                }
                cp.linefeed();
                cp.printCode(1,"// Defines a method which returns 'true' if 'name' corresponds to a member variable of this class or base class.\n");
                cp.printCode(1,"bool isMember(const char *name) const\n");
                cp.printCode(1,"{\n");
                if ( mInheritsFrom.empty())
                {
                    cp.printCode(2,"bool ret = false;\n");
                }
                else
                {
                    cp.printCode(2, "bool ret = %s::isMember(name);\n", mInheritsFrom.c_str());
                }
                cp.printCode(2, "(name);\n");
                if ( nonMapItems.size() == 1 )
                {
                    cp.printCode(2,"if ( strcmp(name,\"%s\") == 0 )\n", nonMapItems[0].c_str());
                    cp.printCode(2,"{\n");
                    cp.printCode(3,"ret = true;\n");
                    cp.printCode(2,"}\n");
                }
                else if ( !nonMapItems.empty() )
                {
                    cp.printCode(2, "if ( strcmp(name,\"%s\") == 0 ||\n", nonMapItems[0].c_str());
                    for (size_t m=1; m<nonMapItems.size(); m++)
                    {
                        if ( (m+1) == nonMapItems.size() )
                        {
                            cp.printCode(3, "strcmp(name,\"%s\") == 0 )\n", nonMapItems[m].c_str());
                        }
                        else
                        {
                            cp.printCode(3, "strcmp(name,\"%s\") == 0 ||\n", nonMapItems[m].c_str());
                        }
                    }
                    cp.printCode(2, "{\n");
                    cp.printCode(3, "ret = true;\n");
                    cp.printCode(2, "}\n");
                }
                cp.printCode(2,"return ret;\n");
                cp.printCode(1,"}\n");
                cp.linefeed();
            }
            {
                cp.printCode(1,"//Defines the equality operator for this class and any sub-class.\n");
                cp.printCode(1,"bool operator==(const %s& other) const\n", mName.c_str());
                cp.printCode(1,"{\n");

                if ( mInheritsFrom.empty() )
                {
                    cp.printCode(2,"bool equal = true;\n");
                }
                else
                {
                    cp.printCode(2,"bool equal = %s(*this) == %s(other);\n", mInheritsFrom.c_str(), mInheritsFrom.c_str());
                }

                for (auto &i : mItems)
                {
                    if ( !i.mInheritsFrom.empty() )
                    {
                        continue;
                    }
                    std::string memberName = i.mMember;
                    if ( i.mIsMap )
                    {
                        memberName = "_" + memberName;
                    }
                    cp.printCode(2,"equal &= %s == other.%s;\n", memberName.c_str(), memberName.c_str());
                }

                cp.printCode(2,"return equal;\n");
                cp.printCode(1,"}\n");

                cp.linefeed();
                cp.printCode(1, "//Defines the not equal operator for this class and any sub-class.\n");
                cp.printCode(1, "bool operator!=(const %s& other) const\n", mName.c_str());
                cp.printCode(1, "{\n");
                cp.printCode(2,"return !((*this) == other);\n");
                cp.printCode(1, "}\n");
                cp.linefeed();

            }
            bool haveDefaultConstructor = false;
            if (mAssignment)
            {
                haveDefaultConstructor = true;
                cp.printCode(1, "// Declare the constructor.\n");
                cp.printCode(1, "%s() { }\n", getClassNameString(mName, isDef));
                cp.printCode(0, "\n");

                cp.printCode(1, "// Declare the assignment constructor.\n");
                cp.printCode(1, "%s(", getClassNameString(mName, isDef));
                bool needComma = false;
                for (auto &i : mItems)
                {
                    if (needComma)
                    {
                        cp.printCode(0, ",");
                    }
                    cp.printCode(0, "const %s &_%s", getCppTypeString(i.mType.c_str(), isDef), getMemberName(i.mMember, isDef, i.mIsMap));
                    needComma = true;
                }
                cp.printCode(0, ")\n");
                cp.printCode(1, "{\n");

                for (auto &i : mItems)
                {
                    cp.printCode(2, "%s = _%s;\n", getMemberName(i.mMember, isDef,i.mIsMap), getMemberName(i.mMember, isDef,i.mIsMap));
                }

                cp.printCode(1, "}\n");
                cp.printCode(0, "\n");
            }

            if (hasInheritedItemsWithDefaultValues)
            {
                if (!haveDefaultConstructor)
                {
                    haveDefaultConstructor = true;
                    cp.printCode(1, "// Declare the constructor.\n");
                    cp.printCode(1, "%s()\n", getClassNameString(mName, isDef));
                    cp.printCode(1, "{\n");
                    {
                        for (auto &i : mItems)
                        {
                            // If this is an 'inherited' data item. Don't clear it here
                            // Because it was already handled in the initializer
                            if (!i.mInheritsFrom.empty() && !i.mDefaultValue.empty())
                            {
                                cp.printCode(2, "%s%s::%s = %s;\n",
                                    i.mInheritsFrom.c_str(),
                                    isDef ? "Def" : "",
                                    getMemberName(i.mMember, isDef,i.mIsMap),
                                    getCppRValue(i, dom, isDef).c_str());
                            }
                        }
                    }
                    cp.printCode(1, "}\n");
                    cp.printCode(0, "\n");
                }
            }

            // see if any items are an array of pointers...
            bool hasArrayOfPointers = false;
            bool hasPointers = false;
            for (auto &i : mItems)
            {
                if (i.mIsArray && i.mIsPointer)
                {
                    hasArrayOfPointers = true;
                }
                else if (i.mIsPointer)
                {
                    hasPointers = true;
                }
            }
            bool needsDOMVector = false; // True if we need to declare the DOM vector
            for (auto &i : mItems)
            {
                // If this is an 'inherited' data item. Don't clear it here
                // Because it was already handled in the initializer
                if (!i.mInheritsFrom.empty())
                {
                    continue;
                }

                bool needsNull = false;
                bool needsArrayOperator = false;
                // Output the member variable declaration.
                if (i.mIsArray)
                {
                    if (i.mIsString)
                    {
                        if ( i.mIsMap )
                        {
                            cp.printCode(1, "std::unordered_map< std::string, std::string >");
                        }
                        else
                        {
                            cp.printCode(1, "std::vector< std::string >");
                        }
                        needsDOMVector = true;
                    }
                    else
                    {
                        needsDOMVector = true;
                        char temp[512];
                        strncpy(temp, getCppTypeString(i.mType.c_str(),false), 512);
//                        temp[0] = upcase(temp[0]);
                        char vectorName[512];
                        if ( i.mIsMap )
                        {
                            const char *mapType = i.mMapType.c_str();
                            if ( strcmp(mapType,"string") == 0 )
                            {
                                mapType = "std::string";
                            }
                            StandardType stype = getStandardType(i.mType.c_str());
                            if ( stype != StandardType::none )
                            {
                                STRING_HELPER::stringFormat(vectorName, 512, "std::unordered_map<%s, %s>", mapType, getCppTypeString(i.mType.c_str(), false));
                            }
                            else
                            {
                                STRING_HELPER::stringFormat(vectorName, 512, "std::unordered_map<%s, std::vector< %s>>", mapType, temp);
                            }
                            needsArrayOperator = true;
                        }
                        else
                        {
                            STRING_HELPER::stringFormat(vectorName, 512, "std::vector< %s>", temp);
                        }
                        cp.printCode(1, "%s", vectorName);
                    }
                }
                else
                {
                    assert( !i.mIsMap ); // not yet implemented, todo..
                    if (isDef && i.mIsPointer && !i.mIsArray)
                    {
                        needsNull = false;
                        cp.printCode(1, "%s%s", getCppTypeString(i.mType.c_str(), isDef),
                            isDef ? "Def" : "");
                    }
                    else
                    {
                        if ( i.mIsOptional  == OptionalType::optional )
                        {
                            cp.printCode(1, "codegen::optional< %s >", getCppTypeString(i.mType.c_str(),true));
                        }
                        else
                        {
                            cp.printCode(1, "%s", getCppTypeString(i.mType.c_str(), true));
                        }
                    }
                }

                if (i.mIsPointer && !i.mIsArray)
                {
                    needsNull = true;
                    cp.printCode(4, "*%s", getMemberName(i.mMember, isDef,i.mIsMap));
                }
                else
                {
                    cp.printCode(4, "%s", getMemberName(i.mMember, isDef, i.mIsMap));
                }

                if (i.mDefaultValue.empty())
                {
                    cp.printCode(0, "{ }");
                }
                else
                {
                    //cp.printCode(0, "{ %s }", getDefaultValueString(i.mDefaultValue.c_str()));
                    cp.printCode(0, "{ %s }", getCppValueInitializer(i, dom, isDef).c_str());
                }

                cp.printCode(0, ";");

                cp.printCode(16, "// %s\n", i.mShortDescription.c_str());

                if ( needsArrayOperator )
                {
                    cp.linefeed();
                    cp.printCode(1,"// Defines the array operator to access this map\n");
                    const char *mapType = i.mMapType.c_str();
                    if (strcmp(mapType, "string") == 0)
                    {
                        mapType = "std::string";
                    }
                    char temp[512];
                    strncpy(temp, i.mType.c_str(), 512);
                    temp[0] = upcase(temp[0]);
                    StandardType stype = getStandardType(i.mType.c_str());
                    if ( stype == StandardType::none )
                    {
                        cp.printCode(1, "std::vector< %s>& operator[](const %s& x)\n", temp, mapType);
                    }
                    else
                    {
                        cp.printCode(1, "%s operator[](const %s& x)\n", getCppTypeString(i.mType.c_str(),false), mapType);
                    }
                    cp.printCode(1,"{\n");
                    cp.printCode(2,"return _%s[x];\n", i.mMember.c_str());
                    cp.printCode(1,"}\n");
                    cp.linefeed();
                }

            }
            if (isDef)
            {
                cp.printCode(0, "private:\n");
                cp.printCode(1, "%s", getClassNameString(mName, false));
                cp.printCode(4, "mDOM; // Declare the DOM version.\n");
            }
            if (needsDOMVector)
            {
                for (auto &i : mItems)
                {
                    // If this is an 'inherited' data item. Don't clear it here
                    // Because it was already handled in the initializer
                    if (!i.mInheritsFrom.empty())
                    {
                        continue;
                    }

                    if (!i.needsReflection() && isDef)
                        continue;

                    // Output the member variable declaration.
                    if (i.mIsArray)
                    {
                        if (isDef)
                        {
                            if (i.mIsString)
                            {
                                cp.printCode(1, "ConstCharVector");
                                cp.printCode(4, "%sDef; // Scratch array for const char pointers.\n",
                                    getMemberName(i.mMember, true,i.mIsMap));
                            }
                            else
                            {
                                if (i.mNeedsReflection)
                                {
                                    char temp[512];
                                    strncpy(temp, i.mType.c_str(), 512);
                                    temp[0] = upcase(temp[0]);
                                    char vectorName[512];
                                    STRING_HELPER::stringFormat(vectorName, 512, "std::vector<%s>", temp);
                                    cp.printCode(1, "%s", vectorName);
                                    cp.printCode(4, "%sDOM; // Scratch array for const char pointers.\n",
                                        getMemberName(i.mMember, true,i.mIsMap));
                                }
                            }
                        }
                    }
                }
            }
        };

        if (needsDef)
        {
//            classBody(cpimpl, true, needsDeepCopy, _needsReflection, dom);
        }

        classBody(cpdom, false, needsDeepCopy, _needsReflection, dom);

        auto endClass = [this](CodePrinter &cp)
        {
            cp.printCode(0, "};\n");
            cp.printCode(0, "\n");
        };
        if (needsDef)
        {
//            endClass(cpimpl);
        }
        endClass(cpdom);
    }

	void saveCPP(CodePrinter &cpimpl,
				 CodePrinter &cpdom,
				 StringVector &arrays,
				 const StringVector &_needsReflection,
				 StringVector &cloneObjects,
				 const DOM &dom)
	{
		// check if an alias to an existing type was provided
		if (!mAlias.empty())
		{
			if (mAlias != mName)
			{
				// emit a typedef
				cpdom.printCode(0, "using %s = %s;\n", getClassNameString(mName, false), mAlias.c_str());
			}
			return;
		}

		if (_stricmp(mType.c_str(), "Enum") == 0)
		{
			// it's an enum...
			cpdom.printCode(0, "\n");
			if (!mShortDescription.empty())
			{
				cpdom.printCode(0, "// %s\n", mShortDescription.c_str());
			}
			if (!mLongDescription.empty())
			{
				cpdom.printCode(0, "// %s\n", mLongDescription.c_str());
			}
			cpdom.printCode(0, "enum class %s : uint32_t\n", mName.c_str());
			cpdom.printCode(0, "{\n");
			for (auto &i : mItems)
			{
				cpdom.printCode(1, "%s,",i.mMember.c_str());
				cpdom.printCode(10, "// %s\n",
					i.mShortDescription.c_str());
			}
			cpdom.printCode(0, "};\n");
			cpdom.printCode(0, "\n");
			return;
		}

		bool hasInheritance = !mInheritsFrom.empty();
		bool hasArrays = false;
		bool hasPointer = false;
		bool hasStrings = false;


		// Before we can save out the class definition; we need to first see
		// if this class definition contains any arrays..
		for (auto &i : mItems)
		{
			if (strcmp(i.mType.c_str(), "string") == 0)
			{
				hasStrings = true;
			}
			if (i.mIsPointer)
			{
				hasPointer = true;
			}
			if (i.mIsArray) // ok.. this is an array of object!
			{
				hasArrays = true;
				// ok, now see if this array is already represented.
				bool found = false;
				for (auto &j : arrays)
				{
					if (j == i.mType)
					{
						found = true;
						break;
					}
				}
				// If this array type not already represented; we need to define it
				if (!found)
				{
					char temp[512];
					strncpy(temp, i.mType.c_str(),512);
					temp[0] = upcase(temp[0]);
					if (i.mIsPointer)
					{
						if (!i.mIsString)
						{
						}
					}
					else if ( !i.mIsString )
					{
						if (i.mNeedsReflection)
						{
						}
						else
						{
							if (typeInfo(dom, i.mType.c_str()) != nullptr)
							{
							}
							else
							{
							}
						}
					}
					arrays.push_back(i.mType);
				}
			}
		}
		bool needsDef = false;
		if (hasArrays || hasInheritance || hasPointer || hasStrings || mClone || mNeedsReflection)
		{
			needsDef = true;
		}
		auto classDefinition = [this](CodePrinter &cp, bool isDef,bool needsDeepCopy,const StringVector &_needsReflection,StringVector &cloneObjects)
		{
			cp.printCode(0, "\n");
			if (!mShortDescription.empty())
			{
				cp.printCode(0, "// %s\n", mShortDescription.c_str());
			}
			if (!mLongDescription.empty())
			{
				cp.printCode(0, "// %s\n", mLongDescription.c_str());
			}
			cp.printCode(0, "class %s", getClassNameString(mName,isDef)); 
			bool firstInherit = true;
			if (!mInheritsFrom.empty() )
			{
				firstInherit = false;
				cp.printCode(0, " : public %s", getClassNameString(mInheritsFrom.c_str(),isDef));
			}

			if ((mClone || mNeedsReflection) && isDef )
			{
				bool doCloneObject = true;
				// Don't add this if we inherit from an object which already has clone object!
				if (!mInheritsFrom.empty())
				{
					for (auto &i:cloneObjects)
					{
						if (i == mInheritsFrom)
						{
							doCloneObject = false;
							break;
						}
					}
				}
				if (doCloneObject)
				{
					cp.printCode(0, "%s public CloneObject", firstInherit ? ":" : ",");
					cloneObjects.push_back(mName);
				}
			}

			cp.printCode(0, "\n");
			cp.printCode(0, "{\n");

			cp.printCode(0, "public:\n");
		};

		bool needsDeepCopy = mInheritsFrom.empty() ? false : true;
		if ( !needsDeepCopy )
		{
			needsDeepCopy = mClone | mNeedsReflection;
			if (!needsDeepCopy)
			{
				for (auto &i : mItems)
				{
					if (i.mIsArray && i.mIsPointer)
					{
						needsDeepCopy = true;
						break;
					}
					else if (i.mIsPointer)
					{
						needsDeepCopy = true;
						break;
					}
				}
			}
		}

		// If we need an implementation class
		if (needsDef)
		{
			classDefinition(cpimpl, true, needsDeepCopy,_needsReflection,cloneObjects);
		}
		classDefinition(cpdom, false, needsDeepCopy,_needsReflection,cloneObjects);

		// Implementation of the class body..
		auto classBody = [this](CodePrinter &cp, bool isDef, bool needsDeepCopy, const StringVector &_needsReflection, const DOM &dom)
		{
			bool hasInheritedItemsWithDefaultValues = false;
			for (auto &i : mItems)
			{
				// If this is an 'inherited' data item. Don't clear it here
				// Because it was already handled in the initializer
				if (!i.mInheritsFrom.empty() && !i.mDefaultValue.empty())
				{
					hasInheritedItemsWithDefaultValues = true;
					break;
				}
			}
			bool haveDefaultConstructor = false;
			if (mAssignment)
			{
				haveDefaultConstructor = true;
				cp.printCode(1, "// Declare the constructor.\n");
				cp.printCode(1, "%s() { }\n", getClassNameString(mName, isDef));
				cp.printCode(0, "\n");

				cp.printCode(1, "// Declare the assignment constructor.\n");
				cp.printCode(1, "%s(", getClassNameString(mName, isDef));
				bool needComma = false;
				for (auto &i : mItems)
				{
					if (needComma)
					{
						cp.printCode(0, ",");
					}
					cp.printCode(0, "const %s &_%s", getCppTypeString(i.mType.c_str(),isDef), getMemberName(i.mMember, isDef,i.mIsMap));
					needComma = true;
				}
				cp.printCode(0, ")\n");
				cp.printCode(1, "{\n");

				for (auto &i : mItems)
				{
					cp.printCode(2, "%s = _%s;\n", getMemberName(i.mMember, isDef,i.mIsMap), getMemberName(i.mMember, isDef,i.mIsMap));
				}

				cp.printCode(1, "}\n");
				cp.printCode(0, "\n");
			}

			if (hasInheritedItemsWithDefaultValues)
			{
				if (!haveDefaultConstructor)
				{
					haveDefaultConstructor = true;
					cp.printCode(1, "// Declare the constructor.\n");
					cp.printCode(1, "%s()\n", getClassNameString(mName, isDef));
					cp.printCode(1, "{\n");
					{
						for (auto &i : mItems)
						{
							// If this is an 'inherited' data item. Don't clear it here
							// Because it was already handled in the initializer
							if (!i.mInheritsFrom.empty() && !i.mDefaultValue.empty())
							{
								cp.printCode(2, "%s%s::%s = %s;\n",
									i.mInheritsFrom.c_str(),
									isDef ? "Def" : "",
									getMemberName(i.mMember, isDef,i.mIsMap),
									getCppRValue(i, dom, isDef).c_str());
							}
						}
					}
					cp.printCode(1, "}\n");
					cp.printCode(0, "\n");
				}
			}

			// see if any items are an array of pointers...
			bool hasArrayOfPointers = false;
			bool hasPointers = false;
			for (auto &i : mItems)
			{
				if (i.mIsArray && i.mIsPointer)
				{
					hasArrayOfPointers = true;
				}
				else if (i.mIsPointer)
				{
					hasPointers = true;
				}
			}
			if (!haveDefaultConstructor && isDef)
			{
				haveDefaultConstructor = true;
				cp.printCode(0, "\n");
				cp.printCode(1, "// Declare the constructor.\n");
				cp.printCode(1, "%s() { }\n", getClassNameString(mName, isDef));
				cp.printCode(0, "\n");
			}
			if (hasArrayOfPointers || hasPointers)
			{
				if (needsDeepCopy)
				{
					if (isDef)
					{
						cp.printCode(0, "\n");
						cp.printCode(1, "// Declare the virtual destructor; cleanup any pointers or arrays of pointers\n");
						cp.printCode(1, "virtual ~%s() override\n", getClassNameString(mName, isDef));
						cp.printCode(1, "{\n");

						for (auto &i : mItems)
						{
							if (i.mIsArray && i.mIsPointer)
							{
								cp.printCode(2, "for (auto &i:%s) delete i; // Delete all of the object pointers in this array\n", getMemberName(i.mMember, isDef,i.mIsMap));
							}
							else if (i.mIsPointer)
							{
								cp.printCode(2, "delete %s; // Delete this object\n", getMemberName(i.mMember, isDef,i.mIsMap));
							}
						}
						cp.printCode(1, "}\n");
						cp.printCode(0, "\n");
					}
				}
			}
			else if (isDef /*&& (!mInheritsFrom.empty() || !mChildren.empty())*/)
			{
				// assume all Def types that are part of an inheritance hierarchy need a virtual destructor
				cp.printCode(0, "\n");
				cp.printCode(1, "// Declare the virtual destructor.\n");
				cp.printCode(1, "virtual ~%s() override\n", getClassNameString(mName, isDef));
				cp.printCode(1, "{\n");
				cp.printCode(1, "}\n");
				cp.printCode(0, "\n");
			}
			// create the deep copy constructors and such
			if (needsDeepCopy)
			{
				// Do the deep copy constructor and assignment operators
				if (isDef)
				{
					cp.printCode(0, "\n");
					cp.printCode(1, "// Declare the deep copy constructor; handles copying pointers and pointer arrays\n");
					cp.printCode(1, "%s(const %s &other)\n", getClassNameString(mName, isDef), getClassNameString(mName, isDef));
					cp.printCode(1, "{\n");
					cp.printCode(2, "*this = other;\n");
					cp.printCode(1, "}\n");
					cp.printCode(0, "\n");

					if (mInheritsFrom.empty())
					{
						cp.printCode(0, "\n");
						cp.printCode(1, "virtual %s * get%s(void) // Declare virtual method to return DOM version of base class.\n", mName.c_str(), mName.c_str());
						cp.printCode(1, "{\n");
						cp.printCode(2, "return &mDOM; // return the address of the DOM.\n");
						cp.printCode(1, "}\n");
						cp.printCode(0, "\n");
					}
					else
					{
						cp.printCode(0, "\n");
						cp.printCode(1, "virtual %s * get%s(void) override // Declare virtual method to return DOM version of base class.\n", mInheritsFrom.c_str(), mInheritsFrom.c_str());
						cp.printCode(1, "{\n");
						cp.printCode(2, "return &mDOM; // return the address of the DOM.\n", mInheritsFrom.c_str());
						cp.printCode(1, "}\n");
						cp.printCode(0, "\n");

						cp.printCode(0, "\n");
						cp.printCode(1, "virtual %s * get%s(void) // Declare virtual method to return the DOM version\n", mName.c_str(), mName.c_str());
						cp.printCode(1, "{\n");
						cp.printCode(2, "return &mDOM; // return the address of the DOM.\n");
						cp.printCode(1, "}\n");
						cp.printCode(0, "\n");

						if (!mMultipleInherticance.empty())
						{
							cp.printCode(0, "\n");
							cp.printCode(1, "virtual %s * get%s(void) override // Declare virtual method to return the DOM version based on multiple inheritance\n", mMultipleInherticance.c_str(), mMultipleInherticance.c_str());
							cp.printCode(1, "{\n");
							cp.printCode(2, "return &mDOM; // return the address of the DOM.\n");
							cp.printCode(1, "}\n");
							cp.printCode(0, "\n");
						}

					}

					// **********************************************
					// Start of the clone code
					// **********************************************
					cp.printCode(0, "\n");
					cp.printCode(1, "// Declare the virtual clone method using a deep copy\n");
					cp.printCode(1, "virtual CloneObject* clone() const override\n");
					cp.printCode(1, "{\n");
					cp.printCode(2, "return new %s(*this);\n", getClassNameString(mName, isDef));
					cp.printCode(1, "}\n");
					cp.printCode(0, "\n");


					cp.printCode(1, "// Declare and implement the deep copy assignment operator\n");
					cp.printCode(1, "%s& operator=(const %s& other)\n", getClassNameString(mName, isDef), getClassNameString(mName, isDef));
					cp.printCode(1, "{\n");
					cp.printCode(2, "if (this != &other )\n");
					cp.printCode(2, "{\n");

					if (!mInheritsFrom.empty())
					{
						cp.printCode(3, "%sDef::operator=(other);\n", mInheritsFrom.c_str());
					}

					for (auto &i : mItems)
					{
						if (i.mIsArray && i.mIsPointer)
						{
							cp.printCode(3, "for (auto &i:%s) delete i; // Delete all of the object pointers in this array\n", getMemberName(i.mMember, isDef,i.mIsMap));
							cp.printCode(3, "%s.clear(); // Clear the current array\n", getMemberName(i.mMember, isDef,i.mIsMap));
							cp.printCode(3, "%s.reserve(other.%s.size()); // Reserve number of items for the new array\n", getMemberName(i.mMember, isDef,i.mIsMap), getMemberName(i.mMember, isDef,i.mIsMap));
							cp.printCode(3, "for (auto &i:other.%s) %s.push_back( static_cast< %sDef *>(i->clone())); // Deep copy object pointers into the array\n", getMemberName(i.mMember, isDef,i.mIsMap), getMemberName(i.mMember, isDef,i.mIsMap), i.mType.c_str());
						}
						else if (i.mIsPointer)
						{
							cp.printCode(3, "delete %s; // delete any previous pointer.\n", getMemberName(i.mMember, isDef,i.mIsMap));
							cp.printCode(3, "%s = nullptr; // set the pointer to null.\n", getMemberName(i.mMember, isDef,i.mIsMap));
							cp.printCode(3, "if ( other.%s )\n", getMemberName(i.mMember, isDef,i.mIsMap));
							cp.printCode(3, "{\n");
							cp.printCode(4, "%s = static_cast<%sDef *>(other.%s->clone()); // perform the deep copy and assignment here\n",
								getMemberName(i.mMember, isDef,i.mIsMap),
								i.mType.c_str(),
								getMemberName(i.mMember, isDef,i.mIsMap));
							cp.printCode(3, "}\n");
						}
						else if (i.mInheritsFrom.empty())
						{
							cp.printCode(3, "%s = other.%s;\n", getMemberName(i.mMember, isDef,i.mIsMap), getMemberName(i.mMember, isDef,i.mIsMap));
						}
					}

					cp.printCode(2, "}\n");
					cp.printCode(2, "return *this;\n");
					cp.printCode(1, "}\n");
					cp.printCode(0, "\n");
				}
				// **********************************************
				// End of the clone code
				// **********************************************
			}
			if ( isDef )
			{
				// initDOM code
				{
					cp.printCode(1, "// Declare and implement the initDOM method\n");
					cp.printCode(1, "virtual void initDOM(void) override\n", getClassNameString(mName, isDef), getClassNameString(mName, isDef));
					cp.printCode(1, "{\n");

					if (!mInheritsFrom.empty())
					{
						cp.printCode(2, "// Initialize the DOM for the base class.\n");
						cp.printCode(2, "%sDef::initDOM();\n", mInheritsFrom.c_str());
						cp.printCode(2, "// Copy the elements from the base class DOM to our reflection DOM\n");
						cp.printCode(2, "{\n");
						cp.printCode(3, "%s *dom = static_cast< %s *>(&mDOM); // Get the DOM base class.\n",
							mInheritsFrom.c_str(),
							mInheritsFrom.c_str());
						cp.printCode(3, "*dom = *(%sDef::get%s()); // Assign the base class DOM components.\n",
							mInheritsFrom.c_str(),
							mInheritsFrom.c_str());
						cp.printCode(2, "}\n");
					}
					if (strcmp(mName.c_str(), "Node") == 0)
					{
						printf("debug me");
					}
					for (auto &i : mItems)
					{
						const char *arrayPostFix = i.mNeedsReflection ? "DOM" : "";
						if (!i.needsReflection() && classNeedsReflection(i.mType, _needsReflection))
						{
							cp.printCode(2, "{\n");
							cp.printCode(3, "%sDef *impl = static_cast< %sDef *>(&%s); // static cast to the implementation class.\n",
								i.mType.c_str(),
								i.mType.c_str(),
								getMemberName(i.mMember, true,i.mIsMap));
							cp.printCode(3, "impl->initDOM(); // Initialize DOM components of member variable.\n");
							cp.printCode(3, "mDOM.%s = *impl->get%s(); // Copy the DOM struct values.\n",
								i.mMember.c_str(),
								i.mType.c_str());
							cp.printCode(2, "}\n");
						}
						else if (i.mIsString)
						{
							if (i.mIsArray)
							{
								arrayPostFix = "Def";
								cp.printCode(2, "// Initialize the const char * array from the array of std::strings vector %s\n",
									getMemberName(i.mMember, true,i.mIsMap));

								cp.printCode(2, "%sDef.clear(); // Clear previous array definition.\n",
									getMemberName(i.mMember, true,i.mIsMap));


								cp.printCode(2, "%sDef.reserve(%s.size()); // Reserve room for string pointers.\n",
									getMemberName(i.mMember, true,i.mIsMap),
									getMemberName(i.mMember, true,i.mIsMap));
								cp.printCode(2, "for (auto &i: %s) // For each std::string\n", getMemberName(i.mMember, true,i.mIsMap));
								cp.printCode(3, "%sDef.push_back( i.c_str() ); // Add the const char * for the string.\n", getMemberName(i.mMember, true,i.mIsMap));
								cp.printCode(2, "mDOM.%sCount = uint32_t(%s%s.size()); // Assign the number of strings\n",
									getMemberName(i.mMember, false,i.mIsMap),
									getMemberName(i.mMember, true,i.mIsMap),
									arrayPostFix);

								cp.printCode(2, "mDOM.%s = mDOM.%sCount ? &%s%s[0] : nullptr; // Assign the pointer array.\n",
									getMemberName(i.mMember, false,i.mIsMap),
									getMemberName(i.mMember, false,i.mIsMap),
									getMemberName(i.mMember, true,i.mIsMap),
									arrayPostFix);
							}
							else
							{
								cp.printCode(2, "mDOM.%s = %s.c_str(); // Assign the current string pointer.\n", 
									getMemberName(i.mMember, false,i.mIsMap), 
									getMemberName(i.mMember, true,i.mIsMap));
							}
						}
						else if (i.mIsArray && i.mIsPointer)
						{
							// If it is an array of pointers...
							cp.printCode(2, "%sDOM.clear();\n", getMemberName(i.mMember, true,i.mIsMap));
							cp.printCode(2, "%sDOM.reserve( %s.size() );\n",
								getMemberName(i.mMember, true,i.mIsMap),
								getMemberName(i.mMember, true,i.mIsMap));
							cp.printCode(2, "for (auto &i:%s)\n", getMemberName(i.mMember, true,i.mIsMap));
							cp.printCode(2, "{\n");
							cp.printCode(3, "i->initDOM();\n");
							cp.printCode(3, "%sDOM.push_back( i->get%s() );\n",
								getMemberName(i.mMember, true,i.mIsMap),
								i.mType.c_str());
							cp.printCode(2, "}\n");
							cp.printCode(2, "mDOM.%sCount = uint32_t(%s%s.size()); // assign the number of items in the array.\n",
								getMemberName(i.mMember, false,i.mIsMap),
								getMemberName(i.mMember, true,i.mIsMap),
								arrayPostFix);
							cp.printCode(2, "mDOM.%s = mDOM.%sCount ? &%s%s[0] : nullptr; // Assign the pointer array\n",
								getMemberName(i.mMember, false,i.mIsMap),
								getMemberName(i.mMember, false,i.mIsMap),
								getMemberName(i.mMember, true,i.mIsMap),
								arrayPostFix);
						}
						else if (i.mIsArray)
						{

							// If the array needs a deep copy..
							if (i.mNeedsReflection )
							{
								cp.printCode(2, "%sDOM.clear();\n", getMemberName(i.mMember, true,i.mIsMap));
								cp.printCode(2, "%sDOM.reserve( %s.size() );\n",
									getMemberName(i.mMember, true,i.mIsMap),
									getMemberName(i.mMember, true,i.mIsMap));
								cp.printCode(2, "for (auto &i:%s)\n", getMemberName(i.mMember, true,i.mIsMap));
								cp.printCode(2, "{\n");
								cp.printCode(3, "i.initDOM();\n");
								cp.printCode(3, "%sDOM.push_back( *(i.get%s()) );\n",
									getMemberName(i.mMember, true,i.mIsMap),
									i.mType.c_str());
								cp.printCode(2, "}\n");
							}


							cp.printCode(2, "mDOM.%sCount = uint32_t(%s%s.size()); // assign the number of items in the array.\n",
								getMemberName(i.mMember, false,i.mIsMap),
								getMemberName(i.mMember, true,i.mIsMap),
								arrayPostFix);
							cp.printCode(2, "mDOM.%s = mDOM.%sCount ? &%s%s[0] : nullptr; // Assign the pointer array\n",
								getMemberName(i.mMember, false,i.mIsMap),
								getMemberName(i.mMember, false,i.mIsMap),
								getMemberName(i.mMember, true,i.mIsMap),
								arrayPostFix);
						}
						else if (i.mIsPointer)
						{
							cp.printCode(2, "if ( %s )\n", getMemberName(i.mMember, true,i.mIsMap));
							cp.printCode(2, "{\n");
							cp.printCode(3, "%s->initDOM(); // Initialize any DOM components of this object.\n",
								getMemberName(i.mMember, true,i.mIsMap));

							cp.printCode(3, "mDOM.%s = %s->get%s(); // assign the DOM reflection pointer.\n",
								getMemberName(i.mMember, false,i.mIsMap),
								getMemberName(i.mMember, true,i.mIsMap),
								i.mType.c_str());

							cp.printCode(2, "}\n");
						}
						else
						{
							cp.printCode(2, "mDOM.%s = %s; // Simple member variable assignment to the DOM reflection: %s\n",
								getMemberName(i.mMember, false,i.mIsMap),
								getMemberName(i.mMember, true,i.mIsMap),
								getMemberName(i.mMember, false,i.mIsMap));
						}
					}
					cp.printCode(1, "}\n");
					cp.printCode(0, "\n");
				}



				// Do the move constructor and assignment operators
				if ( isDef )
				{
					cp.printCode(0, "\n");
					cp.printCode(1, "// Declare the move constructor; handles copying pointers and pointer arrays\n");
					cp.printCode(1, "%s(%s &&other)\n", getClassNameString(mName,isDef), getClassNameString(mName,isDef));
					cp.printCode(1, "{\n");
					cp.printCode(2, "*this = std::move(other);\n");
					cp.printCode(1, "}\n");
					cp.printCode(0, "\n");

					cp.printCode(1, "// Declare and implement the move assignment operator\n");
					cp.printCode(1, "%s& operator=(%s&& other)\n", getClassNameString(mName,isDef), getClassNameString(mName,isDef));
					cp.printCode(1, "{\n");
					cp.printCode(2, "if (this != &other )\n");
					cp.printCode(2, "{\n");

					if (!mInheritsFrom.empty())
					{
						cp.printCode(3, "%sDef::operator=(std::move(other));\n", mInheritsFrom.c_str());
					}

					for (auto &i : mItems)
					{
						if (i.mIsArray && i.mIsPointer)
						{
							cp.printCode(3, "%s = other.%s;\n", getMemberName(i.mMember,isDef,i.mIsMap), getMemberName(i.mMember,isDef,i.mIsMap));
							cp.printCode(3, "other.%s.clear(); // Clear the 'other' array now that we have moved it\n", getMemberName(i.mMember, isDef,i.mIsMap));
						}
						else if (i.mIsPointer)
						{
							cp.printCode(3, "%s = other.%s;\n", getMemberName(i.mMember, isDef,i.mIsMap), getMemberName(i.mMember, isDef,i.mIsMap));
							cp.printCode(3, "other.%s = nullptr; // Set 'other' pointer to null since we have moved it\n", getMemberName(i.mMember, isDef,i.mIsMap));
						}
						else if (i.mInheritsFrom.empty())
						{
							cp.printCode(3, "%s = other.%s;\n", getMemberName(i.mMember, isDef,i.mIsMap), getMemberName(i.mMember, isDef,i.mIsMap));
						}
					}
					cp.printCode(2, "}\n");
					cp.printCode(2, "return *this;\n");
					cp.printCode(1, "}\n");
					cp.printCode(0, "\n");
				}
			}

			bool needsDOMVector = false; // True if we need to declare the DOM vector
			for (auto &i : mItems)
			{
				// If this is an 'inherited' data item. Don't clear it here
				// Because it was already handled in the initializer
				if (!i.mInheritsFrom.empty())
				{
					continue;
				}

				bool needsNull = false;

				// Output the member variable declaration.
				if (i.mIsArray)
				{
					if (isDef)
					{
						if (i.mIsString)
						{
							cp.printCode(1, "std::vector< std::string >");
							needsDOMVector = true;
						}
						else
						{
							needsDOMVector = true;
							char temp[512];
							strncpy(temp, i.mType.c_str(), 512);
							temp[0] = upcase(temp[0]);
							char vectorName[512];
							STRING_HELPER::stringFormat(vectorName, 512, "std::vector<%s>", temp);
							cp.printCode(1, "%s", vectorName);
						}
					}
					else
					{
						needsNull = true;
						cp.printCode(1, "uint32_t");
						cp.printCode(4, "%sCount { 0 };\n", getMemberName(i.mMember,isDef,i.mIsMap) );
						if (i.mIsString && !isDef)
						{
							cp.printCode(1, "%s*", getCppTypeString(i.mType.c_str(), isDef));
						}
						else
						{
							if (i.mIsPointer)
							{
								cp.printCode(1, "%s**", getCppTypeString(i.mType.c_str(), isDef));
							}
							else
							{
								cp.printCode(1, "%s*", getCppTypeString(i.mType.c_str(), isDef));
							}
						}
					}
				}
				else
				{
					if (isDef && i.mIsPointer && !i.mIsArray)
					{
						needsNull = true;
						cp.printCode(1, "%s%s", getCppTypeString(i.mType.c_str(), isDef),
							isDef ? "Def" : "");
					}
					else
					{
						if (i.mIsString && !isDef )
						{
							needsNull = true;
						}
						if (i.mNeedsReflection && !i.mIsString && isDef )
						{
							cp.printCode(1, "%sDef", getCppTypeString(i.mType.c_str(), isDef));
						}
						else
						{
							if (isDef && typeInfo(dom, i.mType) != nullptr)
							{
								cp.printCode(1, "%s", getCppTypeString(i.mType.c_str(), isDef));
							}
							else
							{
								cp.printCode(1, "%s", getCppTypeString(i.mType.c_str(), isDef));
							}
						}
					}
				}

				if (i.mIsPointer && !i.mIsArray)
				{
					needsNull = true;
					cp.printCode(4, "*%s", getMemberName(i.mMember, isDef,i.mIsMap));
				}
				else
				{
					cp.printCode(4, "%s", getMemberName(i.mMember, isDef,i.mIsMap));
				}

				if (i.mDefaultValue.empty())
				{
					if ( needsNull )
					{
						cp.printCode(0, "{ nullptr }");
					}
				}
				else
				{
					//cp.printCode(0, "{ %s }", getDefaultValueString(i.mDefaultValue.c_str()));
					cp.printCode(0, "{ %s }", getCppValueInitializer(i, dom, isDef).c_str());
				}

				cp.printCode(0, ";");

				cp.printCode(16, "// %s\n", i.mShortDescription.c_str());

			}
			if (isDef)
			{
				cp.printCode(0, "private:\n");
				cp.printCode(1, "%s", getClassNameString(mName, false));
				cp.printCode(4, "mDOM; // Declare the DOM version.\n");
			}
			if (needsDOMVector)
			{
				cp.printCode(0, "// Declare private temporary array(s) to hold flat DOM version of arrays.\n");

				for (auto &i : mItems)
				{
					// If this is an 'inherited' data item. Don't clear it here
					// Because it was already handled in the initializer
					if (!i.mInheritsFrom.empty())
					{
						continue;
					}

					if (!i.needsReflection() && isDef)
						continue;

					// Output the member variable declaration.
					if (i.mIsArray)
					{
						if (isDef)
						{
							if (i.mIsString)
							{
								cp.printCode(1, "ConstCharVector");
								cp.printCode(4, "%sDef; // Scratch array for const char pointers.\n",
									getMemberName(i.mMember, true,i.mIsMap));
							}
							else
							{
								if (i.mNeedsReflection)
								{
									char temp[512];
									strncpy(temp, i.mType.c_str(), 512);
									temp[0] = upcase(temp[0]);
									char vectorName[512];
									STRING_HELPER::stringFormat(vectorName, 512, "std::vector<%s>", temp);
									cp.printCode(1, "%s", vectorName);
									cp.printCode(4, "%sDOM; // Scratch array for const char pointers.\n",
										getMemberName(i.mMember, true,i.mIsMap));
								}
							}
						}
					}
				}
			}
		};

		if (needsDef)
		{
			classBody(cpimpl, true, needsDeepCopy,_needsReflection,dom);
		}

		classBody(cpdom, false, needsDeepCopy,_needsReflection,dom);

		auto endClass = [this](CodePrinter &cp)
		{
			cp.printCode(0, "};\n");
			cp.printCode(0, "\n");
		};
		if (needsDef)
		{
			endClass(cpimpl);
		}
		endClass(cpdom);
	}

	void savePython(CodePrinter &cp, const DOM &dom)
	{
		if (_stricmp(mType.c_str(), "Enum") == 0)
		{
			// it's an enum...
			// For now, implement enums as global integer constants.
			// Unique enum name prefixes should ensure that no name clashes occur.
			cp.printCode(0, "\n");
			if (!mShortDescription.empty())
			{
				cp.printCode(0, "# %s\n", mShortDescription.c_str());
			}
			if (!mLongDescription.empty())
			{
				cp.printCode(0, "# %s\n", mLongDescription.c_str());
			}
			// define enum values
			uint32_t id = 0;
			for (auto &i : mItems)
			{
				cp.printCode(0, "%s = %d", i.mMember.c_str(), id);
				cp.printCode(10, "# %s\n",
					i.mShortDescription.c_str());
				id++;
			}
			// add a lookup table that converts the enums to strings
			cp.printCode(0, "\n");
			cp.printCode(0, "%s_strings = [\n", mName.c_str());
			for (auto &i : mItems)
			{
				cp.printCode(1, "'%s',\n", i.mMember.c_str());
			}
			cp.printCode(0, "]\n");

			cp.printCode(0, "\n");
			return;
		}

		// write class declaration
		cp.printCode(0, "\n");
		cp.printCode(0, "class %s", mName.c_str());
		if (!mInheritsFrom.empty())
		{
			cp.printCode(0, "(%s)", mInheritsFrom.c_str());
		}
		cp.printCode(0, ":\n");

		// write docstring
		if (!mShortDescription.empty() || !mLongDescription.empty())
		{
			cp.printCode(1, "\"\"\"");
			if (!mShortDescription.empty())
			{
				cp.printCode(1, "%s", mShortDescription.c_str());
			}
			if (!mLongDescription.empty())
			{
				if (!mShortDescription.empty())
				{
					cp.printCode(1, "\n\n");
				}
				cp.printCode(1, "%s\n", mLongDescription.c_str());
			}
			cp.printCode(1, "\"\"\"\n");
		}

		//cp.printCode(1, "pass\n");

		constexpr bool DBG_TRACE = false;
		if (DBG_TRACE)
		{
			printf("class %s (%s)\n", mName.c_str(), mInheritsFrom.c_str());
			for (auto &i : mItems)
			{
				printf("  %s: type=%s, default=%s, inheritsFrom=%s\n", i.mMember.c_str(), i.mType.c_str(), i.mDefaultValue.c_str(), i.mInheritsFrom.c_str());
			}
			printf("\n");
		}

		std::vector<MemberVariable*> inheritedMembersWithDefaultValues;
		std::vector<MemberVariable*> localMembers;

		for (auto &i : mItems)
		{
			if (i.mInheritsFrom.empty())
			{
				// collect local members that need to be initialized
				localMembers.push_back(&i);
			}
			else if (!i.mDefaultValue.empty())
			{
				// collect inherited members with default values for a call to super().__init__
				inheritedMembersWithDefaultValues.push_back(&i);
			}
		}

		std::stringstream initArgsStream;
		std::stringstream superInitArgsStream;
		std::vector<std::string> memberInitList;

		bool isFirstInitArg = true;
		for (auto &i : inheritedMembersWithDefaultValues)
		{
			if (!isFirstInitArg)
			{
				initArgsStream << ", ";
				superInitArgsStream << ", ";
			}
			initArgsStream << getPythonArgDef(*i, dom);
			superInitArgsStream << i->mMember << '=' << i->mMember;
			isFirstInitArg = false;
		}

		for (auto &i : localMembers)
		{
			if (!isFirstInitArg)
			{
				initArgsStream << ", ";
			}
			initArgsStream << getPythonArgDef(*i, dom);
			memberInitList.push_back("self." + i->mMember + " = " + i->mMember);
			isFirstInitArg = false;
		}

		std::string initArgs = initArgsStream.str();
		std::string superInitArgs = superInitArgsStream.str();

		// write __init__ method
		//cp.printCode(0, "\n");
		if (initArgs.empty())
		{
			cp.printCode(1, "def __init__(self):\n");
			cp.printCode(2, "pass\n");
		}
		else
		{
			cp.printCode(1, "def __init__(self, %s):\n", initArgs.c_str());
			bool bodyIsEmpty = true;
			if (!superInitArgs.empty())
			{
				cp.printCode(2, "super().__init__(%s)\n", superInitArgs.c_str());
				bodyIsEmpty = false;
			}
			if (!memberInitList.empty())
			{
				for (auto &m : memberInitList)
				{
					cp.printCode(2, "%s\n", m.c_str());
				}
				bodyIsEmpty = false;
			}
			if (bodyIsEmpty)
			{
				cp.printCode(2, "pass\n");
			}
		}

		// write as_data method
		cp.printCode(0, "\n");
		cp.printCode(1, "def as_data(self):\n", initArgs.c_str());
		Object* type = typeInfo(dom, mName);
		if (!type)
		{
			fprintf(stderr, "*** Invalid type name: '%s'\n", mName.c_str());
			return;
		}
		cp.printCode(2, "data = {}\n");
		for (auto m : localMembers)
		{
			if (m->mIsArray)
			{
				// for arrays, generate a list comprehension
				if (isClassType(dom, m->mType))
				{
					// for classes, invoke their as_data method to get their value
					cp.printCode(2, "data['%s'] = [e.as_data() for e in self.%s]\n", m->mMember.c_str(), m->mMember.c_str());
				}
				else if (isEnumType(dom, m->mType))
				{
					// for enums, use lookup table for their string values
					cp.printCode(2, "data['%s'] = '%s_strings[self.%s]\n", m->mMember.c_str(), m->mType.c_str(), m->mMember.c_str());
				}
				else if (m->mType == "i8" || m->mType == "i16" || m->mType == "i32" || m->mType == "i64" ||
					m->mType == "u8" || m->mType == "u16" || m->mType == "u32" || m->mType == "u64" ||
					m->mType == "float" || m->mType == "double" || m->mType == "bool" || m->mType == "string")
				{
					// for numeric, boolean, and string types use their value directly
					cp.printCode(2, "data['%s'] = [e for e in self.%s]\n", m->mMember.c_str(), m->mMember.c_str());
				}
				else
				{
					fprintf(stderr, "*** Don't know how to handle member type '%s' as data\n", m->mType.c_str());
				}
			}
			else if (isClassType(dom, m->mType))
			{
				// for classes, invoke their as_data method to get their value
				cp.printCode(2, "data['%s'] = self.%s.as_data()\n", m->mMember.c_str(), m->mMember.c_str());
			}
			else if (isEnumType(dom, m->mType))
			{
				// for enums, use lookup table for their string values
				cp.printCode(2, "data['%s'] = %s_strings[self.%s]\n", m->mMember.c_str(), m->mType.c_str(), m->mMember.c_str());
			}
			else if (m->mType == "i8" || m->mType == "i16" || m->mType == "i32" || m->mType == "i64" ||
				m->mType == "u8" || m->mType == "u16" || m->mType == "u32" || m->mType == "u64" ||
				m->mType == "float" || m->mType == "double" || m->mType == "bool" || m->mType == "string")
			{
				// for numeric, boolean, and string types use their value directly
				cp.printCode(2, "data['%s'] = self.%s\n", m->mMember.c_str(), m->mMember.c_str());
			}
			else
			{
				fprintf(stderr, "*** Don't know how to handle member type '%s' as data\n", m->mType.c_str());
			}
		}
		// If there is a base class, the child data is written to a special "slice" dictionary.
		// This is for compatibility with the protobuf JSON format.
		bool hasBase = !type->mInheritsFrom.empty();
		if (hasBase)
		{
			// we need to make sure this slice is properly nested in the inheritance chain
			std::stack<Object*> parents;
			Object* t = type;
			while (!t->mInheritsFrom.empty())
			{
				Object* p = typeInfo(dom, t->mInheritsFrom);
				if (!p)
				{
					fprintf(stderr, "*** Invalid base type name: '%s'\n", t->mInheritsFrom.c_str());
					return;  // hmmm, ungraceful?
				}
				parents.push(p);
				t = p;
			}
			Object* p = parents.top();
			parents.pop();
			std::string baseSlice = p->mName;
			baseSlice[0] = lowerCase(baseSlice[0]);
			std::string rootSlice = baseSlice;
			cp.printCode(2, "%s_data = super().as_data()\n", baseSlice.c_str());
			while (!parents.empty())
			{
				p = parents.top();
				parents.pop();
				std::string nextSlice = p->mName;
				nextSlice[0] = lowerCase(nextSlice[0]);
				cp.printCode(2, "%s_data = %s_data['%s']\n", nextSlice.c_str(), baseSlice.c_str(), nextSlice.c_str());
				baseSlice = nextSlice;
			}
			std::string sliceName = mName;
			sliceName[0] = lowerCase(sliceName[0]);
			cp.printCode(2, "%s_data['%s'] = data\n", baseSlice.c_str(), sliceName.c_str());
			cp.printCode(2, "return %s_data\n", rootSlice.c_str());
		}
		else
		{
			cp.printCode(2, "return data\n");
		}
	}

	void savePROTO(CodePrinter &cp, StringVector &arrays)
	{
		if (_stricmp(mType.c_str(), "Enum") == 0)
		{
			// it's an enum...
			cp.printCode(0, "\n");
			if (!mShortDescription.empty())
			{
				cp.printCode(0, "// %s\n", mShortDescription.c_str());
			}
			if (!mLongDescription.empty())
			{
				cp.printCode(0, "// %s\n", mLongDescription.c_str());
			}
			cp.printCode(0, "enum %s\n", mName.c_str());
			cp.printCode(0, "{\n");
			uint32_t id = 0;
			for (auto &i : mItems)
			{
				cp.printCode(1, "%s = %d;", i.mMember.c_str(), id);
				cp.printCode(10, "// %s\n",
					i.mShortDescription.c_str());
				id++;
			}

			cp.printCode(0, "}\n");
			cp.printCode(0, "\n");
			return;
		}

		// Before we can save out the class definition; we need to first see
		// if this class definition contains any arrays..
		for (auto &i : mItems)
		{
			if (i.mIsArray) // ok.. this is an array of object!
			{
				// ok, now see if this array is already represented.
				bool found = false;
				for (auto &j : arrays)
				{
					if (j == i.mType)
					{
						found = true;
						break;
					}
				}
				// If this array type not already represented; we need to define it
				if (!found)
				{
					arrays.push_back(i.mType);
				}
			}
		}

		cp.printCode(0, "\n");
		if (!mShortDescription.empty())
		{
			cp.printCode(0, "// %s\n", mShortDescription.c_str());
		}
		if (!mLongDescription.empty())
		{
			cp.printCode(0, "// %s\n", mLongDescription.c_str());
		}
		cp.printCode(0, "message %s", mName.c_str());
		// TODO TODO TODOO
		if (!mInheritsFrom.empty())
		{
			//			cp.printCode(0, " : public %s", mInheritsFrom.c_str());
		}
		cp.printCode(0, "\n");
		cp.printCode(0, "{\n");

		bool hasInheritedItemsWithDefaultValues = false;
		uint32_t id = 1;

		for (auto &i : mItems)
		{
			// If this is an 'inherited' data item. Don't clear it here
			// Because it was already handled in the initializer
			if (!i.mInheritsFrom.empty() && !i.mDefaultValue.empty())
			{
				hasInheritedItemsWithDefaultValues = true;
				continue;
			}
			const char *repeated = "";
			if (i.mIsArray)
			{
				repeated = "repeated ";
			}
			if (i.mProtoType.empty())
			{
				cp.printCode(1, "%s%s %s = %d;\n",
					repeated,
					getPROTOTypeString(i.mType.c_str()),
					i.mMember.c_str(),
					id);
			}
			else
			{
				cp.printCode(1, "%s%s %s = %d;\n",
					repeated,
					getPROTOTypeString(i.mProtoType.c_str()),
					i.mMember.c_str(),
					id);
			}
			id++;
		}

		if (!mChildren.empty())
		{
			cp.printCode(1, "oneof subtype\n");
			cp.printCode(1, "{\n");
			for (auto &k : mChildren)
			{
				char scratch[512];
				strncpy(scratch, k.c_str(), 512);
				scratch[0] = lowerCase(scratch[0]);
				cp.printCode(2, "%s %s = %d;\n", k.c_str(), scratch, id);
				id++;
			}
			cp.printCode(1, "}\n");
			cp.printCode(0, "\n");
		}

		cp.printCode(0, "}\n");

		cp.printCode(0, "\n");


	}

	void saveExportXML(CodePrinter &cph,CodePrinter &cpp, StringVector &arrays)
	{
		// Save the Export XML code
	}

	// If any of our member variables need reflection, then anyone declaring this object
	// will also need reflection
	void computeNeedsReflection(StringVector &needsReflection)
	{
		mNeedsReflection = false;
		for (auto &i : mItems)
		{
			if (i.needsReflection())
			{
				mNeedsReflection = true;
				break;
			}
		}
		if (mNeedsReflection)
		{
			needsReflection.push_back(mName);
		}
	}

	void computeReflectionMembers(StringVector &needsReflection)
	{
		for (auto &i : mItems)
		{
			i.memberNeedsReflection(needsReflection);
		}
	}

	std::string		mName;
	std::string		mType;
	std::string		mInheritsFrom;
	std::string		mMultipleInherticance;
	std::string		mEngineSpecific;
	std::string		mDefaultValue;
	std::string		mAlias;				// alias to existing type
	std::string		mShortDescription;
	std::string		mLongDescription;
	StringVector	mChildren;			// Classes which inherit from this class
	bool			mClone{ false }; // if true, we need to declare the clone virtual method
	bool			mAssignment{ false };
	bool			mNeedsReflection{ false };
	bool			mIsEnum{ false };	// whether this is an Enum type
	bool			mIsClass{ false };	// whether this is a Class type
	MemberVariableVector	mItems;
};

typedef std::vector< Object > ObjectVector;
typedef std::map< std::string, Object*> ObjectMap;

class DOM
{
public:

	void importComplete(void)
	{
		StringVector needsReflection;
		for (auto &i : mObjects)
		{
			i.computeNeedsReflection(needsReflection);
		}
		for (auto &i : mObjects)
		{
			i.computeReflectionMembers(needsReflection);
		}
		// compute multiple inheritance requirements...
		for (auto &i : mObjects)
		{
			if (!i.mInheritsFrom.empty())
			{
				for (auto &j : mObjects)
				{
					if (i.mInheritsFrom == j.mName)
					{
						if (!j.mInheritsFrom.empty())
						{
							i.mMultipleInherticance = j.mInheritsFrom;
						}
					}
				}
			}
		}

		// compute some useful type properties
		for (auto &i : mObjects)
		{
			if (_stricmp(i.mType.c_str(), "Enum") == 0)
			{
				i.mIsEnum = true;
			}
			if (_stricmp(i.mType.c_str(), "Class") == 0)
			{
				i.mIsClass = true;
			}
		}

		// create a lookup table for object types
		for (auto &i : mObjects)
		{
			mObjectMap[i.mName] = &i;
		}
	}

	Object* findObject(const std::string& name) const
	{
		auto it = mObjectMap.find(name);
		if (it != mObjectMap.end())
		{
			return it->second;
		}
		else
		{
			return nullptr;
		}
	}

    void saveDeserialize(CodePrinter &cpHeader, CodePrinter &cpImpl)
    {
        cpImpl.linefeed();

        cpImpl.printCode(0,"/*\n");
        cpImpl.printCode(0,"* Deserialization implementation\n");
        cpImpl.printCode(0,"*/\n");

        cpImpl.printCode(0,"int64_t atoi64(const char* str)\n");
        cpImpl.printCode(0,"{\n");
        cpImpl.printCode(0,"    int64_t ret = 0;\n");
        cpImpl.printCode(0,"\n");
        cpImpl.printCode(0,"    if (str)\n");
        cpImpl.printCode(0,"    {\n");
        cpImpl.printCode(0,"#ifdef _MSC_VER\n");
        cpImpl.printCode(0,"        ret = _atoi64(str);\n");
        cpImpl.printCode(0,"#else\n");
        cpImpl.printCode(0,"    ret = strtoul(str, nullptr, 10);\n");
        cpImpl.printCode(0,"#endif\n");
        cpImpl.printCode(0,"    }\n");
        cpImpl.printCode(0,"    return ret;\n");
        cpImpl.printCode(0,"}\n");


        cpImpl.printCode(0,"// Helper method to convert a string to an integer of this type\n");
        cpImpl.printCode(0,"void stringToInt(const char *str, uint64_t &v)\n");
        cpImpl.printCode(0,"{\n");
        cpImpl.printCode(0,"    v = uint64_t(atoi64(str));\n");
        cpImpl.printCode(0,"}\n");
		cpImpl.printCode(0,"\n");
		cpImpl.printCode(0,"\n");
        cpImpl.printCode(0,"// Helper method to convert a string to an integer of this type\n");
        cpImpl.printCode(0,"void stringToInt(const char *str, uint32_t &v)\n");
        cpImpl.printCode(0,"{\n");
        cpImpl.printCode(0,"    v = uint32_t(atoi(str));\n");
        cpImpl.printCode(0,"}\n");
		cpImpl.printCode(0,"\n");
		cpImpl.printCode(0,"\n");
        cpImpl.printCode(0,"// Helper method to convert a string to an integer of this type\n");
        cpImpl.printCode(0,"void stringToInt(const char *str, uint16_t &v)\n");
        cpImpl.printCode(0,"{\n");
        cpImpl.printCode(0,"    v = uint16_t(atoi(str));\n");
        cpImpl.printCode(0,"}\n");
		cpImpl.printCode(0,"\n");
		cpImpl.printCode(0,"\n");
        cpImpl.printCode(0,"// Helper method to convert a string to an integer of this type\n");
        cpImpl.printCode(0,"void stringToInt(const char *str, uint8_t &v)\n");
        cpImpl.printCode(0,"{\n");
        cpImpl.printCode(0,"    v = uint8_t(atoi(str));\n");
        cpImpl.printCode(0,"}\n");
		cpImpl.printCode(0,"\n");
		cpImpl.printCode(0,"\n");
        cpImpl.printCode(0,"// Helper method to convert a string to an integer of this type\n");
        cpImpl.printCode(0,"void stringToInt(const char *str, int64_t &v)\n");
        cpImpl.printCode(0,"{\n");
        cpImpl.printCode(0,"    v = int64_t(atoi64(str));\n");
        cpImpl.printCode(0,"}\n");
		cpImpl.printCode(0,"\n");
		cpImpl.printCode(0,"\n");
        cpImpl.printCode(0,"// Helper method to convert a string to an integer of this type\n");
        cpImpl.printCode(0,"void stringToInt(const char *str, int32_t &v)\n");
        cpImpl.printCode(0,"{\n");
        cpImpl.printCode(0,"    v = int32_t(atoi(str));\n");
        cpImpl.printCode(0,"}\n");
		cpImpl.printCode(0,"\n");
		cpImpl.printCode(0,"\n");
        cpImpl.printCode(0,"// Helper method to convert a string to an integer of this type\n");
        cpImpl.printCode(0,"void stringToInt(const char *str, int16_t &v)\n");
        cpImpl.printCode(0,"{\n");
        cpImpl.printCode(0,"    v = int16_t(atoi(str));\n");
        cpImpl.printCode(0,"}\n");
		cpImpl.printCode(0,"\n");
		cpImpl.printCode(0,"\n");
        cpImpl.printCode(0,"// Helper method to convert a string to an integer of this type\n");
        cpImpl.printCode(0,"void stringToInt(const char *str, int8_t &v)\n");
        cpImpl.printCode(0,"{\n");
        cpImpl.printCode(0,"    v = int8_t(atoi(str));\n");
        cpImpl.printCode(0,"}\n");
        cpImpl.linefeed();


        cpImpl.printCode(0,"template<typename DocumentOrObject, typename T>\n");
        cpImpl.printCode(0,"bool deserializeFrom(const DocumentOrObject&, T&);\n");
        cpImpl.linefeed();
        cpImpl.printCode(0,"rapidjson::Document deserializeDocument(const char* in)\n");
        cpImpl.printCode(0,"{\n");
        cpImpl.printCode(1,"rapidjson::Document d;\n");
        cpImpl.printCode(1,"d.Parse(in);\n");
        cpImpl.printCode(1,"return d;\n");
        cpImpl.printCode(0,"}\n");
        cpImpl.linefeed();

        cpImpl.printCode(0,"namespace details\n");
        cpImpl.printCode(0,"{\n");
        cpImpl.printCode(0,"    template<typename T>\n");
        cpImpl.printCode(0,"    T Deserialize<T>::deserialize(const char* in, bool& deserializedOk)\n");
        cpImpl.printCode(0,"    {\n");
        cpImpl.printCode(0,"        const auto d = deserializeDocument(in);\n");
        cpImpl.printCode(0,"        T result;\n");
        cpImpl.printCode(0,"        deserializedOk = deserializeFrom(d, result);\n");
        cpImpl.printCode(0,"        return result;\n");
        cpImpl.printCode(0,"    }\n");

        cpImpl.printCode(0,"    template<typename T>\n");
        cpImpl.printCode(0,"    T Deserialize<T>::deserialize(const rapidjson::RapidJSONDocument &d, bool& deserializedOk)\n");
        cpImpl.printCode(0,"    {\n");
        cpImpl.printCode(0,"        T result;\n");
        cpImpl.printCode(0,"        deserializedOk = deserializeFrom(d, result);\n");
        cpImpl.printCode(0,"        return result;\n");
        cpImpl.printCode(0,"    }\n");

        cpImpl.printCode(0,"}\n");

        cpImpl.linefeed();

        cpHeader.linefeed();
        cpHeader.printCode(0,"/*\n");
        cpHeader.printCode(0," * Deserialization\n");
        cpHeader.printCode(0," */\n");
        cpHeader.linefeed();

        cpHeader.printCode(0,"namespace details\n");
        cpHeader.printCode(0,"{\n");
		cpHeader.printCode(0,"\n");
        cpHeader.printCode(0,"    template<typename T>\n");
        cpHeader.printCode(0,"    struct Deserialize\n");
        cpHeader.printCode(0,"    {\n");
        cpHeader.printCode(0,"        static T deserialize(const char* in, bool& deserializedOk);\n");
        cpHeader.printCode(0,"        static T deserialize(const rapidjson::RapidJSONDocument &d, bool& deserializedOk);\n");
        cpHeader.printCode(0,"    };\n");
        cpHeader.printCode(0,"}\n");
		cpHeader.printCode(0,"\n");
        cpHeader.printCode(0,"template<typename T>\n");
        cpHeader.printCode(0,"T deserialize(const char* in, bool& deserializedOk)\n");
        cpHeader.printCode(0,"{\n");
        cpHeader.printCode(0,"    return details::Deserialize<T>::deserialize(in, deserializedOk);\n");
        cpHeader.printCode(0,"}\n");
		cpHeader.printCode(0,"\n");
        cpHeader.printCode(0,"template<typename T>\n");
        cpHeader.printCode(0,"T deserialize(const std::string& in, bool& deserializedOk)\n");
        cpHeader.printCode(0,"{\n");
        cpHeader.printCode(0,"    return details::Deserialize<T>::deserialize(in.c_str(), deserializedOk);\n");
        cpHeader.printCode(0,"}\n");

        cpHeader.printCode(0,"template<typename T>\n");
        cpHeader.printCode(0,"T deserialize(const rapidjson::RapidJSONDocument &in, bool& deserializedOk)\n");
        cpHeader.printCode(0,"{\n");
        cpHeader.printCode(0,"    return details::Deserialize<T>::deserialize(in, deserializedOk);\n");
        cpHeader.printCode(0,"}\n");

        cpHeader.linefeed();

        ClassEnumMap classEnumMap;
        for (auto &i : mObjects)
        {
            classEnumMap[i.mName] = i.mIsEnum;
        }

        for (auto &i : mObjects)
        {
            i.saveDeserialize(cpHeader, cpImpl, classEnumMap);
        }

    }


    void saveSerialize(CodePrinter &cpHeader,CodePrinter &cpImpl)
    {
        cpImpl.linefeed();
        cpImpl.linefeed();
        cpImpl.printCode(0, "template<typename T, typename DocumentOrObject, typename Alloc>\n");
        cpImpl.printCode(0, "rapidjson::Document& serializeTo(const T& type, DocumentOrObject& d, Alloc& alloc);\n");
        cpImpl.linefeed();
        cpImpl.printCode(0, "std::string serializeDocument(const rapidjson::Document& d)\n");
        cpImpl.printCode(0, "{\n");
        cpImpl.printCode(0, "    rapidjson::StringBuffer strbuf;\n");
        cpImpl.printCode(0, "    rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);\n");
        cpImpl.printCode(0, "    d.Accept(writer);\n");
        cpImpl.printCode(0, "    return strbuf.GetString();\n");
        cpImpl.printCode(0, "}\n");
        cpImpl.linefeed();

        ClassEnumMap classEnumMap;
        for (auto &i : mObjects)
        {
            classEnumMap[i.mName] = i.mIsEnum;
        }

        for (auto &i : mObjects)
        {
            i.saveSerialize(cpHeader,cpImpl,classEnumMap);
        }

    }

    void saveTypeScript(CodePrinter &dom,CodePrinter &cpenum,CodePrinter &cpenumImpl,const char *destDir)
    {
        cpenum.linefeed();
        cpenum.printCode(0, "template<typename T>\n");
        cpenum.printCode(0, "T unstringifyEnum(const std::string& str, bool& ok);\n");
        cpenum.linefeed();

        cpenumImpl.printCode(0,"// clang-format off\n");
        cpenumImpl.printCode(0, "// CreateDOM: Schema Generation tool written by John W. Ratcliff, 2019\n");
        cpenumImpl.printCode(0, "// C++ binding code for enumeration lookups and initialization to default values.\n");
        cpenumImpl.printCode(0, "// The Google DOCs Schema Spreadsheet for this source came from: %s\n", mURL.c_str());
        cpenumImpl.printCode(0,"#include \"%s.h\"\n", mFilename.c_str());
        cpenumImpl.printCode(0, "#include <assert.h>\n");
        cpenumImpl.printCode(0,"#include <unordered_map>\n");
        cpenumImpl.printCode(0,"#include <mutex>\n");
        cpenumImpl.printCode(0,"#include <string>\n");
        cpenumImpl.printCode(0,"#include <string.h>\n");

        cpenumImpl.printCode(0, "#ifdef _MSC_VER\n");
        cpenumImpl.printCode(0, "#pragma warning(disable:4996 4100)\n");
        cpenumImpl.printCode(0, "#endif\n");
        cpenumImpl.linefeed();
        cpenumImpl.printCode(0, "#include \"RapidJSONDocument.h\"\n");
        cpenumImpl.linefeed();

        cpenumImpl.printCode(0,"namespace %s {\n", mNamespace.c_str());

        OmniCommandInstanceVector instances;
        for (auto &i : mObjects)
        {
            i.saveTypeScript(dom,cpenum,cpenumImpl,*this,instances,mNamespace.c_str(),destDir);
        }
        if ( !instances.empty() )
        {
            std::string fpHPP = fpout("OmniApiInstance.h",mNamespace.c_str(),destDir);
            std::string fpCPP = fpout("OmniApiInstance.cpp",mNamespace.c_str(),destDir);
            {
                CodePrinter cpp(fpCPP);

                cpp.printCode(0, "#include \"OmniApiInstance.h\"\n");
                cpp.printCode(0, "#include \"omniverse_api.h\"\n");
                cpp.printCode(0, "\n");
                cpp.printCode(0, "namespace omniapi\n");
                cpp.printCode(0, "{\n");
                cpp.printCode(0, "\n");
                cpp.printCode(0, "// Forward reference functions to create instances for various command types\n");
                for (auto &i:instances)
                {
                    cpp.printCode(0, "OmniApiCommand *create%sInstance(const rapidjson::RapidJSONDocument &d, OmniConnection *oc);\n", i.mCommand.c_str());
                }
                cpp.printCode(0, "\n");
                cpp.printCode(0, "createCommandInstance getCreateCommandInstance(CommandType ctype)\n");
                cpp.printCode(0, "{\n");
                cpp.printCode(1, "createCommandInstance ret = nullptr;\n");
                cpp.printCode(0, "\n");
                cpp.printCode(1, "switch (ctype)\n");
                cpp.printCode(1, "{\n");
                for (auto &i:instances)
                {
                    cpp.printCode(2, "case CommandType::%s:\n", i.mCommandType.c_str());
                    cpp.printCode(3, "ret = create%sInstance;\n", i.mCommand.c_str());
                    cpp.printCode(3, "break;\n");
                }
                cpp.printCode(1, "}\n");
                cpp.printCode(0,"\n");
                cpp.printCode(1, "return ret;\n");
                cpp.printCode(0, "}\n");
                cpp.printCode(0, "\n");
                cpp.printCode(0, "}\n");
                cpp.finalize();
            }
            {
                CodePrinter hpp(fpHPP);
                hpp.printCode(0, "#pragma once\n");
                hpp.printCode(0, "\n");
                hpp.printCode(0, "#include <carb/Defines.h>\n");
                hpp.printCode(0, "\n");
                hpp.printCode(0, "namespace rapidjson\n");
                hpp.printCode(0, "{\n");
                hpp.printCode(1, "class RapidJSONDocument;\n");
                hpp.printCode(0, "}\n");
                hpp.printCode(0, "\n");
                hpp.printCode(0, "namespace omniapi\n");
                hpp.printCode(0, "{\n");
                hpp.printCode(0, "class OmniApiCommand;\n");
                hpp.printCode(0, "class OmniConnection;\n");
                hpp.printCode(0, "enum class CommandType;\n");
                hpp.printCode(0, "\n");
                hpp.printCode(0, "typedef OmniApiCommand *(CARB_ABI* createCommandInstance)(const rapidjson::RapidJSONDocument &d, OmniConnection *oc);\n");
                hpp.printCode(0, "\n");
                hpp.printCode(0, "createCommandInstance getCreateCommandInstance(CommandType ctype);\n");
                hpp.printCode(0, "\n");
                hpp.printCode(0, "}\n");

                hpp.finalize();
            }
        }
    }

    void saveCPP(CodePrinter &cp)
    {
        cp.printCode(0, "#pragma once\n");
        cp.printCode(0, "\n");
        cp.printCode(0,"// clang-format off\n");
        cp.printCode(0, "// CreateDOM: Schema Generation tool written by John W. Ratcliff, 2017\n");
        cp.printCode(0, "// Warning:This source file was auto-generated by the CreateDOM tool. Do not try to edit this source file manually!\n");
        cp.printCode(0, "// The Google DOCs Schema Spreadsheet for this source came from: %s\n", mURL.c_str());
        cp.printCode(0, "\n");

        cp.printCode(0, "#ifdef _MSC_VER\n");
        cp.printCode(0, "#pragma warning(push)\n");
        cp.printCode(0, "#pragma warning(disable:4244)\n");
        cp.printCode(0, "#endif\n");


        char temp[512];
        STRING_HELPER::stringFormat(temp,sizeof(temp), "%s.h", mFilename.c_str());
        cp.printCode(0, "#include <vector>\n");
        cp.printCode(0, "#include <unordered_map>\n");
        cp.printCode(0, "#include <string>\n");
        cp.printCode(0, "#include <stdint.h>\n");
        cp.printCode(0,"#include <string.h>\n");
        cp.printCode(0, "\n");
        cp.printCode(0, "#define USE_OPTIONAL 1\n");
        cp.printCode(0, "\n");
        cp.printCode(0, "#if __has_include(<optional>) && USE_OPTIONAL == 1\n");
        cp.printCode(0, "#include <optional>\n");
        cp.printCode(0, "#else\n");
        cp.printCode(0, "#define NO_OPTIONAL\n");
        cp.printCode(0, "#endif\n");
        cp.printCode(0, "\n");
        cp.printCode(0, "namespace codegen\n");
        cp.printCode(0, "{\n");
        cp.printCode(0, "template<typename T>\n");
        cp.printCode(0, "#ifdef NO_OPTIONAL\n");
        cp.printCode(0, "using optional = T;\n");
        cp.printCode(0, "#else\n");
        cp.printCode(0, "using optional = std::optional<T>;\n");
        cp.printCode(0, "#endif\n");
        cp.printCode(0, "}\n");

        cp.printCode(0, "namespace rapidjson\n");
        cp.printCode(0, "{\n");
        cp.printCode(0, "    class RapidJSONDocument;\n");
        cp.printCode(0, "}\n");

        cp.printCode(0, "\n");
        cp.printCode(0, "namespace %s\n", mNamespace.c_str());
        cp.printCode(0, "{\n");
        cp.printCode(0, "\n");

        cp.printCode(0, "\n");
        cp.printCode(0, "// Forward declare the two types of string vector containers.\n");
        cp.printCode(0, "\n");

        StringVector arrays;
        StringVector needsReflection;
        for (auto &i : mObjects)
        {
            if (i.mNeedsReflection)
            {
                needsReflection.push_back(i.mName);
            }
        }
        StringVector cloneObjects;
        for (auto &i : mObjects)
        {
            i.saveCPP(cp, arrays, needsReflection, cloneObjects, *this);
        }


    }

	void saveCPP(CodePrinter &impl,CodePrinter &dom)
	{

		StringVector arrays;
		StringVector needsReflection;
		for (auto &i : mObjects)
		{
			if (i.mNeedsReflection)
			{
				needsReflection.push_back(i.mName);
			}
		}

		auto headerBegin = [this](CodePrinter &cp,bool isDef)
		{
			char temp[512];
			STRING_HELPER::stringFormat(temp, 512, "%s%s_H", mFilename.c_str(), isDef ? "_IMPL" : "");
			_strupr(temp);
			cp.printCode(0, "#ifndef %s\n", temp);
			cp.printCode(0, "#define %s\n", temp);
			cp.printCode(0, "\n");
            cp.printCode(0,"// clang-format off\n");
			cp.printCode(0, "// CreateDOM: Schema Generation tool written by John W. Ratcliff, 2017\n");
			cp.printCode(0, "// Warning:This source file was auto-generated by the CreateDOM tool. Do not try to edit this source file manually!\n");
			cp.printCode(0, "// The Google DOCs Schema Spreadsheet for this source came from: %s\n", mURL.c_str());
			cp.printCode(0, "\n");
			if (isDef)
			{
				STRING_HELPER::stringFormat(temp, 512, "%s.h", mFilename.c_str());
				cp.printCode(0, "#include \"%s\"\n", temp);
				cp.printCode(0, "#include <string>\n");
                cp.printCode(0, "#include <string.h>\n");
				cp.printCode(0, "#include <vector>\n");
                cp.printCode(0, "#include <unordered_map>\n");
			}
			else
			{
			}
			cp.printCode(0, "#include <stdint.h>\n");
			cp.printCode(0, "\n");
			cp.printCode(0, "\n");
			cp.printCode(0, "namespace %s\n", mNamespace.c_str());
			cp.printCode(0, "{\n");
			cp.printCode(0, "\n");

			if (isDef)
			{
				cp.printCode(0, "\n");
				cp.printCode(0, "// Forward declare the two types of string vector containers.\n");
				cp.printCode(0, "\n");
				cp.printCode(0, "// Declare the clone-object class for deep copies\n");
				cp.printCode(0, "// of objects by the implementation classes\n");
				cp.printCode(0, "// Not to be used with the base DOM classes;\n");
				cp.printCode(0, "// they do not support deep copies\n");
				cp.printCode(0, "// Also declares the virtual method to init the DOM contents.\n");
				cp.printCode(0, "class CloneObject\n");
				cp.printCode(0, "{\n");
				cp.printCode(0, "public:\n");
				cp.printCode(1, "virtual ~CloneObject() {  };\n");
				cp.printCode(1, "// Declare the default virtual clone method; not implemented for DOM objects; only used for the implementation versions.\n");
				cp.printCode(1, "virtual CloneObject *clone(void) const { return nullptr; };\n");
				cp.printCode(1, "// Declare the default initDOM method; which is only needed for some implementation objects.\n");
				cp.printCode(1, "virtual void initDOM(void) {  };\n");
				cp.printCode(0, "};\n");
			}

		};

		headerBegin(impl, true);
		headerBegin(dom, false);
		StringVector cloneObjects;
		for (auto &i : mObjects)
		{
			i.saveCPP(impl,dom,arrays,needsReflection,cloneObjects,*this);
		}

		auto headerEnd = [this](CodePrinter &cp, bool isDef)
		{
			char temp[512];
			cp.printCode(0, "\n");
			cp.printCode(0, "\n");
			cp.printCode(0, "} // End of %s namespace\n", mNamespace.c_str());
			cp.printCode(0, "\n");
			cp.printCode(0, "#endif // End of %s\n", temp);
		};
		headerEnd(impl, true);
		headerEnd(dom, false);

	}

	void savePython(CodePrinter &cp)
	{
		constexpr bool DBG_TYPE_INFO = false;
		if (DBG_TYPE_INFO)
		{
			for (auto& kv : mObjectMap)
			{
				auto& type = kv.second;
				printf("%s:\n", type->mName.c_str());
				printf("  inheritsFrom: %s\n", type->mInheritsFrom.c_str());
			}
		}
        cp.printCode(0,"// clang-format off\n");
		cp.printCode(0, "# CreateDOM: Schema Generation tool written by John W. Ratcliff, 2017\n");
		cp.printCode(0, "# Warning:This source file was auto-generated by the CreateDOM tool. Do not try to edit this source file manually!\n");
		cp.printCode(0, "# The Google DOCs Schema Spreadsheet for this source came from: %s\n", mURL.c_str());
		cp.printCode(0, "\n");

		StringVector cloneObjects;
		for (auto &i : mObjects)
		{
			i.savePython(cp, *this);
		}
	}

    bool            mPlainOldData{false};
	std::string		mNamespace;
    std::string     mDestDir;
	std::string		mFilename;
	std::string		mURL;
	std::string		mExportXML;
	ObjectVector	mObjects;
	ObjectMap		mObjectMap;
};

class CreateDOMDef : public CreateDOM
{
public:
	CreateDOMDef(const char *destDir)
	{
        mDestDir = std::string(destDir);
	}
	virtual ~CreateDOMDef(void)
	{

	}


	// Parses this XML and accumulates all of the unique element and attribute names
	virtual void parseCSV(const char *xmlName) final
	{
        printf("ParseCSV:%s\n", xmlName);
		FILE *fph = fopen(xmlName, "rb");
		if (fph == nullptr) 
        {
            printf("Failed to open file '%s' for read access!\n",xmlName);
            return;
        }
		fseek(fph, 0L, SEEK_END);
		size_t flen = ftell(fph);
		fseek(fph, 0L, SEEK_SET);
		if (flen > 0)
		{
			char *data = (char *)malloc(flen + 1);
			size_t r = fread(data, flen, 1, fph);
			if (r == 1)
			{
                printf("Successfully read DOM file: %s which is %d bytes long\n",xmlName, uint32_t(flen)); 
				data[flen] = 0;
				char *scan = data;
				uint32_t argc = 0;
#define MAX_ARGV 128
				char *argv[MAX_ARGV];
                mEndOfFile = false;
				while (*scan && !mEndOfFile )
				{
					// Skip any linefeeds
					argc = 0;
					while (*scan == 10 || *scan == 13 && *scan )
					{
						scan++;
					}
					if (*scan == 0)
					{
						break;
					}
					for (;;)
					{
						if (*scan == 34) // if it's an open quote..
						{
							scan++;
							argv[argc] = scan;
							while (*scan && *scan != 34)
							{
								scan++;
							}
							if (*scan)
							{
								*scan = 0;
								scan++;
							}
						}
						else
						{
							argv[argc] = scan;
						}
						while (*scan && *scan != ',' && *scan != 10 && *scan != 13)
						{
							scan++;
						}
						if (argc < MAX_ARGV)
						{
							argc++;
						}
						if (*scan)
						{
							char c = *scan;
							*scan = 0;
							scan++;
							if (c == 10 || c == 13)
							{
								processArgs(argc, argv);
								break;
							}
						}
						else
						{
							break;
						}
					}
				}
			}
			free(data);
		}
		fclose(fph);
		if (mHaveObject)
		{
			mDOM.mObjects.push_back(mCurrentObject);
			mCurrentObject.clear();
			mHaveObject = false;
		}
		//
		mDOM.importComplete();
		//
	}

    virtual void saveCPP(bool saveCPP,bool saveTypeScriptFlag) final
    {
        if (mDOM.mNamespace.empty())
        {
            printf("No namespace specified!\n");
            return;
        }
        if (mDOM.mFilename.empty())
        {
            printf("No source filename specified.\n");
            return;
        }
        char scratch[512];
        std::string fimpl;
        if ( mDOM.mPlainOldData )
        {
            STRING_HELPER::stringFormat(scratch, 512, "%sDef.h", mDOM.mFilename.c_str());
            printf("Saving C++ DOM Definition to: %s\n", scratch);
        }

        STRING_HELPER::stringFormat(scratch, 512, "%s.h", mDOM.mFilename.c_str());
        std::string fdom = fpout(scratch, mDOM.mNamespace.c_str(),mDestDir.c_str());
        printf("Saving C++ DOM to: %s\n", scratch);

        STRING_HELPER::stringFormat(scratch, 512, "%s.cpp", mDOM.mFilename.c_str());
        std::string fpCpp = fpout(scratch,mDOM.mNamespace.c_str(),mDestDir.c_str());
        printf("Saving C++ implementation DOM to: %s\n", scratch);
        if ( mDOM.mPlainOldData )
        {
            CodePrinter impl(fimpl);
            CodePrinter dom(fdom);
            mDOM.saveCPP(impl, dom);
            impl.finalize();
            dom.finalize();
        }
        else
        {
            CodePrinter dom(fdom);
            CodePrinter cpp(fpCpp);
            mDOM.saveCPP(dom);
            saveTypeScript(dom,cpp,mDestDir.c_str(),saveTypeScriptFlag);
            dom.printCode(0,"} // End of namespace:%s\n", mDOM.mNamespace.c_str());

            dom.printCode(0,"#ifdef _MSC_VER\n");
            dom.printCode(0,"#pragma warning(pop)\n");
            dom.printCode(0,"#endif\n");

            cpp.printCode(0,"} // End of namespace:%s\n", mDOM.mNamespace.c_str());
            dom.finalize();
            cpp.finalize();
        }
    }

	// Save the DOM as C++ code
	void saveTypeScript(CodePrinter &hpp,CodePrinter &cpp,const char *destDir,bool saveTypeScriptFlag) 
	{
		if (mDOM.mNamespace.empty())
		{
			printf("No namespace specified!\n");
			return;
		}
		if (mDOM.mFilename.empty())
		{
			printf("No source filename specified.\n");
			return;
		}
        std::string fdom;
        if ( saveTypeScriptFlag )
        {
            char scratch[512];
		    STRING_HELPER::stringFormat(scratch, 512, "%s.ts", mDOM.mFilename.c_str());
            printf("Saving DOM definition in TypeScript to: %s\n", scratch);
		    fdom = fpout(scratch,mDOM.mNamespace.c_str(),destDir);
            printf("Saving TypeScript DOM to: %s\n", scratch);
        }

		CodePrinter typeScript(fdom);

		mDOM.saveTypeScript(typeScript,hpp,cpp,mDestDir.c_str());
        mDOM.saveSerialize(hpp,cpp);
        mDOM.saveDeserialize(hpp,cpp);

        typeScript.finalize();
	}

	// Save the DOM as Python code
	virtual void savePython(void) final
	{
		if (mDOM.mNamespace.empty())
		{
			printf("No namespace specified!\n");
			return;
		}
		if (mDOM.mFilename.empty())
		{
			printf("No source filename specified.\n");
			return;
		}
		char scratch[512];
		STRING_HELPER::stringFormat(scratch, 512, "%s.py", mDOM.mFilename.c_str());
		std::string fdom = fpout(scratch, mDOM.mNamespace.c_str(),mDestDir.c_str());
		printf("Saving Python DOM to: %s\n", scratch);

		CodePrinter cp(fdom);

		mDOM.savePython(cp);

        cp.finalize();
	}
	
	// Save the DOM as a JSON schema
	virtual void saveJSON(void) final
	{

	}

	virtual void release(void) final
	{
		delete this;
	}

	virtual void processArgs(uint32_t argc,char **argv) final
	{
        if ( mEndOfFile )
        {
            return;
        }
        if (_stricmp(argv[0], "EOF") == 0)
        {
            mEndOfFile = true;
            printf("Reached end of file marker, no longer parsing any lines past this.\n");
        }
		else if (_stricmp(argv[0],"Filename") == 0 )
		{
			if (argc >= 2)
			{
				mDOM.mFilename = std::string(argv[1]);
			}
		}
		else if (_stricmp(argv[0], "Namespace") == 0)
		{
			if (argc >= 2)
			{
				mDOM.mNamespace = std::string(argv[1]);
			}
		}
        else if (_stricmp(argv[0], "POD") == 0)
        {
            if (argc >= 2)
            {
                mDOM.mPlainOldData = getBool(argv[1]);
            }
        }
		else if (_stricmp(argv[0], "ExportXML") == 0)
		{
			if (argc >= 2)
			{
				mDOM.mExportXML = std::string(argv[1]);
			}
		}
		else if (_stricmp(argv[0], "URL") == 0)
		{
			if (argc >= 2)
			{
				mDOM.mURL = std::string(argv[1]);
			}
		}
		else if (_stricmp(argv[0], "ObjectName") == 0)
		{
			// skip, this is just the header line..
		}
		else
		{
			const char *object = argv[0];
			if (strlen(object)) // if it's a new object 
			{
				if (mHaveObject)
				{
					mDOM.mObjects.push_back(mCurrentObject);
					mCurrentObject.clear();
				}
				mHaveObject = true;
				mCurrentObject.mName = std::string(object);
				if (argc >= 3 )
				{
					mCurrentObject.mType = std::string(argv[2]);
					if (argc >= 4)
					{
						mCurrentObject.mInheritsFrom = std::string(argv[3]);
						{
							for (auto &j : mDOM.mObjects)
							{
								if (j.mName == mCurrentObject.mInheritsFrom)
								{
									j.mChildren.push_back(mCurrentObject.mName);
								}
							}
						}
						if (argc >= 5)
						{
							mCurrentObject.mEngineSpecific = std::string(argv[4]);
							if (argc >= 6)
							{
								if ( _stricmp(argv[5],"CLONE") == 0 )
								{
									mCurrentObject.mClone = true;
								}
								else if (_stricmp(argv[5], "ASSIGNMENT") == 0)
								{
									mCurrentObject.mAssignment = true;
								}
								if (argc >= 9)
								{
									//printf("Alias '%s' -> '%s'\n", mCurrentObject.mName.c_str(), argv[8]);
									mCurrentObject.mAlias = std::string(argv[8]);
									if (argc >= 10)
									{
										mCurrentObject.mShortDescription = std::string(argv[9]);
										if (argc >= 11)
										{
											mCurrentObject.mLongDescription = std::string(argv[10]);
										}
									}
								}
							}
						}
					}
				}
			}
			else if (argc >= 2)
			{
				char *dataItemName = argv[1];
				if (strlen(dataItemName))
				{
					char *pointer = strchr(dataItemName, '*');
					char *array = strchr(dataItemName+1, '[');
					if (array)
					{
						*array = 0;
					}
					else if (pointer)
					{
						*pointer = 0;
					}
					MemberVariable di;
                    char tempValue[512];
                    const char *source = dataItemName;
                    char *dest = tempValue;
                    char *eos = &tempValue[510];
                    while ( *source && dest < eos )
                    {
                        if ( *source == '?')
                        {
                            source++;
                            di.mIsOptional = OptionalType::optional;
                        }
                        else if (*source == '!')
                        {
                            source++;
                            di.mIsOptional = OptionalType::optional_deserialize;
                        }
                        else
                        {
                            *dest++ = *source++;
                        }
                    }
                    *dest = 0;
                    const char *memberName = tempValue;
                    if ( tempValue[0] == '[' )
                    {
                        memberName = tempValue+1;
                        di.mIsMap = true;
                        char *colon = strchr(tempValue,':');
                        if ( colon )
                        {
                            *colon = 0;
                            colon++;
                            const char *mapType = colon;
                            char *closeBracket = strchr(colon,']');
                            if ( closeBracket )
                            {
                                *closeBracket = 0;
                                di.mMapType = std::string(mapType);
                            }
                            else
                            {
                                assert(0);
                            }
                        }
                        else
                        {
                            assert(0);
                        }
                    }
					di.mMember = std::string(memberName);
					di.mIsArray = array ? true : false;
					di.mIsPointer = pointer ? true : false;
					if (argc >= 3)
					{
                        char type[512];
                        strncpy(type,argv[2],sizeof(type));
                        char *exclamation = strchr(type,'!');
                        if ( exclamation )
                        {
                            *exclamation = 0;
                            di.mSerializeEnumAsInteger = true;
                        }
						di.mType = std::string(type);
						di.mIsString = strcmp(di.mType.c_str(), "string") == 0;
						if (argc >= 4)
						{
							const char *inheritsFrom = argv[3];
							if (strncmp(inheritsFrom, "PROTO:", 6) == 0)
							{
								inheritsFrom += 6;
								di.mProtoType = std::string(inheritsFrom);
							}
							else
							{
								di.mInheritsFrom = std::string(argv[3]);
							}
							if (argc >= 5)
							{
								di.mEngineSpecific = std::string(argv[4]);
								if (argc >= 6)
								{
									di.mDefaultValue = std::string(argv[5]);
									if (argc >= 7)
									{
										di.mMinValue = std::string(argv[6]);
										if (argc >= 8)
										{
											di.mMaxValue = std::string(argv[7]);
                                            if (argc >= 9)
                                            {
                                                di.mAlias = std::string(argv[8]);
											    if (argc >= 10) // skip alias field
											    {
												    di.mShortDescription = std::string(argv[9]);
												    if (argc >= 11)
												    {
													    di.mLongDescription = std::string(argv[10]);
												    }
											    }
                                            }
										}
									}
								}
							}
						}
					}
					di.init();
					mCurrentObject.mItems.push_back(di);
				}
			}
		}
	}

	// Save the DOM as a protobuf file
	virtual void savePROTOBUF(void) final
	{
		if (mDOM.mNamespace.empty())
		{
			printf("No namespace specified!\n");
			return;
		}
		if (mDOM.mFilename.empty())
		{
			printf("No source filename specified.\n");
			return;
		}
	}
    bool    mEndOfFile{false};
	bool	mHaveObject{ false };
	Object	mCurrentObject;
	DOM		mDOM;
    std::string mDestDir;
};

CreateDOM *CreateDOM::create(const char *destDir)
{
	CreateDOMDef *in = new CreateDOMDef(destDir);
	return static_cast<CreateDOM *>(in);
}


std::string getPythonArgDef(const MemberVariable &var, const DOM &dom)
{
	std::ostringstream ss;

	// make sure floats and doubles always have a decimal point
	ss << std::showpoint;

	ss << var.mMember << '=';

	if (var.mIsArray)
	{
		if (!var.mDefaultValue.empty())
		{
			fprintf(stderr, "*** Warning: Don't know how to parse default values for arrays\n");
		}
		// use an empty list
		ss << "[]";
	}
	else if (var.mType == "string")
	{
		ss << '\'';
		if (!var.mDefaultValue.empty())
		{
			ss << var.mDefaultValue;
		}
		ss << '\'';
	}
	else if (var.mType == "i8" || var.mType == "i16" || var.mType == "i32" || var.mType == "i64")
	{
		long long i = 0;
		if (!var.mDefaultValue.empty())
		{
			i = std::strtoll(var.mDefaultValue.c_str(), nullptr, 0);
		}
		ss << i;
	}
	else if (var.mType == "u8" || var.mType == "u16" || var.mType == "u32" || var.mType == "u64")
	{
		unsigned long long u = 0;
		if (!var.mDefaultValue.empty())
		{
			u = std::strtoull(var.mDefaultValue.c_str(), nullptr, 0);
		}
		ss << u;
	}
	else if (var.mType == "float" || var.mType == "double")
	{
		double d = 0.0;
		if (!var.mDefaultValue.empty())
		{
			const char *fvalue = var.mDefaultValue.c_str();
			if (strcmp(fvalue, "FLT_MAX") == 0)
			{
				d = FLT_MAX;
			}
			else if (strcmp(fvalue, "FLT_MIN") == 0)
			{
				d = FLT_MIN;
			}
			else
			{
				d = strtod(var.mDefaultValue.c_str(), nullptr);
			}
		}
		ss << d;
	}
	else if (var.mType == "bool")
	{
		if (_stricmp(var.mDefaultValue.c_str(), "true") == 0)
		{
			ss << "True";
		}
		else
		{
			ss << "False";
		}
	}
	else if (var.mType == "Vec3")
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		if (!var.mDefaultValue.empty())
		{
			sscanf(var.mDefaultValue.c_str(), "%f,%f,%f", &x, &y, &z);
		}
		ss << "Vec3(x=" << x << ",y=" << y << ",z=" << z << ")";
	}
	else if (var.mType == "Quat")
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 1.0f;
		if (!var.mDefaultValue.empty())
		{
			sscanf(var.mDefaultValue.c_str(), "%f,%f,%f,%f", &x, &y, &z, &w);
		}
		ss << "Quat(x=" << x << ",y=" << y << ",z=" << z << ",w=" << w << ")";
	}
	else
	{
		// get some info about this variable's type
		Object* obj = dom.findObject(var.mType);
		if (!obj)
		{
			fprintf(stderr, "*** Warning: Invalid member variable type: %s\n", var.mType.c_str());
			ss << "None";
		}
		else
		{
			if (obj->mIsEnum)
			{
				// This is an enum variable.
				// Presently, enums are represented as named integer constants.
				// If default value was given, use it verbatum.  Otherwise, use 0.
				if (!var.mDefaultValue.empty())
				{
					ss << var.mDefaultValue;
				}
				else
				{
					ss << 0;
				}
			}
			else
			{
				// Assume it's a class that will be initialized using its default constructor.
				if (!var.mDefaultValue.empty())
				{
					fprintf(stderr, "*** Warning: Don't know how to parse default value for type %s (%s): using default constructor\n", var.mType.c_str(), var.mDefaultValue.c_str());
				}
				ss << var.mType << "()";
			}
		}
	}

	return ss.str();
}

std::string getCppValueInitializer(const MemberVariable &var, const DOM &dom, bool isDef)
{
	std::ostringstream ss;

	// make sure floats and doubles always have a decimal point
	ss << std::showpoint;

	if (var.mIsArray)
	{
		if (!var.mDefaultValue.empty())
		{
			fprintf(stderr, "*** Warning: Don't know how to parse default values for arrays\n");
		}
	}
	else if (var.mType == "string")
	{
		ss << '"' << var.mDefaultValue << '"';
	}
	else if (var.mType == "i8" || var.mType == "i16" || var.mType == "i32" || var.mType == "i64")
	{
		long long i = 0;
		if (!var.mDefaultValue.empty())
		{
			i = std::strtoll(var.mDefaultValue.c_str(), nullptr, 0);
		}
		ss << i;
	}
	else if (var.mType == "u8" || var.mType == "u16" || var.mType == "u32" || var.mType == "u64")
	{
		unsigned long long u = 0;
		if (!var.mDefaultValue.empty())
		{
			u = std::strtoull(var.mDefaultValue.c_str(), nullptr, 0);
		}
		ss << u;
	}
	else if (var.mType == "float")
	{
		float f = 0.0f;
		bool addFloat = true;
		if (!var.mDefaultValue.empty())
		{
			const char *fvalue = var.mDefaultValue.c_str();
			if (strcmp(fvalue, "FLT_MAX") == 0)
			{
				f = FLT_MAX;
				ss << "FLT_MAX";
				addFloat = false;
			}
			else if (strcmp(fvalue, "FLT_MIN") == 0)
			{
				f = FLT_MIN;
				ss << "FLT_MIN";
				addFloat = false;
			}
			else
			{
				f = strtof(var.mDefaultValue.c_str(), nullptr);
			}
		}
		if (addFloat)
		{
			ss << f << 'f';
		}
	}
	else if (var.mType == "double")
	{
		double d = 0.0;
		if (!var.mDefaultValue.empty())
		{
			const char *fvalue = var.mDefaultValue.c_str();
			if (strcmp(fvalue, "FLT_MAX") == 0)
			{
				d = FLT_MAX;
			}
			else if (strcmp(fvalue, "FLT_MIN") == 0)
			{
				d = FLT_MIN;
			}
			else
			{
				d = strtod(var.mDefaultValue.c_str(), nullptr);
			}
		}
		ss << d;
	}
	else if (var.mType == "bool")
	{
		if (_stricmp(var.mDefaultValue.c_str(), "true") == 0)
		{
			ss << "true";
		}
		else
		{
			ss << "false";
		}
	}
	else if (var.mType == "Vec3")
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		if (!var.mDefaultValue.empty())
		{
			sscanf(var.mDefaultValue.c_str(), "%f,%f,%f", &x, &y, &z);
		}
		ss << x << "f, " << y << "f, " << z << 'f';
	}
	else if (var.mType == "Quat")
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 1.0f;
		if (!var.mDefaultValue.empty())
		{
			sscanf(var.mDefaultValue.c_str(), "%f,%f,%f,%f", &x, &y, &z, &w);
		}
		ss << x << "f, " << y << "f, " << z << "f, " << w << 'f';
	}
	else
	{
		// get some info about this variable's type
		Object* obj = dom.findObject(var.mType);
		if (!obj)
		{
			fprintf(stderr, "*** Warning: Invalid member variable type: %s\n", var.mType.c_str());
		}
		else
		{
			if (obj->mIsEnum)
			{
				// This is an enum variable.
				// If default value was given, use it verbatim.  Otherwise, use automatic default value.
				if (!var.mDefaultValue.empty())
				{
					ss << var.mQualifiedDefaultValue;
				}
			}
			else
			{
				// Assume it's a class that will be initialized using its default constructor.
				if (!var.mDefaultValue.empty())
				{
					fprintf(stderr, "*** Warning: Don't know how to parse default value for type %s (%s): using default constructor\n", var.mType.c_str(), var.mDefaultValue.c_str());
				}
			}
		}
	}

	return ss.str();
}

std::string getCppRValue(const MemberVariable &var, const DOM &dom, bool isDef)
{
	if (var.mIsArray)
	{
		if (!var.mDefaultValue.empty())
		{
			fprintf(stderr, "*** Warning: Don't know how to parse default values for arrays\n");
		}
		if (isDef)
		{
			return std::string("std::vector<") + getCppTypeString(var.mType.c_str(), isDef) + ">()";
		}
		else
		{
			return "nullptr";
		}
	}
	else if (var.mType == "string" ||
			 var.mType == "i8" || var.mType == "i16" || var.mType == "i32" || var.mType == "i64" ||
			 var.mType == "u8" || var.mType == "u16" || var.mType == "u32" || var.mType == "u64" ||
			 var.mType == "float" || var.mType == "double" ||
			 var.mType == "bool")
	{
		// strings and primitive values can be used as r-values directly
		return getCppValueInitializer(var, dom, isDef);
	}
	else
	{
		// get some info about this variable's type
		Object* obj = dom.findObject(var.mType);
		if (!obj)
		{
			fprintf(stderr, "*** Warning: Invalid variable type: %s\n", var.mType.c_str());
			return "void";
		}
		else
		{
			if (obj->mIsEnum)
			{
				// This is an enum variable.
				// If default value was given, use it verbatim.  Otherwise, use automatic default value.
				if (!var.mDefaultValue.empty())
				{
					return var.mQualifiedDefaultValue;
				}
				else
				{
					return var.mType + "()";
				}
			}
			else
			{
				// Assume it's a class
				return var.mType + '(' + getCppValueInitializer(var, dom, isDef) + ')';
			}
		}
	}
}

Object* typeInfo(const DOM& dom, const std::string& typeName)
{
	return dom.findObject(typeName);
}

static bool isEnumType(const DOM& dom, const std::string& typeName)
{
	Object* type = dom.findObject(typeName);
	return type ? type->mIsEnum : false;
}

static bool isClassType(const DOM& dom, const std::string& typeName)
{
	Object* type = dom.findObject(typeName);
	return type ? type->mIsClass : false;
}

}

