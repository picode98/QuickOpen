//
// Created by sdk on 1/8/22.
//

#include "Utils.h"
#include "PlatformUtils.h"
#include "catch.hpp"

#include <wx/process.h>

#include <set>
#include <regex>

TEST_CASE("getAppExecutablePath function")
{
    wxFileName result = getAppExecutablePath();
    REQUIRE(result.FileExists());
    REQUIRE(result.IsFileExecutable());

#ifdef WIN32
    REQUIRE(result.GetExt() == wxT("exe"));
#else
    REQUIRE(result.GetExt() == wxEmptyString);
#endif
}

TEST_CASE("startSubprocess function")
{
    auto procPID = startSubprocess(wxT("sh"), { wxT("-c"), wxT("sleep 3") });
    REQUIRE(procPID > 0);
    REQUIRE(wxProcess::Exists(procPID));
}

TEST_CASE("getPhysicalNetworkInterfaces function")
{
    std::set<wxString> usedNames;
    for(const NetworkInterfaceInfo& thisInterface : getPhysicalNetworkInterfaces())
    {
        REQUIRE(thisInterface.interfaceName != wxEmptyString);
        REQUIRE(usedNames.count(thisInterface.interfaceName) == 0);
        usedNames.insert(thisInterface.interfaceName);

        for(const IPAddress& thisAddr : thisInterface.IPAddresses)
        {
            switch(thisAddr.type)
            {
                case IPAddress::IPV4:
                    REQUIRE(std::regex_match(thisAddr.addressStr.ToStdString(), std::regex("^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$")));
                    break;
                case IPAddress::IPV6:
                    REQUIRE(std::regex_match(thisAddr.addressStr.ToStdString(), std::regex("^([0-9a-f]{1,4}::?){0,7}[0-9a-f]{0,4}(%\\S+)?$")));
                    break;
            }
        }
    }
}

TEST_CASE("getInstalledWebBrowsers function")
{
    auto browserList = getInstalledWebBrowsers();

    for(const WebBrowserInfo& browser : browserList)
    {
        REQUIRE(!browser.browserID.empty());
        REQUIRE(!browser.browserExecCommandFormat.empty());
        REQUIRE(!browser.browserName.empty());

#ifdef __linux__
        // On Linux, browser IDs are the full path to the executable
        wxFileName browserPath(browser.browserID);
        REQUIRE(browserPath.FileExists());
        REQUIRE(browserPath.IsFileExecutable());

        // The path to the executable should appear in the command line
        REQUIRE(browser.browserExecCommandFormat.find(browser.browserID) != std::string::npos);
#endif
    }
}

TEST_CASE("InstallationInfo::detectInstallation function")
{
    InstallationInfo installInfo = InstallationInfo::detectInstallation();

    REQUIRE(installInfo.binaryFolder == getAppExecutablePath() / wxFileName(".", ""));

    REQUIRE(installInfo.binaryFolder.DirExists());
    REQUIRE(installInfo.binaryFolder.IsDirReadable());
    REQUIRE(installInfo.configFolder.DirExists());
    REQUIRE(installInfo.configFolder.IsDirReadable());
    REQUIRE(installInfo.configFolder.IsDirWritable());
    REQUIRE(installInfo.dataFolder.DirExists());
    REQUIRE(installInfo.dataFolder.IsDirReadable());
}