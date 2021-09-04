#pragma once

#include <wx/window.h>
#include <wx/filename.h>

#include <string>
#include <vector>
#include <regex>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <bcrypt.h>

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
void startSubprocess(const wxString& commandLine);
tstring substituteWinShellFormatString(const tstring& format, const std::vector<tstring>& args);

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

std::filesystem::path getAppExecutablePath();

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