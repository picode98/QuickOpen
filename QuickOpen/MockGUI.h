//
// Created by sdk on 5/15/2023.
//

#ifndef QUICKOPEN_MOCKGUI_H
#define QUICKOPEN_MOCKGUI_H

#include <wx/string.h>

#include "AppConfig.h"

struct FileConsentRequestInfo;

template<typename AppType, typename FuncType, typename ResultType>
ResultType wxCallAfterSync(AppType& app, FuncType func)
{
    return func();
}

class ConsentDialog
{
public:
    enum ResultCode
    {
        ACCEPT,
        DECLINE
    };
};

class TrayStatusWindow
{
public:
    class FileUploadActivityEntry
    {
    public:
        bool completed = false, errored = false, cancelCompleted = false;
        double progress = 0.0;

        void setCompleted(bool completed)
        {
            completed = completed;
        }

        void setCancelCompleted()
        {
            cancelCompleted = true;
        }

        void setError(const std::exception* ex)
        {
            errored = true;
        }

        void setProgress(double progress)
        {
            progress = progress;
        }
    };

    std::vector<wxString> webpageActivities;

    void addWebpageOpenedActivity(const wxString& url)
    {
       webpageActivities.emplace_back(url);
    }

    FileUploadActivityEntry* addFileUploadActivity(const wxFileName& filename, std::atomic<bool>& cancelRequestFlag)
    {
        return new FileUploadActivityEntry();
    }
};

class QuickOpenApplication
{
    std::shared_ptr<WriterReadersLock<AppConfig>> configRef;
public:
    bool confirmPrompts = false, requestBan = false;

    QuickOpenApplication(bool confirmPrompts, bool requestBan): confirmPrompts(confirmPrompts), requestBan(requestBan),
                                                      configRef(std::make_shared<WriterReadersLock<AppConfig>>(std::make_unique<AppConfig>()))
    {}

    bool promptedForWebpage = false;
    struct
    {
        std::string URL;
        wxString requesterName;
    } webpagePromptInfo;

    void notifyUser(MessageSeverity severity, const wxString& title, const wxString& text) {}
    void setupServer(unsigned newPort) {}
    bool promptForWebpageOpen(std::string URL, const wxString& requesterName, bool& banRequested)
    {
        this->promptedForWebpage = true;
        this->webpagePromptInfo = { URL, requesterName };
        banRequested = requestBan;
        return confirmPrompts;
    }

    std::optional<wxFileName> fileDestFolder;
    bool promptedForFileSave = false;

    std::pair<ConsentDialog::ResultCode, bool> promptForFileSave(const wxFileName& defaultDestDir, const wxString& requesterName, FileConsentRequestInfo& rqFileInfo)
    {
        this->promptedForFileSave = true;
        return { (this->confirmPrompts ? ConsentDialog::ACCEPT : ConsentDialog::DECLINE), requestBan };
    }

    bool configUpdateTriggered = false;
    void triggerConfigUpdate()
    {
        configUpdateTriggered = true;
    }

    std::shared_ptr<WriterReadersLock<AppConfig>> getConfigRef()
    {
        return configRef;
    }

    template<typename T>
    void CallAfter(T&& callable)
    {
        callable();
    }

    TrayStatusWindow* getTrayWindow() const
    {
        return new TrayStatusWindow();
    }
};

#endif //QUICKOPEN_MOCKGUI_H
