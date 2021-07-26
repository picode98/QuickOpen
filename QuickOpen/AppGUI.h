#pragma once

#define UNICODE

#include <wx/app.h>
#include <wx/msgdlg.h>
#include <wx/menu.h>
#include <wx/taskbar.h>
#include <wx/frame.h>
#include <wx/statbox.h>
#include <wx/sizer.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/filepicker.h>
#include <wx/spinctrl.h>

#include <iostream>

#include "AppConfig.h"
#include "Utils.h"

#ifdef WIN32
#include "WinUtils.h"
#endif

const int DEFAULT_CONTROL_SPACING = 7;

template<typename T>
class LabeledWindow : wxWindow
{
	wxStaticText label;
	T control;
	
};

wxBoxSizer* makeLabeledSizer(wxWindow* control, const wxString& labelText, wxWindow* parent, int spacing = DEFAULT_CONTROL_SPACING)
{
	auto sizer = new wxBoxSizer(wxHORIZONTAL);
	auto label = new wxStaticText(parent, wxID_ANY, labelText);
	sizer->Add(label, wxSizerFlags(0).Align(wxRIGHT).CenterVertical().Border(wxRIGHT, spacing));
	sizer->Add(control, wxSizerFlags(1).Expand());

	return sizer;
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
	
	wxButton* cancelButton;
	wxButton* saveButton;

	WriterReadersLock<AppConfig>& configRef;

public:
	QuickOpenSettings(WriterReadersLock<AppConfig>& configRef) : wxFrame(nullptr, wxID_ANY, wxT("Settings")), configRef(configRef) //: panel(this)
							//systemGroup(&this->panel, wxID_ANY, wxT("System")),
							//	runAtStartupCheckbox(&this->systemGroup, wxID_ANY, wxT("Run QuickOpen at startup"))
	{
		WriterReadersLock<AppConfig>::ReadableReference config(configRef);
		
		wxBoxSizer* windowPaddingSizer = new wxBoxSizer(wxVERTICAL);
		wxBoxSizer* topLevelSizer = new wxBoxSizer(wxVERTICAL);
		windowPaddingSizer->Add(topLevelSizer, wxSizerFlags(1).Expand().Border(wxALL, DEFAULT_CONTROL_SPACING));
		// this->panel = new wxPanel(this);
		// topLevelSizer->Add(panel, wxSizerFlags(1).Expand());

		// wxBoxSizer* panelSizer = new wxBoxSizer(wxHORIZONTAL);

		// this->systemGroup = new wxStaticBox(this, wxID_ANY, wxT("System"));
		// panelSizer->Add(systemGroup, wxSizerFlags(1).Expand());
		// panel->SetSizerAndFit(panelSizer);
		
		this->systemGroup = new wxStaticBox(this, wxID_ANY, wxT("System"));
		//(new wxBoxSizer(wxVERTICAL))->Add(systemGroup);
		// (new wxBoxSizer(wxVERTICAL))->Add(panel);
		// test->Add(systemGroup);
		// this->systemGroup->SetPosition({ 100, 50 });
		this->systemGroupSizer = new wxStaticBoxSizer(this->systemGroup, wxVERTICAL);
		topLevelSizer->Add(systemGroupSizer);

		topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
		
		this->runAtStartupCheckbox = new wxCheckBox(this, wxID_ANY, wxT("Run QuickOpen at startup"));
		this->runAtStartupCheckbox->SetValue(config->runAtStartup);
		this->systemGroupSizer->Add(runAtStartupCheckbox);

		this->webpageOpenGroup = new wxStaticBox(this, wxID_ANY, wxT("Opening Webpages"));
		this->webpageOpenGroupSizer = new wxStaticBoxSizer(this->webpageOpenGroup, wxVERTICAL);
		topLevelSizer->Add(webpageOpenGroupSizer, wxSizerFlags(0).Expand());

		auto browserChoices = wxArrayString();
		browserChoices.Add(wxT("System default web browser"));
		// browserChoices.Add(wxT("foo"));

		this->installedBrowsers = getInstalledWebBrowsers();

		this->browserSelInstalledListStartIndex = browserChoices.size();
		for(const auto& thisBrowser : installedBrowsers)
		{
			browserChoices.Add(thisBrowser.browserName);
		}

		browserSelCustomBrowserIndex = browserChoices.size();
		browserChoices.Add(wxT("Custom command..."));
		
		this->browserSelection = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, browserChoices);
		this->browserSelection->Bind(wxEVT_CHOICE, &QuickOpenSettings::OnBrowserSelectionChanged, this);

		if(config->browserID.empty())
		{
			if(config->customBrowserPath.empty())
			{
				this->browserSelection->SetSelection(0);
			}
			else
			{
				this->browserSelection->SetSelection(static_cast<int>(browserChoices.size()) - 1);
			}
		}
		else
		{
			auto browserIndex = this->installedBrowsers.end();

			for(auto thisBrowser = this->installedBrowsers.begin(); thisBrowser != this->installedBrowsers.end(); ++thisBrowser)
			{
				if(thisBrowser->browserID == config->browserID)
				{
					browserIndex = thisBrowser;
					break;
				}
			}

			if (browserIndex != this->installedBrowsers.end())
			{
				this->browserSelection->SetSelection(browserIndex - this->installedBrowsers.begin() + this->browserSelInstalledListStartIndex);
			}
			else
			{
				std::cerr << "WARNING: Browser ID \"" << config->browserID << "\" is not valid." << std::endl;
			}
		}

		webpageOpenGroupSizer->Add(makeLabeledSizer(browserSelection, wxT("Web browser for opening webpages:"), this));
		// panelSizer->Add(runAtStartupCheckbox, wxSizerFlags(1).Expand());
		//topLevelSizer->Add(runAtStartupCheckbox, 1);
		
		this->customBrowserCommandText = new wxTextCtrl(this, wxID_ANY);
		customBrowserCommandText->SetValue(config->customBrowserPath);
		this->customBrowserFileBrowseButton = new wxButton(this, wxID_ANY, wxT("Browse..."));
		customBrowserFileBrowseButton->Bind(wxEVT_BUTTON, &QuickOpenSettings::OnCustomBrowserBrowseButtonClicked, this);
		this->customBrowserSizer = new wxBoxSizer(wxHORIZONTAL);
		customBrowserSizer->Add(customBrowserCommandText, wxSizerFlags(1).Expand());
		customBrowserSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
		customBrowserSizer->Add(customBrowserFileBrowseButton);
		webpageOpenGroupSizer->Add(customBrowserSizer, wxSizerFlags(0).Expand().Border(wxTOP, DEFAULT_CONTROL_SPACING));

		topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

		this->fileOpenSaveGroup = new wxStaticBox(this, wxID_ANY, wxT("Opening/Saving Files"));
		this->fileOpenSaveGroupSizer = new wxStaticBoxSizer(fileOpenSaveGroup, wxVERTICAL);
		topLevelSizer->Add(fileOpenSaveGroupSizer, wxSizerFlags(0).Expand());

		fileOpenSaveGroupSizer->Add(
			makeLabeledSizer(new wxSpinCtrl(this, wxID_ANY, (wxString() << config->maxSaveFileSize), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 1024),
				wxT("Maximum size for uploaded files:"), this),
		wxSizerFlags(0).Expand());

		topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
		
		this->saveButton = new wxButton(this, wxID_OK);
		this->cancelButton = new wxButton(this, wxID_CANCEL);
		this->saveButton->Bind(wxEVT_BUTTON, &QuickOpenSettings::OnSaveButton, this);
		this->cancelButton->Bind(wxEVT_BUTTON, &QuickOpenSettings::OnCancelButton, this);
		auto bottomButtonSizer = new wxBoxSizer(wxHORIZONTAL);
		bottomButtonSizer->AddStretchSpacer(1);
		bottomButtonSizer->Add(saveButton, wxSizerFlags(0));
		bottomButtonSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
		bottomButtonSizer->Add(cancelButton, wxSizerFlags(0));
		
		topLevelSizer->AddStretchSpacer(1);
		topLevelSizer->Add(bottomButtonSizer, wxSizerFlags(0).Expand());

		
		this->SetSizerAndFit(windowPaddingSizer);
		this->updateCustomBrowserHidden();
		//this->runAtStartupCheckbox->SetSize(300, 50);
	}

	void OnSaveButton(wxCommandEvent& event)
	{
		WriterReadersLock<AppConfig>::WritableReference config(this->configRef);
		
		std::cout << "Saving settings." << std::endl;
		
		config->runAtStartup = runAtStartupCheckbox->IsChecked();

		int selectionIndex = this->browserSelection->GetSelection();

		bool selectionIsInstalledBrowser = selectionIndex >= this->browserSelInstalledListStartIndex
			&& selectionIndex < this->browserSelInstalledListStartIndex + this->installedBrowsers.size();
		
		config->browserID = selectionIsInstalledBrowser ? this->installedBrowsers[selectionIndex - this->browserSelInstalledListStartIndex].browserID : "";

		config->customBrowserPath = (selectionIndex == this->browserSelCustomBrowserIndex) ? this->customBrowserCommandText->GetValue().ToUTF8() : ""; // TODO
		
		
		config->saveConfig();

		this->Close();
	}

	void OnCancelButton(wxCommandEvent& event)
	{
		this->Close();
	}

	void updateCustomBrowserHidden()
	{
		if (browserSelection->GetSelection() == this->browserSelCustomBrowserIndex)
		{
			this->webpageOpenGroupSizer->Show(this->customBrowserSizer);
		}
		else
		{
			this->webpageOpenGroupSizer->Hide(this->customBrowserSizer);
		}

		this->Layout();
	}

	void OnBrowserSelectionChanged(wxCommandEvent& event)
	{
		this->updateCustomBrowserHidden();
	}

	void OnCustomBrowserBrowseButtonClicked(wxCommandEvent& event)
	{
		auto dialog = wxFileDialog(this, wxT("Select Browser Executable"), wxEmptyString, wxEmptyString,
			wxT("Browser Executables (*.exe)|*.exe"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);

		if(dialog.ShowModal() != wxID_CANCEL)
		{
			this->customBrowserCommandText->SetValue(wxT("\"") + dialog.GetPath() + wxT("\" \"$url\""));
		}
	}
};

class QuickOpenTaskbarIcon : public wxTaskBarIcon
{
	WriterReadersLock<AppConfig>& configRef;
	
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

public:
	QuickOpenTaskbarIcon(WriterReadersLock<AppConfig>& configRef) : configRef(configRef)
	{
		this->SetIcon(wxIcon());
	}
};

wxBEGIN_EVENT_TABLE(QuickOpenTaskbarIcon::TaskbarMenu, wxMenu)
	EVT_MENU(QuickOpenTaskbarIcon::TaskbarMenu::MenuItems::SETTINGS, QuickOpenTaskbarIcon::TaskbarMenu::OnSettingsItemSelected)
	EVT_MENU(QuickOpenTaskbarIcon::TaskbarMenu::MenuItems::EXIT, QuickOpenTaskbarIcon::TaskbarMenu::OnExitItemSelected)
wxEND_EVENT_TABLE()

class QuickOpenApplication : public wxApp
{
	static WriterReadersLock<AppConfig>* configRef;
	QuickOpenTaskbarIcon* icon = nullptr;
	// QuickOpenSettings* settingsWindow = nullptr;
public:
	bool OnInit() override
	{
		// Poco::URI testURI("foobar");

		// system("pause");
		// mg_exit_library();
		//

		assert(configRef != nullptr);
		this->icon = new QuickOpenTaskbarIcon(*configRef);
		// this->settingsWindow->Show();

		return true;
	}

	static void setConfigRef(WriterReadersLock<AppConfig>& configRef)
	{
		QuickOpenApplication::configRef = &configRef;
	}

	int OnExit() override
	{
		if (this->icon != nullptr)
		{
			this->icon->RemoveIcon();
		}

		return 0;
	}
};

WriterReadersLock<AppConfig>* QuickOpenApplication::configRef = nullptr;

bool promptForWebpageOpen(std::string URL)
{
	auto response = wxMessageBox(wxT("Do you want to open ") + wxString::FromUTF8(URL), wxT("Webpage Open Request"), wxYES_NO);
	return (response == wxYES);
}

wxIMPLEMENT_APP(QuickOpenApplication);