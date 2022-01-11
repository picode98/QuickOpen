#pragma once

#include <wx/window.h>
#include <wx/filename.h>

#include <string>
#include <vector>
#include <regex>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <Psapi.h>

#include <filesystem>
#include <iostream>

#ifdef UNICODE
typedef std::wstring tstring;
#else
typedef std::string tstring;
#endif

std::wstring UTF8StrToWideStr(const std::string& UTF8Str);
std::string WideStrToUTF8Str(const std::wstring& wideStr);

#ifdef UNICODE
#define TStringToUTF8String WideStrToUTF8Str
#else
#define TStringToUTF8String(str) str
#endif

inline tstring wxStringToTString(const wxString& str)
{
#ifdef UNICODE
	return str.ToStdWstring();
#else
	return str.ToStdString();
#endif
}

class WindowsException : public std::system_error
{
public:
	WindowsException(DWORD errorCode, const tstring& description): system_error(errorCode, std::system_category(), TStringToUTF8String(description))
	{}
};

WindowsException getWinAPIError(LSTATUS retVal);
void handleWinAPIError(LSTATUS retVal, bool checkGLE = true);
tstring readRegistryStringValue(HKEY key, const tstring& subkeyName, const tstring& valueName);
DWORD startSubprocess(const wxString& commandLine);
DWORD startSubprocess(const wxString& exePath, const std::vector<wxString>& args);
tstring substituteWinShellFormatString(const tstring& format, const std::vector<tstring>& args);

void initializeCOM();

class WMIResultIterator
{
	IEnumWbemClassObject* wmiIterator = nullptr;
	IWbemClassObject* currentResult = nullptr;

public:
	WMIResultIterator(IEnumWbemClassObject* wmiIterator):
		wmiIterator(wmiIterator)
	{
		++(*this);
	}

	WMIResultIterator(const WMIResultIterator& existing)
	{
		existing.wmiIterator->Clone(&wmiIterator);

		if (currentResult != nullptr)
		{
			existing.currentResult->Clone(&currentResult);
		}
	}

	IWbemClassObject* operator->() const
	{
		return currentResult;
	}

	WMIResultIterator& operator++()
	{
		if(currentResult != nullptr)
		{
			currentResult->Release();
		}

		ULONG numResults;
		wmiIterator->Next(WBEM_INFINITE, 1, &currentResult, &numResults);

		if(numResults == 0)
		{
			currentResult = nullptr;
		}

		return *this;
	}

	operator bool() const
	{
		return currentResult != nullptr;
	}

	~WMIResultIterator()
	{
		wmiIterator->Release();

		if(currentResult != nullptr)
		{
			currentResult->Release();
		}
	}
};

WMIResultIterator runWMIQuery(const wxString& wmiNamespace, const wxString& query);

struct IPAddress
{
	enum IPAddressType
	{
		IPV4,
		IPV6
	};

	IPAddressType type;
	wxString addressStr;
	bool isLinkLocal;
};

struct NetworkInterfaceInfo
{
#define NetworkInterfaceInfo_GUID_AVAILABLE true
#define NetworkInterfaceInfo_DRIVER_NAME_AVAILABLE true
	wxString GUID, interfaceName, driverName;
	std::vector<IPAddress> IPAddresses;
};

std::vector<NetworkInterfaceInfo> getPhysicalNetworkInterfaces();

struct WebBrowserInfo
{
	std::string browserID, browserName;
	tstring browserExecCommandFormat;
};

std::vector<WebBrowserInfo> getInstalledWebBrowsers();
tstring getDefaultBrowserCommandLine();

inline wxString getBrowserCommandLine(const wxString& browserID)
{
	return wxString(readRegistryStringValue(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Clients\\StartMenuInternet\\") + wxStringToTString(browserID) + TEXT("\\shell\\open\\command"), TEXT("")));
}

wxFileName getAppExecutablePath();

template<typename T>
T generateCryptoRandomInteger()
{
	T result;
	BCryptGenRandom(nullptr, reinterpret_cast<unsigned char*>(&result), sizeof(T), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	return result;
}

inline std::filesystem::path UTF8StrToPath(const std::string& UTF8Str)
{
	return std::filesystem::path(UTF8StrToWideStr(UTF8Str));
}

void shellExecuteFile(const wxFileName& filePath, const wxWindow* window = nullptr);
void openExplorerFolder(const wxFileName& folder, const wxFileName* selectedFile = nullptr);

inline void activateWindow(wxWindow* window)
{
    SetForegroundWindow(window->GetHandle());
}

struct InstallationInfo
{
	enum InstallationType
	{
		NOT_INSTALLED,
		INSTALLED_SYSTEM
	};

	InstallationType installType;

	wxFileName binaryFolder,
		configFolder,
		dataFolder;

	static InstallationInfo detectInstallation();
};

enum StartupEntryState
{
	ABSENT,
	PRESENT,
	DIFFERENT_APPLICATION
};

void addUserStartupEntry();
void removeUserStartupEntry();
StartupEntryState getStartupEntryState();

inline UINT winGetConfigUpdateMessageID()
{
	static UINT msgID = RegisterWindowMessage(TEXT("UpdateConfiguration"));
	return msgID;
}

template<typename T>
std::vector<T> retryUntilLargeEnough(std::function<bool(std::vector<T>&)> attemptFunc, size_t initialSize = 32)
{
	std::vector<T> objects(initialSize);
	bool result = attemptFunc(objects);

	while(!result)
	{
		objects.resize(objects.size() * 2);
		result = attemptFunc(objects);
	}

	return objects;
}

const DWORD WINDOW_FOUND = (1 << 29) + 1;

void broadcastConfigUpdate();