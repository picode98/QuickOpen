//
// Created by sdk on 8/30/21.
//

#include "LinuxUtils.h"

#include <unistd.h>
#include <climits>

#include <regex>

void handleLinuxSystemError(bool errCond)
{
    if(errCond)
    {
        int errID = errno;
        throw LinuxException(errID, strerror(errID));
    }
}

std::filesystem::path getAppExecutablePath()
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

void shellExecuteFile(const wxFileName& filePath, const wxWindow* window)
{
    std::string filePathStr = static_cast<std::string>(filePath.GetFullPath().ToUTF8());
    const char* argv[2] = {  "xdg-open", filePathStr.c_str() };
    wxExecute(argv, wxEXEC_ASYNC);
//    if(fork() == 0)
//    {
//        std::string filePathStr = static_cast<std::string>(filePath.GetFullPath().ToUTF8());
//        char* argv[1] = { filePathStr.data() };
//        execvp("xdg-open", argv);
//        handleLinuxSystemError(true); // If this point is reached, something went wrong.
//    }
}

void openExplorerFolder(const wxFileName& folder, const wxFileName* selectedFile)
{
    shellExecuteFile(folder, nullptr);
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
        if(std::regex_match(currentLine, matchResults, std::regex("Alternative: (.*)")))
        {
            results.emplace_back(WebBrowserInfo { matchResults[1].str(), matchResults[1].str(), matchResults[1].str() + " \"%1\"" } );
        }
    }

    return results;
}