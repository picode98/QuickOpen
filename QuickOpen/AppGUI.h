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

#ifdef WIN32
#include "WinUtils.h"
#endif

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
	wxCheckBox* savePromptEachFileCheckbox;
	
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

	void OnSavePromptCheckboxChecked(wxCommandEvent& event);

	void updateSaveFolderEnabledState();
};

class FileOpenSaveConsentDialog : public wxDialog
{
	wxButton* acceptButton = nullptr;
	wxButton* rejectButton = nullptr;
	wxFilePickerCtrl* destFilenameInput = nullptr;
public:
	enum ResultCode
	{
		ACCEPT,
		DECLINE
	};

	FileOpenSaveConsentDialog(const wxFileName& defaultDestination, unsigned long long fileSize);

	void OnAcceptClicked(wxCommandEvent& event);

	void OnDeclineClicked(wxCommandEvent& event);

	wxFileName getConsentedFilename() const;
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
		
	public:
		enum MenuItems
		{
			SETTINGS,
			EXIT
		};

		TaskbarMenu(WriterReadersLock<AppConfig>& configRef) : configRef(configRef)
		{
			this->Append(SETTINGS, wxT("Open Settings"));
			this->AppendSeparator();
			this->Append(EXIT, wxT("Exit QuickOpen"));
		}

		void OnSettingsItemSelected(wxCommandEvent& event)
		{
			(new QuickOpenSettings(this->configRef))->Show();
		}

		void OnExitItemSelected(wxCommandEvent& event)
		{
			wxExit();
		}

		wxDECLARE_EVENT_TABLE();
	};

	wxMenu* CreatePopupMenu() override
	{
		return new TaskbarMenu(this->configRef);
	}

	//void focusTimerFunction(wxTimerEvent& timerEvent)
	//{
	//	// this->statusWindow->SetFocus();
	//	this->Unbind(wxEVT_TIMER, &QuickOpenTaskbarIcon::focusTimerFunction, this);
	//}

	void OnIconClick(wxTaskBarIconEvent& event)
	{
		this->statusWindow->SetPosition(wxGetMousePosition() - this->statusWindow->GetSize());
		this->statusWindow->Show();
		SetForegroundWindow(this->statusWindow->GetHandle());
		
		//this->Bind(wxEVT_TIMER, &QuickOpenTaskbarIcon::focusTimerFunction, this);
		//(new wxTimer(this))->StartOnce(1000);
	}

public:
	QuickOpenTaskbarIcon(WriterReadersLock<AppConfig>& configRef) : configRef(configRef)
	{
		this->SetIcon(wxIcon());
		this->Bind(wxEVT_TASKBAR_LEFT_UP, &QuickOpenTaskbarIcon::OnIconClick, this);

		this->statusWindow = new TrayStatusWindow();
	}

	TrayStatusWindow* getTrayWindow() const
	{
		return this->statusWindow;
	}
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
		std::unique_ptr<AppConfig> config = std::make_unique<AppConfig>();

		try
		{
			*config = AppConfig::loadConfig();
		}
		catch (const std::ios_base::failure&)
		{
			std::cerr << "WARNING: Could not open configuration file \"" << AppConfig::DEFAULT_CONFIG_PATH << "\"." << std::endl;
		}
		
		configRef = std::make_shared<WriterReadersLock<AppConfig>>(config);
		server = std::make_unique<QuickOpenWebServer>(configRef, *this);
		
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

	int OnExit() override
	{
		if (this->icon != nullptr)
		{
			this->icon->RemoveIcon();
		}

		mg_exit_library();

		return 0;
	}
};

inline bool promptForWebpageOpen(std::string URL)
{
	auto response = wxMessageBox(wxT("Do you want to open the following webpage?\n") + wxString::FromUTF8(URL), wxT("Webpage Open Request"), wxYES_NO);
	return (response == wxYES);
}