//
// Created by sdk on 8/30/21.
//

#include "LinuxUtils.h"
#include "Utils.h"

#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <climits>

#include <regex>
#include <set>
#include <iostream>

void handleLinuxSystemError(bool errCond)
{
    if(errCond)
    {
        int errID = errno;
        throw LinuxException(errID, strerror(errID));
    }
}

wxFileName getAppExecutablePath()
{
    char procNameBuf[PATH_MAX] = {};
    handleLinuxSystemError(readlink("/proc/self/exe", procNameBuf, PATH_MAX) == -1);
    return { procNameBuf };
}

//void startSubprocess(const wxString& executable, std::vector<wxString>& args)
//{
//    if(fork() == 0)
//    {
//        char** argv = new char*[args.size() + 1];
//
//        std::vector<std::string> convertedStrings(args.size());
//        for(size_t i = 0; i < args.size(); ++i)
//        {
//            convertedStrings[i] = args[i].ToUTF8();
//            argv[i] = convertedStrings[i].data();
//        }
//
//        argv[args.size()] = nullptr;
//
//        execvp(executable.ToUTF8(), argv);
//        handleLinuxSystemError(true); // If this point is reached, something went wrong.
//    }
//}

void startSubprocess(const wxString& commandLine)
{
    wxExecute(commandLine, wxEXEC_ASYNC);
}

void startSubprocess(const wxString& exePath, const std::vector<wxString>& args)
{
    pid_t childPID = fork();
    handleLinuxSystemError(childPID == -1);
    if(childPID == 0)
    {
        char* argvArray[args.size() + 2];
        argvArray[0] = strdup(exePath.ToUTF8());

        for(unsigned i = 0; i < args.size(); ++i)
        {
            argvArray[i + 1] = strdup(args[i].ToUTF8());
        }

        argvArray[args.size() + 1] = nullptr;

        execvp(exePath.c_str(), argvArray);
        try
        {
            handleLinuxSystemError(true); // If this point is reached, something went wrong.
        }
        catch(LinuxException& ex)
        {
            std::cerr << "ERROR: Child process failed to load new program: " << ex.what() << std::endl;
            exit(1);
        }
    }
}

void shellExecuteFile(const wxFileName& filePath, const wxWindow* window)
{
    // std::string filePathStr = static_cast<std::string>(filePath.GetFullPath().ToUTF8());
    // const char* argv[2] = {  "xdg-open", filePathStr.c_str() };
    // wxExecute(argv, wxEXEC_ASYNC);

    startSubprocess(wxT("xdg-open"), { filePath.GetFullPath() });
}

void openExplorerFolder(const wxFileName& folder, const wxFileName* selectedFile)
{
    shellExecuteFile(folder, nullptr);
}

bool isLinkLocal(in6_addr address)
{
    const uint16_t LINK_LOCAL_PREFIX = 0xfe80, NUM_COMPARE_BITS = 10,
            BITS_TO_SHIFT = 16 - NUM_COMPARE_BITS;

    auto* addressBytes = reinterpret_cast<uint8_t*>(&address);

    return addressBytes[0] == 0xfe && ((addressBytes[1] >> 6) << 6) == 0x80;
}

bool isLinkLocal(in_addr address)
{
    auto* byteView = reinterpret_cast<uint8_t*>(&(address.s_addr));
    return byteView[0] == static_cast<uint8_t>(169) && byteView[1] == static_cast<uint8_t>(254);
}

IPAddress getSockaddrInfo(sockaddr* address, const wxString& interfaceName)
{
    switch(address->sa_family)
    {
        case AF_INET:
        {
            char strBuf[INET_ADDRSTRLEN];
            in_addr ipv4Address = reinterpret_cast<sockaddr_in*>(address)->sin_addr;
            handleLinuxSystemError(inet_ntop(AF_INET, &ipv4Address, strBuf, sizeof(strBuf)) == nullptr);
            return { IPAddress::IPV4, wxString::FromAscii(strBuf), isLinkLocal(ipv4Address) };
        }
        case AF_INET6:
        {
            char strBuf[INET6_ADDRSTRLEN];
            in6_addr ipv6Address = reinterpret_cast<sockaddr_in6*>(address)->sin6_addr;

            handleLinuxSystemError(inet_ntop(AF_INET6, &ipv6Address, strBuf, sizeof(strBuf)) == nullptr);
            bool llAddr = isLinkLocal(ipv6Address);
            return { IPAddress::IPV6,
                     (llAddr ? (wxString::FromAscii(strBuf) + wxT("%") + interfaceName) : wxString::FromAscii(strBuf)), isLinkLocal(ipv6Address) };
        }
        default:
            throw std::domain_error("Handling for this address type has not been implemented.");
    }
}

std::vector<NetworkInterfaceInfo> getPhysicalNetworkInterfaces()
{
    using namespace std::filesystem;
    path adapterInfoPath = "/sys/class/net", virtualDevPath = "/sys/devices/virtual";

    std::set<std::string> physicalAdapterNames;

    for(const path& thisPath : directory_iterator(adapterInfoPath))
    {
         path thisTarget = read_symlink(thisPath);

         if(thisTarget.is_relative())
         {
             thisTarget = canonical(adapterInfoPath / thisTarget);
         }

         if(!startsWith(thisTarget, virtualDevPath))
         {
             physicalAdapterNames.insert(thisTarget.filename());
         }
    }

    ifaddrs* addrList = nullptr;
    getifaddrs(&addrList);

    std::map<std::string, NetworkInterfaceInfo> resultMap;
    for(ifaddrs* thisAddr = addrList; thisAddr != nullptr; thisAddr = thisAddr->ifa_next)
    {
        if(thisAddr->ifa_addr->sa_family != AF_PACKET && physicalAdapterNames.count(thisAddr->ifa_name) > 0)
        {
            if(resultMap.count(thisAddr->ifa_name) > 0)
            {
                resultMap.at(thisAddr->ifa_name).IPAddresses.emplace_back(getSockaddrInfo(thisAddr->ifa_addr, thisAddr->ifa_name));
            }
            else
            {
                NetworkInterfaceInfo newInfo = { thisAddr->ifa_name, { getSockaddrInfo(thisAddr->ifa_addr, thisAddr->ifa_name) } };
                resultMap.insert({ thisAddr->ifa_name,  newInfo });
            }
        }
    }

    freeifaddrs(addrList);

    std::vector<NetworkInterfaceInfo> resultVector;
    for(const auto& [_, value] : resultMap)
    {
        resultVector.push_back(value);
    }

    return resultVector;
}

std::vector<WebBrowserInfo> getInstalledWebBrowsers()
{
    std::vector<WebBrowserInfo> results;

    FILE* alternativesProc = nullptr;
    handleLinuxSystemError((alternativesProc = popen("update-alternatives --query x-www-browser", "r")) == nullptr);

    size_t currentLineBufSize, currentLineLength;
    char* currentLine;
    while((currentLineLength = getline(&currentLine, &currentLineBufSize, alternativesProc)) != -1)
    {
        std::cmatch matchResults;
        if(std::regex_match(currentLine, matchResults, std::regex("Alternative: (.*)\n")))
        {
            results.emplace_back(WebBrowserInfo { matchResults[1].str(), matchResults[1].str(), matchResults[1].str() + " \"%1\"" } );
        }
    }

    return results;
}

InstallationInfo InstallationInfo::detectInstallation()
{
	wxFileName thisFolder = getAppExecutablePath();
	thisFolder.SetName("");

	wxString staticEnvVar;
	bool staticEnvVarSet = wxGetEnv(wxT("QUICKOPEN_STATIC_DIR"), &staticEnvVar);

	if (thisFolder.GetDirs().Last() == wxT("bin"))
	{
		return { INSTALLED_PACKAGE, thisFolder, thisFolder / wxFileName("../etc/QuickOpen", ""),
			staticEnvVarSet ? wxFileName(staticEnvVar, "") : (thisFolder / wxFileName("../share/QuickOpen", "")) };
	}
	else
	{
		return { NOT_INSTALLED, thisFolder, thisFolder,
			staticEnvVarSet ? wxFileName(staticEnvVar, "") : thisFolder };
	}
}