#include "WinUtils.h"
#include "Utils.h"
#include "ApplicationInfo.h"

#include <wx/stdpaths.h>

#include <map>

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

WindowsException getWinAPIError(LSTATUS retVal)
{
	if (retVal != ERROR_SUCCESS)
	{
		TCHAR* errorStrPtr;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
		              nullptr, retVal,
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

void handleWinAPIError(LSTATUS retVal, bool checkGLE)
{
	WindowsException error(ERROR_SUCCESS, tstring(TEXT("")));
	auto test = GetLastError();
	if (retVal != ERROR_SUCCESS)
	{
		error = getWinAPIError(retVal);
	}
	else if (checkGLE)
	{
		error = getWinAPIError(GetLastError());
	}

	if (error.code().value() != ERROR_SUCCESS)
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
	CreateProcess(nullptr, commandLineCopy.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo,
	              &newProcInfo);
	handleWinAPIError(ERROR_SUCCESS);
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
			else if (thisChar == TEXT('%'))
			{
				if (currentNumStr.empty())
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

	if (parsingPlaceholder)
	{
		resultStr += currentNumStr.empty() ? TEXT("%") : args.at(std::stoi(currentNumStr) - 1);
	}

	return resultStr;
}

std::vector<WebBrowserInfo> getInstalledWebBrowsers()
{
	static const tstring START_MENU_INTERNET_KEY_NAME = TEXT("SOFTWARE\\Clients\\StartMenuInternet");

	HKEY startMenuInternetKey;
	handleWinAPIError(RegOpenKeyEx(HKEY_LOCAL_MACHINE, START_MENU_INTERNET_KEY_NAME.c_str(), 0, KEY_READ,
	                               &startMenuInternetKey));

	DWORD numSubkeys, maxSubkeyLength;
	handleWinAPIError(RegQueryInfoKey(startMenuInternetKey, nullptr, nullptr, nullptr, &numSubkeys, &maxSubkeyLength,
	                                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));

	std::vector<WebBrowserInfo> resultList(numSubkeys);
	for (DWORD i = 0; i < numSubkeys; ++i)
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

	tstring defaultBrowserProgID = readRegistryStringValue(
		HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice"),
		TEXT("ProgId"));

	tstring browserPath = readRegistryStringValue(
		HKEY_CLASSES_ROOT, defaultBrowserProgID + TEXT("\\shell\\open\\command"), TEXT(""));
	return browserPath;
}

wxFileName getAppExecutablePath()
{
	static const DWORD START_BUF_SIZE = MAX_PATH;
	DWORD currentBufSize = START_BUF_SIZE;
	TCHAR* filenameBuf = new TCHAR[currentBufSize];

	while (true)
	{
		try
		{
			SetLastError(ERROR_SUCCESS);
			GetModuleFileName(nullptr, filenameBuf, currentBufSize);
			handleWinAPIError(ERROR_SUCCESS);

			auto result = wxFileName(filenameBuf);
			delete[] filenameBuf;
			return result;
		}
		catch (const WindowsException& ex)
		{
			delete[] filenameBuf;

			if (ex.code().value() == ERROR_INSUFFICIENT_BUFFER)
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

void shellExecuteFile(const wxFileName& filePath, const wxWindow* window)
{
	HWND windowHandle = nullptr;
	if (window != nullptr)
	{
		windowHandle = window->GetHWND();
	}

	ShellExecute(windowHandle, TEXT("open"), wxStringToTString(filePath.GetFullPath()).c_str(), nullptr,
	             wxStringToTString(filePath.GetPath()).c_str(), SW_SHOWNORMAL);
	handleWinAPIError(ERROR_SUCCESS);
}

void openExplorerFolder(const wxFileName& folder, const wxFileName* selectedFile)
{
	wxString winDirStr;
	wxFileName winDir;
	wxGetEnv(wxT("windir"), &winDirStr);
	winDir.AssignDir(winDirStr);

	wxFileName explorerPath(winDir);
	explorerPath.SetName(wxT("explorer.exe"));

	if (selectedFile != nullptr)
	{
		startSubprocess(
			wxString() << wxT("\"") << explorerPath.GetFullPath() << wxT("\" /select,\"") << selectedFile->GetFullPath()
			<< wxT("\""));
	}
	else
	{
		startSubprocess(
			wxString() << wxT("\"") << explorerPath.GetFullPath() << wxT("\" /select,\"") << folder.GetFullPath() <<
			wxT("\""));
	}
}

void initializeCOM()
{
	handleWinAPIError(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

	try
	{
		handleWinAPIError(CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
			RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr));
	}
	catch (const WindowsException&)
	{
		CoUninitialize();
		throw;
	}
}

WMIResultIterator runWMIQuery(const wxString& wmiNamespace, const wxString& query)
{
	// initializeCOM();
	SetLastError(ERROR_SUCCESS);

	IWbemLocator* wmiLocator = nullptr;
	handleWinAPIError(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, reinterpret_cast<LPVOID*>(&wmiLocator)), false);

	IWbemServices* wmiSvc = nullptr;
	handleWinAPIError(wmiLocator->ConnectServer(_bstr_t(wmiNamespace.ToStdWstring().c_str()), nullptr, nullptr,
		nullptr, 0, nullptr, nullptr, &wmiSvc), false);

	handleWinAPIError(CoSetProxyBlanket(wmiSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
		RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE), false);

	IEnumWbemClassObject* resultIterator = nullptr;
	handleWinAPIError(wmiSvc->ExecQuery(_bstr_t("WQL"), _bstr_t(query.ToStdWstring().c_str()),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &resultIterator), false);

	return WMIResultIterator(resultIterator);
}

inline wxString bStrToWXString(BSTR bStr)
{
	return wxString(std::wstring(bStr, SysStringLen(bStr)));
}

IPAddress parseWMIIPAddress(const wxString& addressStr)
{
	size_t firstColonIndex = addressStr.find(wxT(":"));
	if (firstColonIndex != wxString::npos)
	{
		unsigned long firstOctetVal;
		addressStr.substr(0, firstColonIndex).ToULong(&firstOctetVal, 16);
		return { IPAddress::IPV6, addressStr, ((firstOctetVal >> 6) << 6) == 0xfe80UL };
	}
	else
	{
		wxArrayString addrParts = wxSplit(addressStr, wxChar('.'), wxChar('\0'));
		if(addrParts.size() != 4)
		{
			throw std::domain_error("Could not parse the given address.");
		}

		return { IPAddress::IPV4, addressStr, addrParts[0] == wxT("169") && addrParts[1] == wxT("254") };
	}
}

std::vector<NetworkInterfaceInfo> getPhysicalNetworkInterfaces()
{
	std::vector<NetworkInterfaceInfo> resultList;
	std::map<_bstr_t, _bstr_t> interfaceIDs;

	{
		WMIResultIterator iter = runWMIQuery(wxT("root\\StandardCimv2"), wxT("SELECT DeviceID, Name FROM MSFT_NetAdapter WHERE ConnectorPresent = TRUE"));
		while (iter)
		{
			VARIANT IDProp, intNameProp;
			iter->Get(L"DeviceID", 0, &IDProp, nullptr, nullptr);
			iter->Get(L"Name", 0, &intNameProp, nullptr, nullptr);

			interfaceIDs.insert({ IDProp.bstrVal, intNameProp.bstrVal });

			++iter;
		}
	}

	{
		WMIResultIterator iter = runWMIQuery(wxT("root\\CIMV2"), wxT("SELECT Description, IPAddress, SettingID FROM Win32_NetworkAdapterConfiguration"));
		while (iter)
		{
			VARIANT IDProp;
			iter->Get(L"SettingID", 0, &IDProp, nullptr, nullptr);
			if(interfaceIDs.count(IDProp.bstrVal) > 0)
			{
				VARIANT driverNameProp, addressesProp;

				iter->Get(L"Description", 0, &driverNameProp, nullptr, nullptr);
				iter->Get(L"IPAddress", 0, &addressesProp, nullptr, nullptr);

				if (addressesProp.parray != nullptr)
				{
					std::vector<IPAddress> addresses;

					LONG i, upperBound;

					handleWinAPIError(SafeArrayGetLBound(addressesProp.parray, 1, &i), false);
					handleWinAPIError(SafeArrayGetUBound(addressesProp.parray, 1, &upperBound), false);

					for (; i <= upperBound; ++i)
					{
						BSTR thisAddressObj;
						// VariantInit(&thisAddressObj);
						handleWinAPIError(SafeArrayGetElement(V_ARRAY(&addressesProp), &i, &thisAddressObj), false);
						wxString convertedAddr = bStrToWXString(thisAddressObj);
						addresses.push_back(parseWMIIPAddress(convertedAddr));
					}

					resultList.push_back({ bStrToWXString(IDProp.bstrVal), bStrToWXString(interfaceIDs[IDProp.bstrVal]), bStrToWXString(driverNameProp.bstrVal), addresses });
				}
			}

			++iter;
		}
	}

	return resultList;
}

InstallationInfo InstallationInfo::detectInstallation()
{
	wxFileName appExePath = getAppExecutablePath();
	appExePath.SetName("");

	wxString staticEnvVar;
	bool staticEnvVarSet = wxGetEnv(wxT("QUICKOPEN_DATA_DIR"), &staticEnvVar);

	try
	{
		tstring installPathStr = readRegistryStringValue(HKEY_LOCAL_MACHINE, 
			tstring(TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{")) + TEXT(QUICKOPEN_WINDOWS_PRODUCT_GUID) + TEXT("}"), TEXT("InstallLocation"));
		auto installPath = wxFileName(installPathStr, "");
		if(startsWith(appExePath.GetDirs(), installPath.GetDirs()))
		{
			return { INSTALLED_SYSTEM, appExePath, wxFileName(wxStandardPaths::Get().GetUserConfigDir(), "") / wxFileName("QuickOpen", ""),
				staticEnvVarSet ? wxFileName(staticEnvVar, "") : (installPath / wxFileName("share/QuickOpen", "")) };
		}
		else
		{
			return { NOT_INSTALLED, appExePath, appExePath,
				staticEnvVarSet ? wxFileName(staticEnvVar, "") : (appExePath / wxFileName("../share/QuickOpen", "")) };
		}
	}
	catch (const WindowsException& ex)
	{
		if(ex.code().value() == ERROR_FILE_NOT_FOUND)
		{
			return { NOT_INSTALLED, appExePath, appExePath,
				staticEnvVarSet ? wxFileName(staticEnvVar, "") : (appExePath / wxFileName("../share/QuickOpen", "")) };
		}
		else
		{
			throw;
		}
	}
}