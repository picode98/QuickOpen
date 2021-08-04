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
//#include <wx/gauge.h>
// #include <wx/scrolwin.h>

#include <optional>
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

struct FilePathValidator : public wxValidator
{
	bool dirPath = false,
		absolutePath = true;
	wxFileName fileName;

	wxFileName getControlValue() const
	{
		auto* textCtrl = dynamic_cast<wxTextEntryBase*>(m_validatorWindow);

		if(textCtrl != nullptr)
		{
			if (dirPath)
			{
				wxFileName parsedPath;
				parsedPath.AssignDir(textCtrl->GetValue());
				return parsedPath;
			}
			else
			{
				return wxFileName(textCtrl->GetValue());
			}
		}

		
		auto* pickerCtrl = dynamic_cast<wxFilePickerCtrl*>(m_validatorWindow);

		if(pickerCtrl != nullptr && !dirPath)
		{
			return pickerCtrl->GetFileName();
		}

		auto* dirPickerCtrl = dynamic_cast<wxDirPickerCtrl*>(m_validatorWindow);

		if(dirPickerCtrl != nullptr && dirPath)
		{
			return dirPickerCtrl->GetDirName();
		}

		throw std::bad_cast();
	}

	void setControlValue(const wxFileName& path)
	{
		auto* textCtrl = dynamic_cast<wxTextEntryBase*>(m_validatorWindow);

		if (textCtrl != nullptr)
		{
			textCtrl->SetValue(fileName.GetFullPath());
			return;
		}

		auto* pickerCtrl = dynamic_cast<wxFilePickerCtrl*>(m_validatorWindow);

		if (pickerCtrl != nullptr && !dirPath)
		{
			pickerCtrl->SetFileName(path);
			return;
		}

		auto* dirPickerCtrl = dynamic_cast<wxDirPickerCtrl*>(m_validatorWindow);

		if (dirPickerCtrl != nullptr && dirPath)
		{
			dirPickerCtrl->SetDirName(path);
			return;
		}

		throw std::bad_cast();
	}
public:
	FilePathValidator(const wxFileName& fileName, bool dirPath = false, bool absolutePath = true) :
		fileName(fileName), dirPath(dirPath), absolutePath(absolutePath)
	{}
	
	bool Validate(wxWindow* parent) override
	{
		auto value = getControlValue();
		std::cout << "Validating " << value.GetFullPath() << std::endl;
		// bool isValid = value.IsOk() && (!value.GetFullPath().IsEmpty()) && (!absolutePath || value.IsAbsolute());

		if(value.IsOk() && (!value.GetFullPath().IsEmpty()))
		{
			if(absolutePath && !value.IsAbsolute())
			{
				auto* toolTip = new wxRichToolTip(wxT("Invalid path"), wxT("Please enter an absolute path."));
				toolTip->SetIcon(wxICON_ERROR);
				toolTip->ShowFor(m_validatorWindow);
				return false;
			}
			else
			{
				return true;
			}
		}
		else
		{
			auto* toolTip = new wxRichToolTip(wxT("Invalid path"), wxT("Please enter a valid path."));
			toolTip->SetIcon(wxICON_ERROR);
			toolTip->ShowFor(m_validatorWindow);
			return false;
		}
	}

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

		fileOpenSaveGroupSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

		fileOpenSaveGroupSizer->Add(
			makeLabeledSizer(saveFolderPicker = new wxDirPickerCtrl(this, wxID_ANY, wxEmptyString, wxT("Select Download Folder")),
				wxT("Folder for downloads:"), this),
			wxSizerFlags(0).Expand());
		// saveFolderPicker->SetDirName(config->fileSavePath);
		saveFolderPicker->SetValidator(FilePathValidator(config->fileSavePath, true));

		fileOpenSaveGroupSizer->AddSpacer(DEFAULT_CONTROL_SPACING);

		fileOpenSaveGroupSizer->Add(savePromptEachFileCheckbox = new wxCheckBox(this, wxID_ANY, wxT("Prompt for a save location for each file")));
		savePromptEachFileCheckbox->Bind(wxEVT_CHECKBOX, &QuickOpenSettings::OnSavePromptCheckboxChecked, this);
		savePromptEachFileCheckbox->SetValue(config->alwaysPromptSave);
		
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

		this->TransferDataToWindow();
		this->SetSizerAndFit(windowPaddingSizer);
		this->updateCustomBrowserHidden();
		this->updateSaveFolderEnabledState();
		//this->runAtStartupCheckbox->SetSize(300, 50);
	}

	void OnSaveButton(wxCommandEvent& event)
	{
		if (this->Validate() && this->TransferDataFromWindow())
		{
			WriterReadersLock<AppConfig>::WritableReference config(this->configRef);

			std::cout << "Saving settings." << std::endl;

			config->runAtStartup = runAtStartupCheckbox->IsChecked();

			int selectionIndex = this->browserSelection->GetSelection();

			bool selectionIsInstalledBrowser = selectionIndex >= this->browserSelInstalledListStartIndex
				&& selectionIndex < this->browserSelInstalledListStartIndex + this->installedBrowsers.size();

			config->browserID = selectionIsInstalledBrowser ? this->installedBrowsers[selectionIndex - this->browserSelInstalledListStartIndex].browserID : "";

			config->customBrowserPath = (selectionIndex == this->browserSelCustomBrowserIndex) ? this->customBrowserCommandText->GetValue().ToUTF8() : ""; // TODO

			config->fileSavePath = dynamic_cast<FilePathValidator*>(this->saveFolderPicker->GetValidator())->fileName;
			config->alwaysPromptSave = this->savePromptEachFileCheckbox->IsChecked();

			config->saveConfig();

			this->Close();
		}
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

	void OnSavePromptCheckboxChecked(wxCommandEvent& event)
	{
		updateSaveFolderEnabledState();
	}

	void updateSaveFolderEnabledState()
	{
		if (savePromptEachFileCheckbox->GetValue())
		{
			saveFolderPicker->Disable();
		}
		else
		{
			saveFolderPicker->Enable();
		}
	}
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
	
	FileOpenSaveConsentDialog(const wxFileName& defaultDestination, unsigned long long fileSize) : wxDialog(nullptr, wxID_ANY, wxT("Receiving File"),
		wxDefaultPosition, wxDefaultSize)
	{
		wxBoxSizer* windowPaddingSizer = new wxBoxSizer(wxVERTICAL);
		wxBoxSizer* topLevelSizer = new wxBoxSizer(wxVERTICAL);
		windowPaddingSizer->Add(topLevelSizer, wxSizerFlags(1).Expand().Border(wxALL, DEFAULT_CONTROL_SPACING));

		topLevelSizer->Add(new wxStaticText(this, wxID_ANY,
			wxString() << wxT("A user is sending the file \"") << defaultDestination.GetFullName() << wxT("\" (") << wxFileName::GetHumanReadableSize(fileSize) << wxT(").\n")
							<< wxT("Would you like to accept it?")
		));

		topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
		
		topLevelSizer->Add(makeLabeledSizer(destFilenameInput =
			new wxFilePickerCtrl(this, wxID_ANY, wxEmptyString, wxT("Select Save Destination"),
				wxString() << wxT(".") << defaultDestination.GetExt() << wxT(" files|*.") << defaultDestination.GetExt() << wxT("|All files|*.*"), 
				wxDefaultPosition, wxDefaultSize, wxFLP_SAVE | wxFLP_USE_TEXTCTRL | wxFLP_OVERWRITE_PROMPT),
			wxT("Save file to:"), this), wxSizerFlags(0).Expand());

		destFilenameInput->SetValidator(FilePathValidator(defaultDestination));

		topLevelSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
		topLevelSizer->AddStretchSpacer(1);
		
		auto* bottomButtonSizer = new wxBoxSizer(wxHORIZONTAL);

		bottomButtonSizer->AddStretchSpacer(1);
		bottomButtonSizer->Add(acceptButton = new wxButton(this, wxID_ANY, wxT("Accept")));
		bottomButtonSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
		bottomButtonSizer->Add(rejectButton = new wxButton(this, wxID_ANY, wxT("Decline")));

		topLevelSizer->Add(bottomButtonSizer, wxSizerFlags(0).Expand());

		acceptButton->Bind(wxEVT_BUTTON, &FileOpenSaveConsentDialog::OnAcceptClicked, this);
		rejectButton->Bind(wxEVT_BUTTON, &FileOpenSaveConsentDialog::OnDeclineClicked, this);

		this->SetSizerAndFit(windowPaddingSizer);
	}

	void OnAcceptClicked(wxCommandEvent& event)
	{
		if(this->Validate() && this->TransferDataFromWindow())
		{
			this->EndModal(ACCEPT);
		}
		else
		{
			std::cout << "Validation failed." << std::endl;
		}
	}

	void OnDeclineClicked(wxCommandEvent& event)
	{
		this->EndModal(DECLINE);
	}

	wxFileName getConsentedFilename() const
	{
		return dynamic_cast<FilePathValidator*>(destFilenameInput->GetValidator())->fileName;
	}
};

class TrayStatusWindow : public wxFrame
{
public:
	class ActivityEntry : public wxBoxSizer
	{
	public:
		ActivityEntry(wxWindow* parent) : wxBoxSizer(wxVERTICAL)
		{}
	};

	class WebpageOpenedActivityEntry : public ActivityEntry
	{
		wxBoxSizer* horizSizer = nullptr;
		wxStaticText* entryText = nullptr;
		wxButton* copyURLButton = nullptr;
	public:
		WebpageOpenedActivityEntry(wxWindow* parent, const wxString& url) : ActivityEntry(parent)
		{
			horizSizer = new wxBoxSizer(wxHORIZONTAL);
			
			horizSizer->Add(entryText = new wxStaticText(parent, wxID_ANY,
				wxString() << wxT("Opened the URL \"") << url << wxT("\"."), wxDefaultPosition, wxDefaultSize), wxSizerFlags(1).CenterVertical());
			horizSizer->AddSpacer(DEFAULT_CONTROL_SPACING);
			horizSizer->Add(copyURLButton = new wxButton(parent, wxID_ANY, wxT("Copy URL")));
			this->Add(horizSizer, wxSizerFlags(1).Expand());

			// this->SetSizerAndFit(horizSizer);
		}
	};
	
	class FileUploadActivityEntry : public ActivityEntry
	{
		// wxBoxSizer* vertSizer = nullptr;
		wxStaticText* entryText = nullptr;
		wxGauge* fileUploadProgress = nullptr;
		wxBoxSizer* bottomButtonSizer = nullptr;
		wxButton* openButton = nullptr;
		wxButton* openFolderButton = nullptr;

		wxFileName filename;
		double uploadProgress = 0.0;
		bool uploadCompleted = false;

		wxString getEntryText() const
		{
			return wxString() << (this->uploadCompleted ? wxT("Uploaded \"") : wxT("Uploading \"")) << this->filename.GetFullPath() << wxT("\"")
				<< (this->uploadCompleted ? wxT(".") : wxT("..."));
		}

		void updateProgressBar()
		{
			this->entryText->SetLabel(getEntryText());
			
			if(this->uploadCompleted)
			{
				this->fileUploadProgress->SetValue(this->fileUploadProgress->GetRange());
			}
			else
			{
				this->fileUploadProgress->SetValue(static_cast<int>(this->uploadProgress * (this->fileUploadProgress->GetRange()) / 100.0));
			}
		}
	public:
		FileUploadActivityEntry(wxWindow* parent, const wxString& filename, bool uploadCompleted = false, double uploadProgress = 0.0) : ActivityEntry(parent),
		filename(filename), uploadCompleted(uploadCompleted), uploadProgress(uploadProgress)
		{
			// vertSizer = new wxBoxSizer(wxVERTICAL);
			this->Add(entryText = new wxStaticText(parent, wxID_ANY, this->getEntryText()), wxSizerFlags(0).Expand());
			this->Add(fileUploadProgress = new wxGauge(parent, wxID_ANY, 100));
			this->Add(bottomButtonSizer = new wxBoxSizer(wxHORIZONTAL));

			bottomButtonSizer->Add(openButton = new wxButton(parent, wxID_OPEN));
			bottomButtonSizer->Add(openFolderButton = new wxButton(parent, wxID_ANY, wxT("Open Folder")));

			// this->SetSizerAndFit(vertSizer);
		}

		void setProgress(double progress)
		{
			this->uploadProgress = progress;
			this->updateProgressBar();
		}
		
		double getProgress() const
		{
			return this->uploadProgress;
		}

		void setCompleted(bool completed)
		{
			this->uploadCompleted = completed;
			this->updateProgressBar();
		}

		bool getCompleted() const
		{
			return this->uploadCompleted;
		}
	};
	
	class ActivityList : public wxBoxSizer
	{
		wxWindow* parent = nullptr;
		// wxBoxSizer* topLevelSizer = nullptr;
		wxBoxSizer* noItemsElement = nullptr;
		wxBoxSizer* items = nullptr;

	public:
		ActivityList(wxWindow* parent) : wxBoxSizer(wxVERTICAL), parent(parent)
		{
			// topLevelSizer = new wxBoxSizer(wxVERTICAL);
			
			noItemsElement = new wxBoxSizer(wxVERTICAL);
			noItemsElement->Add(new wxStaticText(parent, wxID_ANY, wxT("No activity items")),
				wxSizerFlags(1).Expand().Border(wxALL, 10));
			this->Add(noItemsElement, wxSizerFlags(0).Expand());

			items = new wxBoxSizer(wxVERTICAL);
			this->Add(items, wxSizerFlags(0).Expand());
			this->Hide(items, true);

			// this->SetSizerAndFit(topLevelSizer);
		}

		void addActivity(ActivityEntry* entry)
		{
			// this->items->Add(new wxStaticText(parent, wxID_ANY, wxT("A test label.")), wxSizerFlags(0).Expand());
			this->items->Add(entry, wxSizerFlags(0).Expand().Border(wxBOTTOM, DEFAULT_CONTROL_SPACING));
			this->Hide(noItemsElement, true);
			this->Show(items, true, true);
			this->Layout();
		}

		void removeActivity(ActivityEntry* entry)
		{
			this->items->Detach(entry);
			if(this->items->GetItemCount() == 0)
			{
				this->Hide(items, true);
				this->Show(noItemsElement, true, true);
				this->Layout();
			}
		}
	};
private:
	wxBoxSizer* topLevelSizer = nullptr;
	ActivityList* activityList = nullptr;

public:
	TrayStatusWindow() : wxFrame(nullptr, wxID_ANY, wxT("QuickOpen Tray Status Window"), wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
	{
		topLevelSizer = new wxBoxSizer(wxVERTICAL);
		topLevelSizer->Add(activityList = new ActivityList(this), wxSizerFlags(1).Expand());

		this->SetSizerAndFit(topLevelSizer);
	}

	WebpageOpenedActivityEntry* addWebpageOpenedActivity(const wxString& url)
	{
		auto* newActivity = new WebpageOpenedActivityEntry(this, url);
		this->activityList->addActivity(newActivity);
		// this->topLevelSizer->Add(new wxStaticText(this, wxID_ANY, wxT("A test label.")), wxSizerFlags(0).Expand());
		this->Layout();
		this->Fit();
		
		return newActivity;
	}

	FileUploadActivityEntry* addFileUploadActivity(const wxFileName& filename)
	{
		auto* newActivity = new FileUploadActivityEntry(this, filename.GetFullPath());
		this->activityList->addActivity(newActivity);

		this->Layout();
		this->Fit();

		return newActivity;
	}
};

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

	void OnIconClick(wxTaskBarIconEvent& event)
	{
		this->statusWindow->SetPosition(wxGetMousePosition() - this->statusWindow->GetSize());
		this->statusWindow->Show();
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

wxBEGIN_EVENT_TABLE(QuickOpenTaskbarIcon::TaskbarMenu, wxMenu)
	EVT_MENU(QuickOpenTaskbarIcon::TaskbarMenu::MenuItems::SETTINGS, QuickOpenTaskbarIcon::TaskbarMenu::OnSettingsItemSelected)
	EVT_MENU(QuickOpenTaskbarIcon::TaskbarMenu::MenuItems::EXIT, QuickOpenTaskbarIcon::TaskbarMenu::OnExitItemSelected)
wxEND_EVENT_TABLE()

class QuickOpenApplication : public wxApp
{
	WriterReadersLock<AppConfig>* configRef = nullptr;
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

	void setConfigRef(WriterReadersLock<AppConfig>& configRef)
	{
		this->configRef = &configRef;
	}

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

		return 0;
	}
};

bool promptForWebpageOpen(std::string URL)
{
	auto response = wxMessageBox(wxT("Do you want to open ") + wxString::FromUTF8(URL), wxT("Webpage Open Request"), wxYES_NO);
	return (response == wxYES);
}

wxIMPLEMENT_APP(QuickOpenApplication);