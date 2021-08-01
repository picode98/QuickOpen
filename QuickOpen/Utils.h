#pragma once

#include <shared_mutex>
#include <sstream>
#include <string>
#include <wx/string.h>
#include <wx/wxcrt.h>
#include <map>


#include <civetweb.h>

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

		T* operator->() const
		{
			return owner.obj;
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

		const T* operator->() const
		{
			return owner.obj;
		}

		~ReadableReference()
		{
			owner.unlockForReading();
		}
	};

	T* obj;

	WriterReadersLock(T* obj) : obj(obj)
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

long strtol(const wxString& str)
{
	long intVal;
	if(!str.ToLong(&intVal))
	{
		throw std::invalid_argument("Could not parse the supplied string as an integer.");
	}

	return intVal;
}

wxString getPlaceholderValue(const std::vector<wxString>& args, const std::map<wxString, wxString>& kwArgs, const wxString& placeholderStr, bool KWPlaceholder)
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

wxString substituteFormatString(const wxString& format, wxUniChar placeholderChar, const std::vector<wxString>& args, const std::map<wxString, wxString>& kwArgs)
{
	wxString resultStr, currentPlaceholderStr;
	bool parsingPlaceholder = false, parsingKWPlaceholder = false;

	for (wxUniChar thisChar : format)
	{
		if (parsingPlaceholder)
		{
			if (wxIsdigit(thisChar))
			{
				currentPlaceholderStr += thisChar;
			}
			else if(wxIsalpha(thisChar))
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
				resultStr += currentPlaceholderStr.empty() ? placeholderChar : (getPlaceholderValue(args, kwArgs, currentPlaceholderStr, parsingKWPlaceholder) + thisChar);
				parsingPlaceholder = parsingKWPlaceholder = false;
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
		resultStr += currentPlaceholderStr.empty() ? placeholderChar : getPlaceholderValue(args, kwArgs, currentPlaceholderStr, parsingKWPlaceholder);
	}

	return resultStr;
}

std::string MGReadAll(mg_connection* conn)
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