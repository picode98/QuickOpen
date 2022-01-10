//
// Created by sdk on 1/4/22.
//

#include "AppGUI.h"

class QuickOpenApplication : public IQuickOpenApplication
{
    std::shared_ptr<WriterReadersLock<AppConfig>> configRef;
    QuickOpenTaskbarIcon* icon = nullptr;
    std::unique_ptr<QuickOpenWebServer> server;
    std::unique_ptr<ManagementServer> mgmtServer;

    std::unique_ptr<wxSingleInstanceChecker> singleInstanceChecker;

    std::optional<wxFileName> cliNewDefaultUploadFolder;
    // QuickOpenSettings* settingsWindow = nullptr;

    //class MainWindow : public wxFrame
    //{
    //	QuickOpenApplication& wxAppRef;
    //public:
    //	MainWindow(QuickOpenApplication& wxAppRef): wxFrame(nullptr, wxID_ANY, wxT("Main Window")), wxAppRef(wxAppRef)
    //	{}

    //	WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam) override
    //	{
    //		if(nMsg == winGetConfigUpdateMessageID())
    //		{
    //			wxAppRef.triggerConfigUpdate();
    //		}

    //		return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
    //	}
    //};

    //MainWindow* mainWindow = nullptr;
    static const std::vector<wxCmdLineEntryDesc> COMMAND_LINE_DESC;
public:
    void triggerConfigUpdate() override;

    bool OnInit() override;

    void OnInitCmdLine(wxCmdLineParser& parser) override;

    bool OnCmdLineParsed(wxCmdLineParser& parser) override;

    //void setConfigRef(WriterReadersLock<AppConfig>& configRef)
    //{
    //	this->configRef = &configRef;
    //}

    TrayStatusWindow* getTrayWindow() const override
    {
        return this->icon->getTrayWindow();
    }

    QuickOpenTaskbarIcon* getTrayIcon() const override
    {
        return this->icon;
    }

    void notifyUser(MessageSeverity severity, const wxString& title, const wxString& text) override;

    void setupServer(unsigned newPort) override;

    bool promptForWebpageOpen(std::string URL, const wxString& requesterName, bool& banRequested) override;

    std::pair<ConsentDialog::ResultCode, bool> promptForFileSave(const wxFileName& defaultDestDir, const wxString& requesterName, FileConsentRequestInfo& rqFileInfo) override;

    std::shared_ptr<WriterReadersLock<AppConfig>> getConfigRef() override
    {
        return configRef;
    }

    int OnExit() override;
};

const std::vector<wxCmdLineEntryDesc> QuickOpenApplication::COMMAND_LINE_DESC = {
        { wxCMD_LINE_OPTION, nullptr, "set-default-upload-folder", "Updates the default upload folder", wxCMD_LINE_VAL_STRING, 0 },
        { wxCMD_LINE_NONE }
};

void QuickOpenApplication::triggerConfigUpdate()
{
    unsigned newPort;
    {
        WriterReadersLock<AppConfig>::WritableReference configLock(*configRef);
        *configLock = AppConfig::loadConfig();
        newPort = configLock->serverPort;
    }

    if(newPort != server->getPort())
    {
        setupServer(newPort);
    }
}

bool QuickOpenApplication::OnInit()
{
    singleInstanceChecker = std::make_unique<wxSingleInstanceChecker>(wxT("Single Instance Checker"),
                                                                      InstallationInfo::detectInstallation().configFolder.GetFullPath());

    wxImage::AddHandler(new wxICOHandler());

    if (!wxApp::OnInit()) return false;
    // mainWindow = new MainWindow(*this);

    this->SetAppName(wxT("QuickOpen"));
    this->SetAppDisplayName(wxT("QuickOpen"));

    std::unique_ptr<AppConfig> config = std::make_unique<AppConfig>();

    try
    {
        *config = AppConfig::loadConfig();
    }
    catch (const std::ios_base::failure&)
    {
        std::cerr << "WARNING: Could not open configuration file \"" << AppConfig::defaultConfigPath().GetFullPath() << "\"." << std::endl;
    }

    if(cliNewDefaultUploadFolder.has_value())
    {
        config->fileSavePath = cliNewDefaultUploadFolder.value();
        config->saveConfig();
    }

    mg_init_library(0);

    if (singleInstanceChecker->IsAnotherRunning())
    {
        ManagementClient::sendReload();
        return false;
    }

    configRef = std::make_shared<WriterReadersLock<AppConfig>>(config);

    setupServer(configRef->obj->serverPort);

    unsigned attempts = 0;
    while (true)
    {
        try
        {
            mgmtServer = std::make_unique<ManagementServer>(*this, generateCryptoRandomInteger<uint16_t>() % 32768 + 32768);
            break;
        }
        catch (const CivetException& ex)
        {
            ++attempts;

            if (std::string(ex.what()).find("binding to port") == std::string::npos)
            {
                throw;
            }
            else if(attempts >= 100)
            {
                throw std::runtime_error("Could not find free port to bind management server");
            }
        }
    }

    assert(configRef != nullptr);
    this->icon = new QuickOpenTaskbarIcon(*this);
    // this->settingsWindow->Show();
    return true;
}

void QuickOpenApplication::OnInitCmdLine(wxCmdLineParser& parser)
{
    wxApp::OnInitCmdLine(parser);

    parser.SetDesc(COMMAND_LINE_DESC.data());
}

bool QuickOpenApplication::OnCmdLineParsed(wxCmdLineParser& parser)
{
    wxApp::OnCmdLineParsed(parser);

    wxString uploadFolder;
    if (parser.Found(wxT("set-default-upload-folder"), &uploadFolder))
    {
        if (uploadFolder.EndsWith(">")) uploadFolder.RemoveLast(1); // Allow a trailing > to prevent issues with quoting file paths that end with
        // a backslash (e.g. "C:\" escapes the ending quote).
        this->cliNewDefaultUploadFolder = wxFileName(uploadFolder, "");
    }

    return true;
}

void QuickOpenApplication::notifyUser(MessageSeverity severity, const wxString& title, const wxString& text)
{
    bool notificationShown = false;
#if wxUSE_TASKBARICON_BALLOONS
    notificationShown = this->icon->ShowBalloon(title, text, 0, wxICON_INFORMATION);
#endif

    if(!notificationShown)
    {
        auto notifyWindow = new NotificationWindow(nullptr, severity, text, title, [this]
        {
            this->getTrayWindow()->showAtCursor();
        });
        notifyWindow->Show();
        notifyWindow->RequestUserAttention();
    }
}

void QuickOpenApplication::setupServer(unsigned newPort)
{
    server = std::make_unique<QuickOpenWebServer>(*this, newPort);
}

bool QuickOpenApplication::promptForWebpageOpen(std::string URL, const wxString& requesterName, bool& banRequested)
{
    auto messageBoxLambda = [URL, &requesterName, &banRequested] {
        auto dialog = WebpageOpenConsentDialog(URL, requesterName);
        dialog.RequestUserAttention();
        auto result = static_cast<ConsentDialog::ResultCode>(dialog.ShowModal());
        banRequested = dialog.denyFutureRequestsRequested();
        return result;
        // return wxMessageBox(wxT("Do you want to open the following webpage?\n") + wxString::FromUTF8(URL), wxT("Webpage Open Request"), wxYES_NO);
    };

    auto response = wxCallAfterSync<QuickOpenApplication, decltype(messageBoxLambda), ConsentDialog::ResultCode>(*this, messageBoxLambda);
    return (response == ConsentDialog::ACCEPT);
}

std::pair<ConsentDialog::ResultCode, bool> QuickOpenApplication::promptForFileSave(const wxFileName& defaultDestDir,
                                                                                   const wxString& requesterName, FileConsentRequestInfo& rqFileInfo)
{
    auto dlgLambda = [this, &defaultDestDir, &requesterName, &rqFileInfo]
    {
        auto consentDlg = FileOpenSaveConsentDialog(defaultDestDir, rqFileInfo, this->configRef, requesterName);
        consentDlg.Show();
        consentDlg.RequestUserAttention();
        auto resultVal = static_cast<FileOpenSaveConsentDialog::ResultCode>(consentDlg.ShowModal());
        std::vector<wxFileName> consentedFileNames = consentDlg.getConsentedFilenames();

        for (size_t i = 0; i < consentedFileNames.size(); ++i)
        {
            rqFileInfo.fileList[i].consentedFileName = consentedFileNames[i];
        }

        return std::pair{ resultVal, consentDlg.denyFutureRequestsRequested() };
    };

    if (wxIsMainThread())
    {
        return dlgLambda();
    }
    else
    {
        return wxCallAfterSync<decltype(*this), decltype(dlgLambda), std::pair<ConsentDialog::ResultCode, bool>>(*this, dlgLambda);
    }
}

int QuickOpenApplication::OnExit()
{
    if (this->icon != nullptr)
    {
        this->icon->RemoveIcon();
        this->icon->Destroy();
        this->icon = nullptr;
    }

    mg_exit_library();

    return 0;
}

wxIMPLEMENT_APP(QuickOpenApplication);