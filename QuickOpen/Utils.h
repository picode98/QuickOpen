#pragma once

#include <wx/string.h>
#include <wx/crt.h>
#include <wx/filename.h>

#include <memory>
#include <shared_mutex>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <optional>

#include "CivetWebIncludes.h"

#include <nlohmann/json.hpp>

enum class MessageSeverity
{
	MSG_ERROR,
	MSG_WARNING,
	MSG_INFO,
	MSG_DEBUG
};

inline std::map<wxString, wxString> getDependencyVersions()
{
	return {
		{ wxT("wxWidgets"), wxVERSION_NUM_DOT_STRING_T },
		{ wxT("nlohmann::json"), wxMAKE_VERSION_DOT_STRING_T(NLOHMANN_JSON_VERSION_MAJOR, NLOHMANN_JSON_VERSION_MINOR, NLOHMANN_JSON_VERSION_PATCH) },
		{ wxT("CivetWeb"), wxT(CIVETWEB_VERSION) }
	};
}

namespace nlohmann
{
	template<>
	struct adl_serializer<wxString>
	{
		static void to_json(json& j, const wxString& opt)
		{
			j = opt.ToUTF8();
		}

		static void from_json(const json& j, wxString& opt)
		{
			assert(j.is_string());
			opt.assign(wxString::FromUTF8(j.get<std::string>()));
		}
	};
}

inline wxFileName getDirName(const wxFileName& fileName)
{
	wxFileName newFileName(fileName);
	newFileName.SetFullName(wxEmptyString);
	return newFileName;
}

template<typename T>
class WriterReadersLock
{
	std::shared_mutex rwLock;

public:
	class WritableReference
	{
		WriterReadersLock& owner;

	public:
		WritableReference(WriterReadersLock& owner) : owner(owner)
		{
			owner.lockForWriting();
		}

		T& operator*() const
		{
			return *owner.obj.get();
		}

		T* operator->() const
		{
			return owner.obj.get();
		}

		~WritableReference()
		{
			owner.unlockForWriting();
		}
	};

	class ReadableReference
	{
		WriterReadersLock& owner;

	public:
		ReadableReference(WriterReadersLock& owner) : owner(owner)
		{
			owner.lockForReading();
		}

		const T& operator*() const
		{
			return *owner.obj.get();
		}

		const T* operator->() const
		{
			return owner.obj.get();
		}

		~ReadableReference()
		{
			owner.unlockForReading();
		}
	};

	std::unique_ptr<T> obj;

	WriterReadersLock(std::unique_ptr<T>& obj) : obj(std::move(obj))
	{}

	WriterReadersLock(std::unique_ptr<T>&& obj) : obj(std::move(obj))
	{}

	void lockForReading()
	{
		rwLock.lock_shared();
	}

	void lockForWriting()
	{
		rwLock.lock();
	}

	void unlockForReading()
	{
		rwLock.unlock_shared();
	}

	void unlockForWriting()
	{
		rwLock.unlock();
	}
};

template<typename T, T defaultValue>
class WithStaticDefault
{
	std::optional<T> value;
public:
	static inline const T DEFAULT_VALUE = defaultValue;

	WithStaticDefault() {}

	WithStaticDefault(const T& value) : value(value)
	{}

	T effectiveValue() const
	{
		return value.value_or(defaultValue);
	}

	operator T() const
	{
		return effectiveValue();
	}

	WithStaticDefault& operator=(const T& rhs)
	{
		this->value = rhs;
		return *this;
	}

	void reset()
	{
		this->value.reset();
	}

	bool isSet() const
	{
		return this->value.has_value();
	}
};

namespace nlohmann
{
	template<typename T, T defaultValue>
	struct adl_serializer<WithStaticDefault<T, defaultValue>>
	{
		static void to_json(json& j, const WithStaticDefault<T, defaultValue>& opt)
		{
			if(opt.isSet())
			{
				j = static_cast<T>(opt);
			}
			else
			{
				j = nullptr;
			}
		}

		static void from_json(const json& j, WithStaticDefault<T, defaultValue>& opt)
		{
			if(j.is_null())
			{
				opt.reset();
			}
			else
			{
				opt = static_cast<T>(j);
			}
		}
	};
}

inline long strtol(const wxString& str)
{
	long intVal;
	if (!str.ToLong(&intVal))
	{
		throw std::invalid_argument("Could not parse the supplied string as an integer.");
	}

	return intVal;
}

inline wxFileName operator/(const wxFileName& lhs, const wxFileName& rhs)
{
    wxFileName result(rhs);
    result.Normalize(wxPATH_NORM_ALL, lhs.GetPath());
    return result;
}

inline wxString getPlaceholderValue(const std::vector<wxString>& args, const std::map<wxString, wxString>& kwArgs, const wxString& placeholderStr, bool KWPlaceholder)
{
	if (KWPlaceholder)
	{
		return kwArgs.at(placeholderStr);
	}
	else
	{
		return args.at(strtol(placeholderStr) - 1);
	}
}

inline wxString substituteFormatString(const wxString& format, wxUniChar placeholderChar, const std::vector<wxString>& args,
	const std::map<wxString, wxString>& kwArgs)
{
	wxString resultStr, currentPlaceholderStr;
	bool parsingPlaceholder = false, parsingKWPlaceholder = false;

	for (size_t i = 0; i < format.size(); ++i)
	{
		wxUniChar thisChar = format.GetChar(i); // This is not very efficient (O(n^2)), but there seem to be linking problems with wxString::const_iterator ...
		if (parsingPlaceholder)
		{
			if (wxIsdigit(thisChar))
			{
				currentPlaceholderStr += thisChar;
			}
			else if (wxIsalpha(thisChar))
			{
				currentPlaceholderStr += thisChar;
				parsingKWPlaceholder = true;
			}
			else if (thisChar == placeholderChar)
			{
				if (currentPlaceholderStr.empty())
				{
					resultStr += placeholderChar;
					parsingPlaceholder = parsingKWPlaceholder = false;
				}
				else
				{
					resultStr += getPlaceholderValue(args, kwArgs, currentPlaceholderStr, parsingKWPlaceholder);
				}
			}
			else
			{
				resultStr += currentPlaceholderStr.empty()
					? placeholderChar
					: (getPlaceholderValue(args, kwArgs, currentPlaceholderStr, parsingKWPlaceholder) +
						thisChar);
				parsingPlaceholder = parsingKWPlaceholder = false;
				currentPlaceholderStr = wxEmptyString;
			}
		}
		else if (thisChar == placeholderChar)
		{
			parsingPlaceholder = true;
		}
		else
		{
			resultStr += thisChar;
		}
	}

	if (parsingPlaceholder)
	{
		resultStr += currentPlaceholderStr.empty()
			? placeholderChar
			: getPlaceholderValue(args, kwArgs, currentPlaceholderStr, parsingKWPlaceholder);
	}

	return resultStr;
}

template<typename ObjType, typename PrefixType>
bool startsWith(const ObjType& obj, const PrefixType& prefix)
{
    auto objIter = obj.begin();
    auto prefixIter = prefix.begin();

    for (; objIter != obj.end() && prefixIter != prefix.end();
           ++objIter, ++prefixIter)
    {
        if (*objIter != *prefixIter) return false;
    }

    return (prefixIter == prefix.end());
}

inline std::string MGReadAll(mg_connection* conn)
{
	std::stringstream strBuilder;

	static const long long CHUNK_SIZE = 1LL << 16;
	char bodyBuffer[CHUNK_SIZE];

	int bytesRead;
	while ((bytesRead = mg_read(conn, bodyBuffer, CHUNK_SIZE)) > 0)
	{
		strBuilder << std::string(bodyBuffer, bytesRead);
	}

	return strBuilder.str();
}

inline std::string fileReadAll(const wxFileName& path)
{
	std::ifstream fileIn;
	std::string fileStr;
	fileIn.exceptions(std::ios::failbit);
#ifdef WIN32
	fileIn.open(path.GetFullPath().ToStdWstring());
#else
    fileIn.open(path.GetFullPath().ToStdString());
#endif
	std::stringstream dataStream;
	dataStream << fileIn.rdbuf();
	fileIn.close();

	return dataStream.str();
}

#define MAP_VECTOR(ResultType, input, ResultName, expr, cond) \
std::vector<ResultType> ResultName; \
for(const auto& elem : input) \
{ \
    if(cond) \
    { \
    ResultName.emplace_back(expr); \
    } \
}