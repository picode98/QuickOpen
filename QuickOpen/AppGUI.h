#pragma once

#define UNICODE

#include <wx/wx.h>
//#include <wx/app.h>
//#include <wx/msgdlg.h>
//#include <wx/menu.h>
#include <wx/taskbar.h>
//#include <wx/frame.h>
//#include <wx/statbox.h>
//#include <wx/sizer.h>
//#include <wx/checkbox.h>
//#include <wx/choice.h>
//#include <wx/stattext.h>
//#include <wx/button.h>
//#include <wx/textctrl.h>
#include <wx/filepicker.h>
#include <wx/spinctrl.h>
#include <wx/richtooltip.h>
#include <wx/clipbrd.h>
#include <wx/statline.h>
#include <wx/aboutdlg.h>
#include <wx/snglinst.h>
#include <wx/cmdline.h>
//#include <wx/gauge.h>
// #include <wx/scrolwin.h>

#include <optional>
#include <iostream>

#include "TrayStatusWindow.h"
// #include "WebServer.h"
#include "AppConfig.h"
#include "GUIUtils.h"
#include "Utils.h"
#include <atomic>
#include <condition_variable>
#include "CivetWebIncludes.h"
#include <wx/url.h>

#include "ApplicationInfo.h"
#include "ManagementServer.h"
#include "PlatformUtils.h"
#include "WebServer.h"

class QuickOpenWebServer;
class QuickOpenApplication;
struct FileConsentRequestInfo;

template<typename T>
class LabeledWindow : wxWindow
{
	wxStaticText label;
	T control;
	
};

wxBoxSizer* makeLabeledSizer(wxWindow* control, const wxString& labelText, wxWindow* parent,
                             int spacing = DEFAULT_CONTROL_SPACING);

struct FilePathValidator : public wxValidator
{
	bool dirPath = false,
		absolutePath = true;
	wxFileName fileName;

	wxFileName getControlValue() const;

	void setControlValue(const wxFileName& path);
public:
	FilePathValidator(const wxFileName& fileName, bool dirPath = false, bool absolutePath = true) :
		fileName(fileName), dirPath(dirPath), absolutePath(absolutePath)
	{}

	bool Validate(wxWindow* parent) override;

	bool TransferFromWindow() override
	{
		fileName = getControlValue();
		return true;
	}

	bool TransferToWindow() override
	{
		setControlValue(fileName);
		return true;
	}

	wxObject* Clone() const override
	{
		return new FilePathValidator(*this);
	}
};

template<typename AppType, typename FuncType, typename ResultType>
ResultType wxCallAfterSync(AppType& app, FuncType func)
{
	std::optional<ResultType> resultVal;
	std::condition_variable resultFlag;
	std::mutex resultMutex;
	std::unique_lock<std::mutex> resultLock(resultMutex);

	app.CallAfter([&resultMutex, &resultFlag, &func, &resultVal]
	{
		ResultType result = func();
		std::lock_guard<std::mutex> resultSetLock(resultMutex);
		resultVal = result;
		resultFlag.notify_all();
	});

	while (!resultVal.has_value())
	{
		resultFlag.wait(resultLock);
	}

	return resultVal.value();
}

inline wxFileName getAppIconPath()
{
	return InstallationInfo::detectInstallation().dataFolder / wxFileName("./static", "favicon.ico");
}

class QuickOpenSettings : public wxFrame
{
	wxSpinCtrl* serverPortCtrl;

	// wxPanel* panel;
	wxStaticBox* systemGroup;
	wxStaticBoxSizer* systemGroupSizer;
	wxCheckBox* runAtStartupCheckbox;

	wxStaticBox* webpageOpenGroup;
	wxStaticBoxSizer* webpageOpenGroupSizer;
	wxChoice* browserSelection;
	int browserSelInstalledListStartIndex, browserSelCustomBrowserIndex;
	std::vector<WebBrowserInfo> installedBrowsers;
	wxBoxSizer* customBrowserSizer;
	wxTextCtrl* customBrowserCommandText;
	wxButton* customBrowserFileBrowseButton;

	wxStaticBox* fileOpenSaveGroup;
	wxStaticBoxSizer* fileOpenSaveGroupSizer;
	wxDirPickerCtrl* saveFolderPicker;
	wxCheckBox* saveUseLastFolderCheckbox;
	
	wxButton* cancelButton;
	wxButton* saveButton;

    QuickOpenApplication& appRef;

public:
	QuickOpenSettings(QuickOpenApplication& appRef);

	void OnSaveButton(wxCommandEvent& event);

	void OnCancelButton(wxCommandEvent& event);

	void updateCustomBrowserHidden();

	void OnBrowserSelectionChanged(wxCommandEvent& event);

	void OnCustomBrowserBrowseButtonClicked(wxCommandEvent& event);

	void OnSaveUseLastFolderCheckboxChecked(wxCommandEvent& event);

	void updateSaveFolderEnabledState();
};

class ConsentDialog : public wxDialog
{
protected:
	wxButton* acceptButton = nullptr;
	wxButton* rejectButton = nullptr;

	wxCheckBox* denyFutureRequestsCheckbox = nullptr;

	wxBoxSizer* topLevelSizer = nullptr;
	wxWindow* content = nullptr;

	virtual void OnAcceptClicked(wxCommandEvent& event)
	{
		this->EndModal(ACCEPT);
	}

	virtual void OnDeclineClicked(wxCommandEvent& event)
	{
		this->EndModal(DECLINE);
	}

	virtual void OnDenyFutureRequestsCheckboxChecked(wxCommandEvent& event)
	{
		this->acceptButton->Enable(!this->denyFutureRequestsCheckbox->IsChecked());
	}

	void setContent(wxWindow* newContent)
	{
		topLevelSizer->Replace(content, newContent);
		content->Destroy();
		content = newContent;
		this->Fit();
	}
public:
	ConsentDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxString& requesterName);

	bool denyFutureRequestsRequested() const
	{
		return this->denyFutureRequestsCheckbox->IsChecked();
	}

	enum ResultCode
	{
		ACCEPT,
		DECLINE
	};
};

class WebpageOpenConsentDialog : public ConsentDialog
{
	wxStaticText* headerText = nullptr;
public:
	WebpageOpenConsentDialog(const wxString& URL, const wxString& requesterName);
};

class FileOpenSaveConsentDialog : public ConsentDialog
{
	std::shared_ptr<WriterReadersLock<AppConfig>> configRef;

	wxFileName defaultDestinationFolder;
	wxFilePickerCtrl* destFilenameInput = nullptr;
	wxDirPickerCtrl* destFolderNameInput = nullptr;

	std::unique_ptr<FileConsentRequestInfo> requestInfo;

	bool multiFileLayout = false;

	virtual void OnAcceptClicked(wxCommandEvent& event);

public:
	FileOpenSaveConsentDialog(const wxFileName& defaultDestinationFolder, const FileConsentRequestInfo& requestInfo,
		std::shared_ptr<WriterReadersLock<AppConfig>> configRef, const wxString& requesterName);

	std::vector<wxFileName> getConsentedFilenames() const;
};

//wxBEGIN_EVENT_TABLE(TrayStatusWindow::ActivityList, wxScrolledWindow)
//	EVT_SCROLLWIN(TrayStatusWindow::ActivityList::OnScroll)
//wxEND_EVENT_TABLE()

class QuickOpenTaskbarIcon : public wxTaskBarIcon
{
    QuickOpenApplication& appRef;
	TrayStatusWindow* statusWindow = nullptr;
	
	class TaskbarMenu : public wxMenu
	{
        QuickOpenApplication& appRef;
        TrayStatusWindow* statusWindow = nullptr;
		
	public:
		enum MenuItems
		{
            STATUS,
			CLIENT_PAGE,
			SETTINGS,
            ABOUT,
			EXIT
		};

		TaskbarMenu(QuickOpenApplication& appRef, TrayStatusWindow* statusWindow);

        void OnStatusItemSelected(wxCommandEvent& event)
        {
            this->statusWindow->showAtCursor();
        }

		void OnClientPageItemSelected(wxCommandEvent& event);

		void OnSettingsItemSelected(wxCommandEvent& event)
		{
			(new QuickOpenSettings(this->appRef))->Show();
		}

        void OnAboutItemSelected(wxCommandEvent& event);

		void OnExitItemSelected(wxCommandEvent& event)
		{
			wxExit();
		}

		wxDECLARE_EVENT_TABLE();
	};

	wxMenu* CreatePopupMenu() override
	{
		return new TaskbarMenu(this->appRef, this->statusWindow);
	}

	//void focusTimerFunction(wxTimerEvent& timerEvent)
	//{
	//	// this->statusWindow->SetFocus();
	//	this->Unbind(wxEVT_TIMER, &QuickOpenTaskbarIcon::focusTimerFunction, this);
	//}

	void OnIconClick(wxTaskBarIconEvent& event)
	{
        this->statusWindow->showAtCursor();
		
		//this->Bind(wxEVT_TIMER, &QuickOpenTaskbarIcon::focusTimerFunction, this);
		//(new wxTimer(this))->StartOnce(1000);
	}

public:
	QuickOpenTaskbarIcon(QuickOpenApplication& appRef) : appRef(appRef)
	{
		this->SetIcon(wxIcon(getAppIconPath().GetFullPath(), wxBITMAP_TYPE_ICO), wxT("QuickOpen"));
		this->Bind(wxEVT_TASKBAR_LEFT_UP, &QuickOpenTaskbarIcon::OnIconClick, this);

		this->statusWindow = new TrayStatusWindow(appRef);
	}

	TrayStatusWindow* getTrayWindow() const
	{
		return this->statusWindow;
	}
};

class NotificationWindow : public wxGenericMessageDialog
{
	std::function<void()> moreInfoCallback;
	bool created = false;

public:
	NotificationWindow(wxWindow *parent, MessageSeverity severity, const wxString& message, const wxString& caption, std::function<void()> moreInfoCallback)
		: wxGenericMessageDialog(parent, message, caption, wxYES_NO | getMessageSeverityIconStyle(severity)), moreInfoCallback(moreInfoCallback)
	{
		this->SetIcon(wxIcon(getAppIconPath().GetFullPath(), wxBITMAP_TYPE_ICO));
		this->SetYesNoLabels(wxID_OK, wxT("More Information"));
	}

	bool Show(bool show = true) override
	{
		if (!created)
		{
			this->DoCreateMsgdialog();
			created = true;
		}

		return wxGenericMessageDialog::Show(show);
	}

private:
	void OnMoreInfoClicked(wxCommandEvent& event)
	{
		moreInfoCallback();
		this->Close();
	}

	void OnOKClicked(wxCommandEvent& event)
	{
		this->Close();
	}

	wxDECLARE_EVENT_TABLE();
};

class QuickOpenApplication : public wxApp
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
    void triggerConfigUpdate();

    bool OnInit() override;

    void OnInitCmdLine(wxCmdLineParser& parser) override;

    bool OnCmdLineParsed(wxCmdLineParser& parser) override;

    //void setConfigRef(WriterReadersLock<AppConfig>& configRef)
    //{
    //	this->configRef = &configRef;
    //}

    TrayStatusWindow* getTrayWindow() const
    {
        return this->icon->getTrayWindow();
    }

    QuickOpenTaskbarIcon* getTrayIcon() const
    {
        return this->icon;
    }

    void notifyUser(MessageSeverity severity, const wxString& title, const wxString& text);

    void setupServer(unsigned newPort);

    bool promptForWebpageOpen(std::string URL, const wxString& requesterName, bool& banRequested);

    std::pair<ConsentDialog::ResultCode, bool> promptForFileSave(const wxFileName& defaultDestDir, const wxString& requesterName, FileConsentRequestInfo& rqFileInfo);

    std::shared_ptr<WriterReadersLock<AppConfig>> getConfigRef()
    {
        return configRef;
    }

    int OnExit() override;
};