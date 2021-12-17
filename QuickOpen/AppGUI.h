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
//#include <wx/gauge.h>
// #include <wx/scrolwin.h>

#include <optional>
#include <iostream>

#include "TrayStatusWindow.h"
#include "WebServer.h"
#include "AppConfig.h"
#include "GUIUtils.h"
#include "Utils.h"
#include <atomic>
#include <CivetServer.h>
#include <wx/url.h>

#include "ApplicationInfo.h"
#include "PlatformUtils.h"

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
	return InstallationInfo::detectInstallation().dataFolder / wxFileName(".", "test_icon.ico");
}

class QuickOpenSettings : public wxFrame
{
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

	WriterReadersLock<AppConfig>& configRef;

public:
	QuickOpenSettings(WriterReadersLock<AppConfig>& configRef);

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
	ConsentDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxString& requesterName) : wxDialog(parent, id, title, wxDefaultPosition, wxDefaultSize,
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	{
		topLevelSizer = new wxBoxSizer(wxVERTICAL);
		topLevelSizer->Add(content = new wxWindow(this, wxID_ANY), wxSizerFlags(1).Expand());

		topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

		topLevelSizer->Add(denyFutureRequestsCheckbox = new wxCheckBox(this, wxID_ANY, wxT("Decline future requests from ") + requesterName + wxT(" (until QuickOpen is restarted)")),
			wxSizerFlags(0).Expand());
		denyFutureRequestsCheckbox->Bind(wxEVT_CHECKBOX, &ConsentDialog::OnDenyFutureRequestsCheckboxChecked, this);

		auto* bottomButtonSizer = new wxBoxSizer(wxHORIZONTAL);

		bottomButtonSizer->AddStretchSpacer(1);
		bottomButtonSizer->Add(acceptButton = new wxButton(this, wxID_ANY, wxT("Accept")));
		bottomButtonSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
		bottomButtonSizer->Add(rejectButton = new wxButton(this, wxID_ANY, wxT("Decline")));

		topLevelSizer->Add(bottomButtonSizer, wxSizerFlags(0).Expand());

		acceptButton->Bind(wxEVT_BUTTON, &ConsentDialog::OnAcceptClicked, this);
		rejectButton->Bind(wxEVT_BUTTON, &ConsentDialog::OnDeclineClicked, this);

		setSizerWithPadding(this, topLevelSizer);
		this->Fit();
	}

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
	WebpageOpenConsentDialog(const wxString& URL, const wxString& requesterName) :
		ConsentDialog(nullptr, wxID_ANY, wxT("Webpage Open Request"), requesterName)
	{
		wxWindow* contentWindow = new wxWindow(this, wxID_ANY);
		wxBoxSizer* contentSizer = new wxBoxSizer(wxVERTICAL);
		contentSizer->Add(headerText = new wxStaticText(contentWindow, wxID_ANY,
			wxString() << wxT("A user at " << requesterName << wxT(" is requesting that you open the following URL:\n\n") << URL
				<< wxT("\n\nWould you like to proceed?"))), wxSizerFlags(0).Expand());

		contentWindow->SetSizerAndFit(contentSizer);
		this->setContent(contentWindow);
	}
};

class FileOpenSaveConsentDialog : public ConsentDialog
{
	std::shared_ptr<WriterReadersLock<AppConfig>> configRef;

	wxFileName defaultDestinationFolder;
	wxFilePickerCtrl* destFilenameInput = nullptr;
	wxDirPickerCtrl* destFolderNameInput = nullptr;

	FileConsentRequestInfo requestInfo;

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
	WriterReadersLock<AppConfig>& configRef;
	TrayStatusWindow* statusWindow = nullptr;
	
	class TaskbarMenu : public wxMenu
	{
		WriterReadersLock<AppConfig>& configRef;
        TrayStatusWindow* statusWindow = nullptr;
		
	public:
		enum MenuItems
		{
            STATUS,
			SETTINGS,
            ABOUT,
			EXIT
		};

		TaskbarMenu(WriterReadersLock<AppConfig>& configRef, TrayStatusWindow* statusWindow) : configRef(configRef),
            statusWindow(statusWindow)
		{
            this->Append(STATUS, wxT("Open Status"));
			this->Append(SETTINGS, wxT("Open Settings"));
            this->Append(ABOUT, wxT("About QuickOpen"));
			this->AppendSeparator();
			this->Append(EXIT, wxT("Exit QuickOpen"));
		}

        void OnStatusItemSelected(wxCommandEvent& event)
        {
            this->statusWindow->showAtCursor();
        }

		void OnSettingsItemSelected(wxCommandEvent& event)
		{
			(new QuickOpenSettings(this->configRef))->Show();
		}

        void OnAboutItemSelected(wxCommandEvent& event)
        {
			wxString depStr = wxT("with additional credit to developers of the following dependencies:\n");

			auto depVersions = getDependencyVersions();
			for(auto item = depVersions.begin(); item != depVersions.end(); ++item)
			{
				auto nextItem = std::next(item);

				depStr += item->first + wxT(" (version ") + item->second + wxT(")");
				
				if(nextItem != depVersions.end())
				{
					if (std::next(nextItem) == depVersions.end())
					{
						depStr += wxT(", and ");
					}
					else
					{
						depStr += wxT(", ");
					}
				}
			}

            wxAboutDialogInfo aboutDialogInfo;
            aboutDialogInfo.AddDeveloper(wxT(QUICKOPEN_DEVELOPER));
			aboutDialogInfo.AddDeveloper(depStr);
			// aboutDialogInfo.AddDeveloper(wxT(""))
            aboutDialogInfo.SetVersion(wxString(wxT("Version ")) + wxT(QUICKOPEN_VERSION_STR));
			aboutDialogInfo.SetDescription(wxT(QUICKOPEN_SHORT_DESC));
			aboutDialogInfo.SetCopyright(wxT(QUICKOPEN_LICENSE));
			aboutDialogInfo.SetWebSite(wxT(QUICKOPEN_REPO_URL));
            aboutDialogInfo.SetIcon(wxIcon(getAppIconPath().GetFullPath(), wxBITMAP_TYPE_ICO));
			// aboutDialogInfo.GetDescriptionAndCredits()

            wxAboutBox(aboutDialogInfo);
        }

		void OnExitItemSelected(wxCommandEvent& event)
		{
			wxExit();
		}

		wxDECLARE_EVENT_TABLE();
	};

	wxMenu* CreatePopupMenu() override
	{
		return new TaskbarMenu(this->configRef, this->statusWindow);
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
	QuickOpenTaskbarIcon(WriterReadersLock<AppConfig>& configRef) : configRef(configRef)
	{
		this->SetIcon(wxIcon(getAppIconPath().GetFullPath(), wxBITMAP_TYPE_ICO), wxT("QuickOpen"));
		this->Bind(wxEVT_TASKBAR_LEFT_UP, &QuickOpenTaskbarIcon::OnIconClick, this);

		this->statusWindow = new TrayStatusWindow();
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
	// QuickOpenSettings* settingsWindow = nullptr;

public:
	bool OnInit() override
	{
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

		configRef = std::make_shared<WriterReadersLock<AppConfig>>(config);

        try {
            server = std::make_unique<QuickOpenWebServer>(configRef, *this);
        }
        catch(const std::exception& ex)
        {
            std::cout << ex.what() << std::endl;
        }
		
		assert(configRef != nullptr);
		this->icon = new QuickOpenTaskbarIcon(*configRef);
		// this->settingsWindow->Show();

		return true;
	}

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

	void notifyUser(MessageSeverity severity, const wxString& title, const wxString& text)
	{
		bool notificationShown = false;
#ifdef wxUSE_TASKBARICON_BALLOONS
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

	int OnExit() override
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
};

inline bool promptForWebpageOpen(QuickOpenApplication& app, std::string URL, const wxString& requesterName, bool& banRequested)
{
    auto messageBoxLambda = [URL, &requesterName, &banRequested]{
		auto dialog = WebpageOpenConsentDialog(URL, requesterName);
		dialog.RequestUserAttention();
		auto result = static_cast<ConsentDialog::ResultCode>(dialog.ShowModal());
		banRequested = dialog.denyFutureRequestsRequested();
		return result;
        // return wxMessageBox(wxT("Do you want to open the following webpage?\n") + wxString::FromUTF8(URL), wxT("Webpage Open Request"), wxYES_NO);
    };

    auto response = wxCallAfterSync<QuickOpenApplication, decltype(messageBoxLambda), ConsentDialog::ResultCode>(app, messageBoxLambda);
	return (response == ConsentDialog::ACCEPT);
}