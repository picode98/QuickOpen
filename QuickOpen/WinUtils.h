#pragma once

#include <string>
#include <vector>
#include <regex>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <filesystem>

#ifdef UNICODE
typedef std::wstring tstring;
#else
typedef std::string tstring;
#endif

std::wstring UTF8StrToWideStr(const std::string& UTF8Str)
{
	int reqBufSize = MultiByteToWideChar(CP_UTF8, 0, UTF8Str.c_str(), UTF8Str.size(), nullptr, 0);
	std::wstring str(reqBufSize, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, UTF8Str.c_str(), UTF8Str.size(), str.data(), str.size());

	return str;
}

std::string WideStrToUTF8Str(const std::wstring& wideStr)
{
	if (wideStr.empty()) return "";
	
	int reqBufSize = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), wideStr.size(), nullptr, 0, nullptr, nullptr);
	std::string str(reqBufSize, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), wideStr.size(), str.data(), str.size(), nullptr, nullptr);

	return str;
}

#ifdef UNICODE
#define TStringToUTF8String WideStrToUTF8Str
#else
#define TStringToUTF8String(str) str
#endif

tstring wxStringToTString(const wxString& str)
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

WindowsException getWinAPIError(LSTATUS retVal)
{
	if (retVal != ERROR_SUCCESS)
	{
		TCHAR* errorStrPtr;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, retVal,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&errorStrPtr), 0, nullptr);
		tstring errorDesc(errorStrPtr);
		LocalFree(errorStrPtr);

		return WindowsException(retVal, errorDesc);
	}
	else
	{
		return WindowsException(ERROR_SUCCESS, TEXT(""));
	}
}

void handleWinAPIError(LSTATUS retVal, bool checkGLE = true)
{
	WindowsException error(ERROR_SUCCESS, tstring(TEXT("")));
	auto test = GetLastError();
	if(retVal != ERROR_SUCCESS)
	{
		error = getWinAPIError(retVal);
	}
	else if(checkGLE)
	{
		error = getWinAPIError(GetLastError());
	}
	
	if(error.code().value() != ERROR_SUCCESS)
	{
		throw error;
	}
}

tstring readRegistryStringValue(HKEY key, const tstring& subkeyName, const tstring& valueName)
{
	SetLastError(ERROR_SUCCESS);
	HKEY URLAssocHandle = nullptr;
	auto keyType = REG_NONE;
	handleWinAPIError(RegOpenKeyEx(key, subkeyName.c_str(), 0, KEY_READ, &URLAssocHandle));
	DWORD reqBufSize(0);
	handleWinAPIError(RegQueryValueEx(URLAssocHandle, valueName.c_str(), nullptr, &keyType, nullptr, &reqBufSize));
	std::basic_string<BYTE> str(reqBufSize, BYTE(0));
	handleWinAPIError(RegQueryValueEx(URLAssocHandle, valueName.c_str(), nullptr, &keyType, str.data(), &reqBufSize));

	return tstring(reinterpret_cast<const TCHAR*>(str.c_str()));
}

void startSubprocess(const wxString& commandLine)
{
	// See https://docs.microsoft.com/en-us/windows/win32/procthread/creating-processes
	STARTUPINFO startupInfo;
	ZeroMemory(&startupInfo, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);

	PROCESS_INFORMATION newProcInfo;
	ZeroMemory(&newProcInfo, sizeof(newProcInfo));

	//tstring fullCommandLine = TEXT('\"') + path + TEXT('\"');

	//for(const tstring& arg : args)
	//{
	//	fullCommandLine += TEXT(" \"") + arg + TEXT('\"');
	//}
	tstring commandLineCopy = commandLine;

	std::wcout << L"Creating process " << commandLineCopy << std::endl;
	
	SetLastError(ERROR_SUCCESS);
	CreateProcess(nullptr, commandLineCopy.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &newProcInfo); handleWinAPIError(ERROR_SUCCESS);
}

tstring substituteWinShellFormatString(const tstring& format, const std::vector<tstring>& args)
{
	tstring resultStr, currentNumStr;
	bool parsingPlaceholder = false;

	for (TCHAR thisChar : format)
	{
		if (parsingPlaceholder)
		{
#ifdef UNICODE
			if (iswdigit(thisChar))
			{
#else
			if (isdigit(thisChar))
			{
#endif
				currentNumStr += thisChar;
			}
			else if(thisChar == TEXT('%'))
			{
				if(currentNumStr.empty())
				{
					resultStr += TEXT('%');
					parsingPlaceholder = false;
				}
				else
				{
					resultStr += args.at(std::stoi(currentNumStr) - 1);
				}
			}
			else
			{
				resultStr += currentNumStr.empty() ? TEXT("%") : args.at(std::stoi(currentNumStr) - 1);
				parsingPlaceholder = false;
			}
		}
		else if (thisChar == TEXT('%'))
		{
			parsingPlaceholder = true;
		}
		else
		{
			resultStr += thisChar;
		}
	}

	if(parsingPlaceholder)
	{
		resultStr += currentNumStr.empty() ? TEXT("%") : args.at(std::stoi(currentNumStr) - 1);
	}
	
	return resultStr;
}

struct WebBrowserInfo
{
	std::string browserID, browserName;
	tstring browserExecCommandFormat;
};

std::vector<WebBrowserInfo> getInstalledWebBrowsers()
{
	static const tstring START_MENU_INTERNET_KEY_NAME = TEXT("SOFTWARE\\Clients\\StartMenuInternet");

	HKEY startMenuInternetKey;
	handleWinAPIError(RegOpenKeyEx(HKEY_LOCAL_MACHINE, START_MENU_INTERNET_KEY_NAME.c_str(), 0, KEY_READ, &startMenuInternetKey));

	DWORD numSubkeys, maxSubkeyLength;
	handleWinAPIError(RegQueryInfoKey(startMenuInternetKey, nullptr, nullptr, nullptr, &numSubkeys, &maxSubkeyLength, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));

	std::vector<WebBrowserInfo> resultList(numSubkeys);
	for(DWORD i = 0; i < numSubkeys; ++i)
	{
		DWORD bufLength = maxSubkeyLength + 1;
		TCHAR* keyName = new TCHAR[bufLength];
		RegEnumKeyEx(startMenuInternetKey, i, keyName, &bufLength, nullptr, nullptr, nullptr, nullptr);

#ifdef UNICODE
		resultList[i] = {
			WideStrToUTF8Str(keyName),
			WideStrToUTF8Str(readRegistryStringValue(startMenuInternetKey, keyName, TEXT(""))),
			readRegistryStringValue(startMenuInternetKey, tstring(keyName) + TEXT("\\shell\\open\\command"), TEXT(""))
		};
#else
		resultList[i] = {
			keyName,
			readRegistryStringValue(startMenuInternetKey, keyName, TEXT("")),
			readRegistryStringValue(startMenuInternetKey, tstring(keyName) + TEXT("\\shell\\open\\command"), TEXT(""))
		};
#endif

		delete[] keyName;
	}

	handleWinAPIError(RegCloseKey(startMenuInternetKey));

	return resultList;
}

tstring getDefaultBrowserCommandLine()
{
	// tstring test = readRegistryStringValue(HKEY_CLASSES_ROOT, TEXT("foo"), TEXT(""));

	tstring defaultBrowserProgID = readRegistryStringValue(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice"), TEXT("ProgId"));

	tstring browserPath = readRegistryStringValue(HKEY_CLASSES_ROOT, defaultBrowserProgID + TEXT("\\shell\\open\\command"), TEXT(""));
	return browserPath;
}

wxString getBrowserCommandLine(const wxString& browserID)
{
	return wxString(readRegistryStringValue(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Clients\\StartMenuInternet\\") + wxStringToTString(browserID) + TEXT("\\shell\\open\\command"), TEXT("")));
}

std::filesystem::path getAppExecutablePath()
{
	static const DWORD START_BUF_SIZE = MAX_PATH;
	DWORD currentBufSize = START_BUF_SIZE;
	TCHAR* filenameBuf = new TCHAR[currentBufSize];

	while(true)
	{
		try
		{
			SetLastError(ERROR_SUCCESS);
			GetModuleFileName(nullptr, filenameBuf, currentBufSize);
			handleWinAPIError(ERROR_SUCCESS);
			
			auto result = std::filesystem::path(filenameBuf);
			delete[] filenameBuf;
			return result;
		}
		catch (const WindowsException& ex)
		{
			delete[] filenameBuf;
			
			if(ex.code().value() == ERROR_INSUFFICIENT_BUFFER)
			{
				currentBufSize *= 2;
				filenameBuf = new TCHAR[currentBufSize];
			}
			else
			{
				throw;
			}
		}
	}
}

template<typename T>
T generateCryptoRandomInteger()
{
	T result;
	BCryptGenRandom(nullptr, reinterpret_cast<unsigned char*>(&result), sizeof(T), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	return result;
}

std::filesystem::path UTF8StrToPath(const std::string& UTF8Str)
{
	return std::filesystem::path(UTF8StrToWideStr(UTF8Str));
}

void shellExecuteFile(const wxFileName& filePath, const wxWindow* window = nullptr)
{
	HWND windowHandle = nullptr;
	if(window != nullptr)
	{
		windowHandle = window->GetHWND();
	}
	
	ShellExecute(windowHandle, TEXT("open"), wxStringToTString(filePath.GetFullPath()).c_str(), nullptr, wxStringToTString(filePath.GetPath()).c_str(), SW_SHOWNORMAL);
	handleWinAPIError(ERROR_SUCCESS);
}

void openExplorerFolder(const wxFileName& folder, const wxFileName* selectedFile = nullptr)
{
	wxString winDirStr;
	wxFileName winDir;
	wxGetEnv(wxT("windir"), &winDirStr);
	winDir.AssignDir(winDirStr);
	
	wxFileName explorerPath(winDir);
	explorerPath.SetName(wxT("explorer.exe"));
	
	if(selectedFile != nullptr)
	{
		startSubprocess(wxString() << wxT("\"") << explorerPath.GetFullPath() << wxT("\" /select,\"") << selectedFile->GetFullPath() << wxT("\""));
	}
	else
	{
		startSubprocess(wxString() << wxT("\"") << explorerPath.GetFullPath() << wxT("\" /select,\"") << folder.GetFullPath() << wxT("\""));
	}
}