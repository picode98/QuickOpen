//
// Created by sdk on 8/30/21.
//

#ifndef QUICKOPEN_LINUXUTILS_H
#define QUICKOPEN_LINUXUTILS_H

#include <wx/window.h>
#include <wx/filename.h>

#include <fstream>
#include <filesystem>

class LinuxException : public std::system_error
{
public:
    LinuxException(int errorCode, const std::string& description): system_error(errorCode, std::system_category())
    {}
};

void handleLinuxSystemError(bool errCond);

// void startSubprocess(const wxString& executable, const std::vector<wxString>& args);
void startSubprocess(const wxString& commandLine);

std::filesystem::path getAppExecutablePath();
void shellExecuteFile(const wxFileName& filePath, const wxWindow* window = nullptr);
void openExplorerFolder(const wxFileName& folder, const wxFileName* selectedFile = nullptr);

struct WebBrowserInfo
{
    std::string browserID, browserName,
    browserExecCommandFormat;
};

std::vector<WebBrowserInfo> getInstalledWebBrowsers();

inline void activateWindow(wxWindow* window)
{
    window->SetFocus();
}

inline wxString getDefaultBrowserCommandLine()
{
    return "x-www-browser \"%1\"";
}

inline wxString getBrowserCommandLine(const wxString& browserID)
{
    // Browser IDs are the browser's full path on Linux, so wrapping them in quotes yields the command line
    // to launch the browser.
    return wxT("\"") + browserID + wxT("\"");
}

template<typename T>
T generateCryptoRandomInteger()
{
    std::ifstream urandomInput;
    urandomInput.exceptions(std::ifstream::failbit);
    urandomInput.open("/dev/urandom", std::ifstream::binary);

    char inputBytes[sizeof(T)];
    urandomInput.read(inputBytes, sizeof(T));
    urandomInput.close();
    return *reinterpret_cast<T*>(inputBytes);
}

#endif //QUICKOPEN_LINUXUTILS_H
